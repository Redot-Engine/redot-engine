/**************************************************************************/
/*  flow_context_rd.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "flow_context_rd.h"

#ifdef GODOT_PHYSX_FLOW

#include "NvFlowContext.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_method_pointer.h"
#include "core/string/print_string.h"
#include "core/templates/hash_map.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/local_vector.h"
#include "core/version.h"
#include "servers/rendering/renderer_rd/shader_rd.h"
#include "servers/rendering/rendering_device.h"

#include <cstdarg>
#include <cstdio>

namespace FlowContextRD {

namespace {

// Pool resources nobody has used for this many frames are freed.
constexpr uint64_t POOL_LIFETIME = 16;

struct Buffer {
	uint64_t id = 0;
	NvFlowBufferDesc desc = {};
	NvFlowMemoryType memory = eNvFlowMemoryType_device;
	// Flow binds the same buffer as a constant buffer or a structured
	// buffer depending on its usage flags; RenderingDevice keeps those as
	// different kinds of buffer, so a buffer holds whichever it needs, kept
	// in step.
	RID storage;
	RID uniform;
	uint32_t storage_size = 0;
	uint32_t uniform_size = 0;
	// Upload: what Flow writes between map and unmap. Readback: the last
	// copy the GPU finished.
	Vector<uint8_t> shadow;
	// Upload: what the GPU copies hold (see unmapBuffer).
	Vector<uint8_t> uploaded;
	int refs = 0; // owners outside the per-frame pool
	bool pooled = false;
	uint64_t last_frame = 0;
	bool readback_pending = false;
};

struct BufferTransient {
	NvFlowBufferDesc desc = {};
	Buffer *buffer = nullptr;
};

struct BufferAcquire {
	BufferTransient *transient = nullptr;
	Buffer *buffer = nullptr;
};

struct Texture {
	NvFlowTextureDesc desc = {};
	RID rid;
	bool external = false; // Godot's; never freed here
	int refs = 0;
	bool pooled = false;
	uint64_t last_frame = 0;
};

struct TextureTransient {
	NvFlowTextureDesc desc = {};
	Texture *texture = nullptr;
};

struct TextureAcquire {
	TextureTransient *transient = nullptr;
	Texture *texture = nullptr;
};

struct Sampler {
	RID rid;
};

struct Pipeline {
	// Built on first use: Flow registers every pipeline it might need when
	// the grid is made, but a grid only ever runs a fraction of them -- and
	// one it doesn't run takes about a minute to build on a cold driver
	// cache. That one is EmitterNanoVdbCS (4.7 MB of SPIR-V), used only by
	// NanoVDB emitters: a node that adds those should build it in the
	// background first, or its first frame stalls that long on a new machine.
	Vector<uint8_t> spirv;
	bool built = false;
	RID shader;
	RID pipeline;
};

// RenderingDevice has no image-to-buffer copy, and Flow's readback path
// (addPassCopyTextureToBuffer -- the grid density flowing back to the CPU
// for queries) needs one: a tiny compute pass writes the texels into the
// destination as raw words instead. One shader per source image format
// (the image declaration carries the format), one pipeline per texel word
// count, compiled from GLSL on first use.
struct CopyTextureToBuffer {
	RID shader;
	RID pipelines[3] = {}; // indexed by words per texel: 1, 2 or 4
};

} // namespace

class Callbacks;

struct Context {
	RenderingDevice *rd = nullptr;
	uint64_t frame = 1;
	uint64_t completed = 0;
	uint64_t next_buffer_id = 1;

	LocalVector<Buffer *> buffers;
	LocalVector<Texture *> textures;
	HashMap<uint64_t, Buffer *> buffers_by_id;
	LocalVector<BufferTransient *> buffer_transients;
	LocalVector<TextureTransient *> texture_transients;
	LocalVector<BufferAcquire *> buffer_acquires;
	LocalVector<TextureAcquire *> texture_acquires;
	LocalVector<Buffer *> dead_buffers; // freed at the next flush
	LocalVector<Texture *> dead_textures;
	LocalVector<Sampler *> samplers;
	LocalVector<Pipeline *> pipelines;
	Sampler *default_sampler = nullptr;
	RID fence;
	Callbacks *callbacks = nullptr;
	Stats stats;
	int passes_this_frame = 0;
	CopyTextureToBuffer copy_tex_to_buf[3]; // rgba16f, rgba32f, r32f
};

// Async readback / frame-completion callbacks need an Object to bind to.
class Callbacks : public Object {
	GDCLASS(Callbacks, Object);

public:
	Context *context = nullptr;

	void on_readback(const Vector<uint8_t> &p_data, uint64_t p_buffer_id) {
		if (context == nullptr) {
			return;
		}
		Buffer **b = context->buffers_by_id.getptr(p_buffer_id);
		if (b == nullptr) {
			return; // destroyed meanwhile
		}
		Buffer *buffer = *b;
		const int n = MIN(p_data.size(), buffer->shadow.size());
		memcpy(buffer->shadow.ptrw(), p_data.ptr(), n);
	}

	void on_fence(const Vector<uint8_t> &p_data, uint64_t p_frame) {
		if (context != nullptr) {
			context->completed = MAX(context->completed, p_frame);
		}
	}
};

namespace {

Context *ctx(NvFlowContext *p_context) {
	return reinterpret_cast<Context *>(p_context);
}

void NV_FLOW_ABI log_print(NvFlowLogLevel p_level, const char *p_format, ...) {
	if (p_level == eNvFlowLogLevel_info) {
		return;
	}
	char buf[512];
	va_list args;
	va_start(args, p_format);
	vsnprintf(buf, sizeof(buf), p_format, args);
	va_end(args);
	if (p_level == eNvFlowLogLevel_error) {
		ERR_PRINT(vformat("Flow: %s", buf));
	} else {
		WARN_PRINT(vformat("Flow: %s", buf));
	}
}

RD::DataFormat to_rd_format(NvFlowFormat p_format) {
	switch (p_format) {
		case eNvFlowFormat_r32g32b32a32_float:
			return RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		case eNvFlowFormat_r32g32b32a32_uint:
			return RD::DATA_FORMAT_R32G32B32A32_UINT;
		case eNvFlowFormat_r32g32b32a32_sint:
			return RD::DATA_FORMAT_R32G32B32A32_SINT;
		case eNvFlowFormat_r32g32b32_float:
			return RD::DATA_FORMAT_R32G32B32_SFLOAT;
		case eNvFlowFormat_r32g32b32_uint:
			return RD::DATA_FORMAT_R32G32B32_UINT;
		case eNvFlowFormat_r32g32b32_sint:
			return RD::DATA_FORMAT_R32G32B32_SINT;
		case eNvFlowFormat_r16g16b16a16_float:
			return RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
		case eNvFlowFormat_r16g16b16a16_unorm:
			return RD::DATA_FORMAT_R16G16B16A16_UNORM;
		case eNvFlowFormat_r16g16b16a16_uint:
			return RD::DATA_FORMAT_R16G16B16A16_UINT;
		case eNvFlowFormat_r16g16b16a16_snorm:
			return RD::DATA_FORMAT_R16G16B16A16_SNORM;
		case eNvFlowFormat_r16g16b16a16_sint:
			return RD::DATA_FORMAT_R16G16B16A16_SINT;
		case eNvFlowFormat_r32g32_float:
			return RD::DATA_FORMAT_R32G32_SFLOAT;
		case eNvFlowFormat_r32g32_uint:
			return RD::DATA_FORMAT_R32G32_UINT;
		case eNvFlowFormat_r32g32_sint:
			return RD::DATA_FORMAT_R32G32_SINT;
		case eNvFlowFormat_r10g10b10a2_unorm:
			return RD::DATA_FORMAT_A2B10G10R10_UNORM_PACK32;
		case eNvFlowFormat_r10g10b10a2_uint:
			return RD::DATA_FORMAT_A2B10G10R10_UINT_PACK32;
		case eNvFlowFormat_r11g11b10_float:
			return RD::DATA_FORMAT_B10G11R11_UFLOAT_PACK32;
		case eNvFlowFormat_r8g8b8a8_unorm:
			return RD::DATA_FORMAT_R8G8B8A8_UNORM;
		case eNvFlowFormat_r8g8b8a8_unorm_srgb:
			return RD::DATA_FORMAT_R8G8B8A8_SRGB;
		case eNvFlowFormat_r8g8b8a8_uint:
			return RD::DATA_FORMAT_R8G8B8A8_UINT;
		case eNvFlowFormat_r8g8b8a8_snorm:
			return RD::DATA_FORMAT_R8G8B8A8_SNORM;
		case eNvFlowFormat_r8g8b8a8_sint:
			return RD::DATA_FORMAT_R8G8B8A8_SINT;
		case eNvFlowFormat_r16g16_float:
			return RD::DATA_FORMAT_R16G16_SFLOAT;
		case eNvFlowFormat_r16g16_unorm:
			return RD::DATA_FORMAT_R16G16_UNORM;
		case eNvFlowFormat_r16g16_uint:
			return RD::DATA_FORMAT_R16G16_UINT;
		case eNvFlowFormat_r16g16_snorm:
			return RD::DATA_FORMAT_R16G16_SNORM;
		case eNvFlowFormat_r16g16_sint:
			return RD::DATA_FORMAT_R16G16_SINT;
		case eNvFlowFormat_r32_float:
			return RD::DATA_FORMAT_R32_SFLOAT;
		case eNvFlowFormat_r32_uint:
			return RD::DATA_FORMAT_R32_UINT;
		case eNvFlowFormat_r32_sint:
			return RD::DATA_FORMAT_R32_SINT;
		case eNvFlowFormat_r8g8_unorm:
			return RD::DATA_FORMAT_R8G8_UNORM;
		case eNvFlowFormat_r8g8_uint:
			return RD::DATA_FORMAT_R8G8_UINT;
		case eNvFlowFormat_r8g8_snorm:
			return RD::DATA_FORMAT_R8G8_SNORM;
		case eNvFlowFormat_r8g8_sint:
			return RD::DATA_FORMAT_R8G8_SINT;
		case eNvFlowFormat_r16_float:
			return RD::DATA_FORMAT_R16_SFLOAT;
		case eNvFlowFormat_r16_unorm:
			return RD::DATA_FORMAT_R16_UNORM;
		case eNvFlowFormat_r16_uint:
			return RD::DATA_FORMAT_R16_UINT;
		case eNvFlowFormat_r16_snorm:
			return RD::DATA_FORMAT_R16_SNORM;
		case eNvFlowFormat_r16_sint:
			return RD::DATA_FORMAT_R16_SINT;
		case eNvFlowFormat_r8_unorm:
			return RD::DATA_FORMAT_R8_UNORM;
		case eNvFlowFormat_r8_uint:
			return RD::DATA_FORMAT_R8_UINT;
		case eNvFlowFormat_r8_snorm:
			return RD::DATA_FORMAT_R8_SNORM;
		case eNvFlowFormat_r8_sint:
			return RD::DATA_FORMAT_R8_SINT;
		case eNvFlowFormat_b8g8r8a8_unorm:
			return RD::DATA_FORMAT_B8G8R8A8_UNORM;
		case eNvFlowFormat_b8g8r8a8_unorm_srgb:
			return RD::DATA_FORMAT_B8G8R8A8_SRGB;
		default:
			return RD::DATA_FORMAT_MAX;
	}
}

uint32_t align_up(uint64_t p_v, uint32_t p_a) {
	return (uint32_t)((p_v + p_a - 1) / p_a * p_a);
}

bool buffer_desc_equal(const NvFlowBufferDesc &a, const NvFlowBufferDesc &b) {
	return a.usageFlags == b.usageFlags && a.format == b.format && a.structureStride == b.structureStride && a.sizeInBytes == b.sizeInBytes;
}

bool texture_desc_equal(const NvFlowTextureDesc &a, const NvFlowTextureDesc &b) {
	return a.textureType == b.textureType && a.usageFlags == b.usageFlags && a.format == b.format && a.width == b.width && a.height == b.height && a.depth == b.depth && a.mipLevels == b.mipLevels;
}

/* -------------------------------- buffers -------------------------------- */

Buffer *buffer_create(Context *c, NvFlowMemoryType p_memory, const NvFlowBufferDesc &p_desc) {
	Buffer *b = memnew(Buffer);
	b->id = c->next_buffer_id++;
	b->desc = p_desc;
	b->memory = p_memory;
	const bool wants_uniform = (p_desc.usageFlags & eNvFlowBufferUsage_constantBuffer) != 0;
	const bool wants_storage = !wants_uniform || (p_desc.usageFlags & ~(NvFlowBufferUsageFlags)eNvFlowBufferUsage_constantBuffer) != 0;
	if (wants_storage) {
		b->storage_size = MAX(4u, align_up(p_desc.sizeInBytes, 4));
		const BitField<RD::StorageBufferUsage> usage = (p_desc.usageFlags & eNvFlowBufferUsage_indirectBuffer) ? RD::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT : 0;
		b->storage = c->rd->storage_buffer_create(b->storage_size, {}, usage);
		c->rd->buffer_clear(b->storage, 0, b->storage_size);
	}
	if (wants_uniform) {
		b->uniform_size = MAX(16u, align_up(p_desc.sizeInBytes, 16));
		b->uniform = c->rd->uniform_buffer_create(b->uniform_size);
	}
	if (p_memory != eNvFlowMemoryType_device) {
		b->shadow.resize(MAX(b->storage_size, b->uniform_size));
		memset(b->shadow.ptrw(), 0, b->shadow.size());
	}
	c->buffers.push_back(b);
	c->buffers_by_id.insert(b->id, b);
	c->stats.buffers++;
	return b;
}

void buffer_free(Context *c, Buffer *b) {
	if (b->storage.is_valid()) {
		c->rd->free(b->storage);
	}
	if (b->uniform.is_valid()) {
		c->rd->free(b->uniform);
	}
	c->buffers_by_id.erase(b->id);
	c->buffers.erase(b);
	c->stats.buffers--;
	memdelete(b);
}

NvFlowBuffer *NV_FLOW_ABI createBuffer(NvFlowContext *p_context, NvFlowMemoryType p_memory, const NvFlowBufferDesc *p_desc) {
	Buffer *b = buffer_create(ctx(p_context), p_memory, *p_desc);
	b->refs = 1;
	return reinterpret_cast<NvFlowBuffer *>(b);
}

void NV_FLOW_ABI destroyBuffer(NvFlowContext *p_context, NvFlowBuffer *p_buffer) {
	Context *c = ctx(p_context);
	Buffer *b = reinterpret_cast<Buffer *>(p_buffer);
	if (--b->refs > 0) {
		return;
	}
	if (b->pooled) {
		return; // back to the pool
	}
	// Transients may still point at it this frame.
	c->dead_buffers.push_back(b);
}

NvFlowBufferTransient *NV_FLOW_ABI getBufferTransient(NvFlowContext *p_context, const NvFlowBufferDesc *p_desc) {
	Context *c = ctx(p_context);
	Buffer *found = nullptr;
	for (Buffer *b : c->buffers) {
		if (b->pooled && b->refs == 0 && b->last_frame != c->frame && buffer_desc_equal(b->desc, *p_desc)) {
			found = b;
			break;
		}
	}
	if (found == nullptr) {
		found = buffer_create(c, eNvFlowMemoryType_device, *p_desc);
		found->pooled = true;
	}
	found->last_frame = c->frame;
	BufferTransient *t = memnew(BufferTransient);
	t->desc = *p_desc;
	t->buffer = found;
	c->buffer_transients.push_back(t);
	return reinterpret_cast<NvFlowBufferTransient *>(t);
}

NvFlowBufferTransient *NV_FLOW_ABI registerBufferAsTransient(NvFlowContext *p_context, NvFlowBuffer *p_buffer) {
	Context *c = ctx(p_context);
	Buffer *b = reinterpret_cast<Buffer *>(p_buffer);
	b->last_frame = c->frame;
	BufferTransient *t = memnew(BufferTransient);
	t->desc = b->desc;
	t->buffer = b;
	c->buffer_transients.push_back(t);
	return reinterpret_cast<NvFlowBufferTransient *>(t);
}

NvFlowBufferTransient *NV_FLOW_ABI aliasBufferTransient(NvFlowContext *p_context, NvFlowBufferTransient *p_buffer, NvFlowFormat p_format, NvFlowUint p_stride) {
	// Format aliasing is reported unsupported; structured buffers don't
	// carry a format, so the same buffer serves.
	return p_buffer;
}

NvFlowBufferAcquire *NV_FLOW_ABI enqueueAcquireBuffer(NvFlowContext *p_context, NvFlowBufferTransient *p_buffer) {
	BufferAcquire *a = memnew(BufferAcquire);
	a->transient = reinterpret_cast<BufferTransient *>(p_buffer);
	ctx(p_context)->buffer_acquires.push_back(a);
	return reinterpret_cast<NvFlowBufferAcquire *>(a);
}

NvFlowBool32 NV_FLOW_ABI getAcquiredBuffer(NvFlowContext *p_context, NvFlowBufferAcquire *p_acquire, NvFlowBuffer **r_buffer) {
	Context *c = ctx(p_context);
	BufferAcquire *a = reinterpret_cast<BufferAcquire *>(p_acquire);
	if (a->buffer == nullptr) {
		return NV_FLOW_FALSE;
	}
	*r_buffer = reinterpret_cast<NvFlowBuffer *>(a->buffer);
	c->buffer_acquires.erase(a);
	memdelete(a);
	return NV_FLOW_TRUE;
}

void *NV_FLOW_ABI mapBuffer(NvFlowContext *p_context, NvFlowBuffer *p_buffer) {
	Buffer *b = reinterpret_cast<Buffer *>(p_buffer);
	if (b->shadow.is_empty()) {
		b->shadow.resize(MAX(b->storage_size, b->uniform_size));
	}
	return b->shadow.ptrw();
}

void NV_FLOW_ABI unmapBuffer(NvFlowContext *p_context, NvFlowBuffer *p_buffer) {
	Context *c = ctx(p_context);
	Buffer *b = reinterpret_cast<Buffer *>(p_buffer);
	if (b->memory == eNvFlowMemoryType_readback) {
		return;
	}
	// Flow's upload buffers are 64 KB at least but carry a few hundred bytes
	// a step: only what changed since the last upload is sent, which keeps a
	// few grids well inside RenderingDevice's staging memory (it stalls the
	// frame when that runs out).
	const uint32_t size = b->shadow.size();
	const uint8_t *src = b->shadow.ptr();
	uint32_t from = 0;
	uint32_t to = size;
	if (b->uploaded.size() == (int)size) {
		// 256 keeps the range aligned as buffer_update wants (4 bytes).
		constexpr uint32_t CHUNK = 256;
		const uint8_t *old = b->uploaded.ptr();
		from = size;
		to = 0;
		for (uint32_t o = 0; o < size; o += CHUNK) {
			const uint32_t n = MIN(CHUNK, size - o);
			if (memcmp(src + o, old + o, n) != 0) {
				from = MIN(from, o);
				to = o + n;
			}
		}
		if (from >= to) {
			return;
		}
	} else {
		// First upload: everything (a new uniform buffer's contents aren't
		// defined).
		b->uploaded.resize(size);
	}
	memcpy(b->uploaded.ptrw() + from, src + from, to - from);
	// Recorded in the command stream here, so it lands before any pass Flow
	// adds afterwards that reads it.
	if (b->storage.is_valid() && from < b->storage_size) {
		const uint32_t end = MIN(to, b->storage_size);
		c->rd->buffer_update(b->storage, from, end - from, src + from);
	}
	if (b->uniform.is_valid() && from < b->uniform_size) {
		const uint32_t end = MIN(to, b->uniform_size);
		c->rd->buffer_update(b->uniform, from, end - from, src + from);
	}
}

NvFlowBufferTransient *NV_FLOW_ABI getBufferTransientById(NvFlowContext *p_context, NvFlowUint64 p_id) {
	return nullptr;
}

void NV_FLOW_ABI getBufferExternalHandle(NvFlowContext *p_context, NvFlowBuffer *p_buffer, NvFlowInteropHandle *r_handle) {
	*r_handle = NvFlowInteropHandle_default;
}

void NV_FLOW_ABI closeBufferExternalHandle(NvFlowContext *p_context, NvFlowBuffer *p_buffer, const NvFlowInteropHandle *p_handle) {
}

NvFlowBuffer *NV_FLOW_ABI createBufferFromExternalHandle(NvFlowContext *p_context, const NvFlowBufferDesc *p_desc, const NvFlowInteropHandle *p_handle) {
	return nullptr;
}

/* -------------------------------- textures ------------------------------- */

Texture *texture_create(Context *c, const NvFlowTextureDesc &p_desc) {
	Texture *t = memnew(Texture);
	t->desc = p_desc;
	RD::TextureFormat tf;
	tf.texture_type = p_desc.textureType == eNvFlowTextureType_1d ? RD::TEXTURE_TYPE_1D : (p_desc.textureType == eNvFlowTextureType_3d ? RD::TEXTURE_TYPE_3D : RD::TEXTURE_TYPE_2D);
	tf.format = to_rd_format(p_desc.format);
	tf.width = MAX(1u, p_desc.width);
	tf.height = MAX(1u, p_desc.height);
	tf.depth = MAX(1u, p_desc.depth);
	tf.mipmaps = MAX(1u, p_desc.mipLevels);
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	if (p_desc.usageFlags & eNvFlowTextureUsage_rwTexture) {
		if (c->rd->texture_is_format_supported_for_usage(tf.format, tf.usage_bits | RD::TEXTURE_USAGE_STORAGE_BIT)) {
			tf.usage_bits |= RD::TEXTURE_USAGE_STORAGE_BIT;
		} else {
			ERR_PRINT(vformat("Flow: texture format %d can't be written by compute here.", (int)p_desc.format));
		}
	}
	if (tf.format == RD::DATA_FORMAT_MAX) {
		ERR_PRINT(vformat("Flow: unsupported texture format %d.", (int)p_desc.format));
		tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	}
	t->rid = c->rd->texture_create(tf, RD::TextureView());
	if (t->rid.is_valid()) {
		c->rd->texture_clear(t->rid, Color(0, 0, 0, 0), 0, tf.mipmaps, 0, 1);
	}
	c->textures.push_back(t);
	c->stats.textures++;
	return t;
}

void texture_free(Context *c, Texture *t) {
	if (t->rid.is_valid() && !t->external) {
		c->rd->free(t->rid);
	}
	c->textures.erase(t);
	c->stats.textures--;
	memdelete(t);
}

NvFlowTexture *NV_FLOW_ABI createTexture(NvFlowContext *p_context, const NvFlowTextureDesc *p_desc) {
	Texture *t = texture_create(ctx(p_context), *p_desc);
	t->refs = 1;
	return reinterpret_cast<NvFlowTexture *>(t);
}

void NV_FLOW_ABI destroyTexture(NvFlowContext *p_context, NvFlowTexture *p_texture) {
	Context *c = ctx(p_context);
	Texture *t = reinterpret_cast<Texture *>(p_texture);
	if (--t->refs > 0 || t->pooled) {
		return;
	}
	c->dead_textures.push_back(t);
}

NvFlowTextureTransient *NV_FLOW_ABI getTextureTransient(NvFlowContext *p_context, const NvFlowTextureDesc *p_desc) {
	Context *c = ctx(p_context);
	Texture *found = nullptr;
	for (Texture *t : c->textures) {
		if (t->pooled && t->refs == 0 && t->last_frame != c->frame && texture_desc_equal(t->desc, *p_desc)) {
			found = t;
			break;
		}
	}
	if (found == nullptr) {
		found = texture_create(c, *p_desc);
		found->pooled = true;
	}
	found->last_frame = c->frame;
	TextureTransient *tt = memnew(TextureTransient);
	tt->desc = *p_desc;
	tt->texture = found;
	c->texture_transients.push_back(tt);
	return reinterpret_cast<NvFlowTextureTransient *>(tt);
}

NvFlowTextureTransient *NV_FLOW_ABI registerTextureAsTransient(NvFlowContext *p_context, NvFlowTexture *p_texture) {
	Context *c = ctx(p_context);
	Texture *t = reinterpret_cast<Texture *>(p_texture);
	t->last_frame = c->frame;
	TextureTransient *tt = memnew(TextureTransient);
	tt->desc = t->desc;
	tt->texture = t;
	c->texture_transients.push_back(tt);
	return reinterpret_cast<NvFlowTextureTransient *>(tt);
}

NvFlowTextureTransient *NV_FLOW_ABI aliasTextureTransient(NvFlowContext *p_context, NvFlowTextureTransient *p_texture, NvFlowFormat p_format) {
	return p_texture; // aliasing reported unsupported; Flow doesn't ask
}

NvFlowTextureAcquire *NV_FLOW_ABI enqueueAcquireTexture(NvFlowContext *p_context, NvFlowTextureTransient *p_texture) {
	TextureAcquire *a = memnew(TextureAcquire);
	a->transient = reinterpret_cast<TextureTransient *>(p_texture);
	ctx(p_context)->texture_acquires.push_back(a);
	return reinterpret_cast<NvFlowTextureAcquire *>(a);
}

NvFlowBool32 NV_FLOW_ABI getAcquiredTexture(NvFlowContext *p_context, NvFlowTextureAcquire *p_acquire, NvFlowTexture **r_texture) {
	Context *c = ctx(p_context);
	TextureAcquire *a = reinterpret_cast<TextureAcquire *>(p_acquire);
	if (a->texture == nullptr) {
		return NV_FLOW_FALSE;
	}
	*r_texture = reinterpret_cast<NvFlowTexture *>(a->texture);
	c->texture_acquires.erase(a);
	memdelete(a);
	return NV_FLOW_TRUE;
}

NvFlowTextureTransient *NV_FLOW_ABI getTextureTransientById(NvFlowContext *p_context, NvFlowUint64 p_id) {
	return nullptr;
}

/* -------------------------------- samplers ------------------------------- */

RD::SamplerRepeatMode to_rd_repeat(NvFlowSamplerAddressMode p_mode) {
	switch (p_mode) {
		case eNvFlowSamplerAddressMode_wrap:
			return RD::SAMPLER_REPEAT_MODE_REPEAT;
		case eNvFlowSamplerAddressMode_mirror:
			return RD::SAMPLER_REPEAT_MODE_MIRRORED_REPEAT;
		case eNvFlowSamplerAddressMode_border:
			return RD::SAMPLER_REPEAT_MODE_CLAMP_TO_BORDER;
		default:
			return RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	}
}

Sampler *sampler_create(Context *c, const NvFlowSamplerDesc &p_desc) {
	RD::SamplerState s;
	const RD::SamplerFilter f = p_desc.filterMode == eNvFlowSamplerFilterMode_linear ? RD::SAMPLER_FILTER_LINEAR : RD::SAMPLER_FILTER_NEAREST;
	s.mag_filter = f;
	s.min_filter = f;
	s.mip_filter = f;
	s.repeat_u = to_rd_repeat(p_desc.addressModeU);
	s.repeat_v = to_rd_repeat(p_desc.addressModeV);
	s.repeat_w = to_rd_repeat(p_desc.addressModeW);
	s.border_color = RD::SAMPLER_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	Sampler *sm = memnew(Sampler);
	sm->rid = c->rd->sampler_create(s);
	c->samplers.push_back(sm);
	return sm;
}

NvFlowSampler *NV_FLOW_ABI createSampler(NvFlowContext *p_context, const NvFlowSamplerDesc *p_desc) {
	return reinterpret_cast<NvFlowSampler *>(sampler_create(ctx(p_context), *p_desc));
}

NvFlowSampler *NV_FLOW_ABI getDefaultSampler(NvFlowContext *p_context) {
	return reinterpret_cast<NvFlowSampler *>(ctx(p_context)->default_sampler);
}

void NV_FLOW_ABI destroySampler(NvFlowContext *p_context, NvFlowSampler *p_sampler) {
	Context *c = ctx(p_context);
	Sampler *s = reinterpret_cast<Sampler *>(p_sampler);
	if (s == c->default_sampler) {
		return;
	}
	c->rd->free(s->rid);
	c->samplers.erase(s);
	memdelete(s);
}

/* ------------------------------- pipelines ------------------------------- */

// Turning SPIR-V into a RenderingDevice shader (reflection, and on D3D12 the
// conversion to DXIL) costs ~15 s for Flow's 120 shaders on every launch, so
// the compiled result is kept in Godot's own shader cache folder (the
// editor's .godot/shader_cache, user://shader_cache in a running game; none
// when the project disables shader caching) -- per graphics API and engine
// build, keyed by the SPIR-V's hash.
RID shader_from_spirv_cached(Context *c, const Vector<uint8_t> &p_spirv) {
	const uint8_t *bytes = p_spirv.ptr();
	const int size = p_spirv.size();
	const uint64_t key = ((uint64_t)hash_murmur3_buffer(bytes, size, 0x464c4f57) << 32) | hash_murmur3_buffer(bytes, size, 0x524e4456);
	const String &cache_root = ShaderRD::get_shader_cache_user_dir();
	// Per engine version, not per build: RenderingDevice rejects a binary
	// whose container format it doesn't know, and that falls back to
	// compiling.
	const String dir = cache_root.is_empty() ? String() : cache_root.path_join("physx_flow").path_join(vformat("%s_%s", c->rd->get_device_api_name().validate_filename().to_lower(), String(REDOT_VERSION_NUMBER)));
	const String path = dir.is_empty() ? String() : dir.path_join(vformat("%016x.cache", key));
	if (!path.is_empty() && FileAccess::exists(path)) {
		const Vector<uint8_t> binary = FileAccess::get_file_as_bytes(path);
		const RID shader = binary.is_empty() ? RID() : c->rd->shader_create_from_bytecode(binary);
		if (shader.is_valid()) {
			return shader;
		}
	}
	RD::ShaderStageSPIRVData stage;
	stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	stage.spirv.resize(size);
	memcpy(stage.spirv.ptrw(), bytes, size);
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	const Vector<uint8_t> binary = c->rd->shader_compile_binary_from_spirv(stages, "NVIDIA Flow");
	if (binary.is_empty()) {
		return RID();
	}
	if (!dir.is_empty() && DirAccess::make_dir_recursive_absolute(dir) == OK) {
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_buffer(binary.ptr(), binary.size());
		}
	}
	return c->rd->shader_create_from_bytecode(binary);
}

NvFlowComputePipeline *NV_FLOW_ABI createComputePipeline(NvFlowContext *p_context, const NvFlowComputePipelineDesc *p_desc) {
	Context *c = ctx(p_context);
	Pipeline *p = memnew(Pipeline);
	p->spirv.resize(p_desc->bytecode.sizeInBytes);
	memcpy(p->spirv.ptrw(), p_desc->bytecode.data, p_desc->bytecode.sizeInBytes);
	c->pipelines.push_back(p);
	return reinterpret_cast<NvFlowComputePipeline *>(p);
}

void NV_FLOW_ABI destroyComputePipeline(NvFlowContext *p_context, NvFlowComputePipeline *p_pipeline) {
	Context *c = ctx(p_context);
	Pipeline *p = reinterpret_cast<Pipeline *>(p_pipeline);
	if (p->pipeline.is_valid()) {
		c->rd->free(p->pipeline);
		c->stats.pipelines--;
	}
	if (p->shader.is_valid()) {
		c->rd->free(p->shader);
	}
	c->pipelines.erase(p);
	memdelete(p);
}

/* --------------------------- copy-texture helper -------------------------- */

// One shader per destination texel size (4 / 8 / 16 bytes); the format
// suffix in p_image_format must match the source texture's, or
// uniform_set_create rejects the binding.
RID copy_tex_to_buf_shader(CopyTextureToBuffer &r_variant, RenderingDevice *p_rd, const char *p_image_format) {
	if (r_variant.shader.is_valid()) {
		return r_variant.shader;
	}
	// Raw GLSL: the stage is passed to shader_compile_spirv_from_source, so
	// no Godot "#[compute]" stage marker here (it's a parse error in raw
	// source).
	String src = R"(
#version 450

layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;

layout(set = 0, binding = 0, IMAGE_FORMAT) restrict readonly uniform image3D src_image;

layout(push_constant, std430) uniform Params {
	ivec3 src_offset;
	uint pad0;
	uvec3 extent;
	uint pad1;
	uint dst_offset_words;
	uint row_pitch_words;
	uint depth_pitch_words;
	uint words_per_texel;
} params;

layout(set = 0, binding = 1, std430) restrict writeonly buffer DstBuf {
	uint data[];
} dst;

void main() {
	uvec3 g = gl_GlobalInvocationID;
	if (g.x >= params.extent.x || g.y >= params.extent.y || g.z >= params.extent.z) {
		return;
	}
	vec4 v = imageLoad(src_image, params.src_offset + ivec3(g));
	uint base = params.dst_offset_words + g.z * params.depth_pitch_words + g.y * params.row_pitch_words + g.x * params.words_per_texel;
	if (params.words_per_texel == 2u) {
		dst.data[base] = packHalf2x16(v.xy);
		dst.data[base + 1u] = packHalf2x16(v.zw);
	} else if (params.words_per_texel == 4u) {
		dst.data[base + 0u] = floatBitsToUint(v.x);
		dst.data[base + 1u] = floatBitsToUint(v.y);
		dst.data[base + 2u] = floatBitsToUint(v.z);
		dst.data[base + 3u] = floatBitsToUint(v.w);
	} else {
		dst.data[base] = floatBitsToUint(v.x);
	}
}
)";
	src = src.replace("IMAGE_FORMAT", p_image_format);
	String err;
	const Vector<uint8_t> spirv = p_rd->shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, src, RD::SHADER_LANGUAGE_GLSL, &err);
	if (spirv.is_empty()) {
		ERR_PRINT("Flow: the texture-readback copy shader failed to compile: " + err);
		return RID();
	}
	RD::ShaderStageSPIRVData stage;
	stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	stage.spirv = spirv;
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	r_variant.shader = p_rd->shader_create_from_spirv(stages, "FlowCopyTexToBuf");
	return r_variant.shader;
}

RID copy_tex_to_buf_pipeline(Context *c, uint32_t p_words_per_texel, RD::DataFormat p_src_format, RID *r_shader) {
	static const char *formats[3] = { "rgba16f", "rgba32f", "r32f" };
	int variant_idx = -1;
	switch (p_src_format) {
		case RD::DATA_FORMAT_R16G16B16A16_SFLOAT:
			variant_idx = 0;
			break;
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT:
			variant_idx = 1;
			break;
		case RD::DATA_FORMAT_R32_SFLOAT:
			variant_idx = 2;
			break;
		default:
			break;
	}
	if (variant_idx < 0) {
		ERR_PRINT_ONCE("Flow: no readback copy shader for this texture format.");
		return RID();
	}
	int idx = -1;
	switch (p_words_per_texel) {
		case 1:
			idx = 0;
			break;
		case 2:
			idx = 1;
			break;
		case 4:
			idx = 2;
			break;
		default:
			return RID();
	}
	CopyTextureToBuffer &variant = c->copy_tex_to_buf[variant_idx];
	if (!variant.pipelines[idx].is_valid()) {
		const RID shader = copy_tex_to_buf_shader(variant, c->rd, formats[variant_idx]);
		if (!shader.is_valid()) {
			return RID();
		}
		variant.pipelines[idx] = c->rd->compute_pipeline_create(shader);
	}
	if (r_shader != nullptr) {
		*r_shader = variant.shader;
	}
	return variant.pipelines[idx];
}

/* --------------------------------- passes -------------------------------- */

void NV_FLOW_ABI addPassCompute(NvFlowContext *p_context, const NvFlowPassComputeParams *p_params) {
	Context *c = ctx(p_context);
	Pipeline *p = reinterpret_cast<Pipeline *>(p_params->pipeline);
	if (p != nullptr && !p->built) {
		p->built = true;
		p->shader = shader_from_spirv_cached(c, p->spirv);
		if (p->shader.is_valid()) {
			p->pipeline = c->rd->compute_pipeline_create(p->shader);
		}
		if (p->pipeline.is_valid()) {
			c->stats.pipelines++;
			p->spirv.clear();
		} else {
			c->stats.pipeline_failures++;
		}
	}
	if (p == nullptr || !p->pipeline.is_valid()) {
		return;
	}
	if (p_params->gridDim.x == 0 || p_params->gridDim.y == 0 || p_params->gridDim.z == 0) {
		return;
	}
	// Flow writes Vulkan set/binding pairs; one uniform set per Vulkan set.
	constexpr int MAX_SETS = 4;
	LocalVector<RD::Uniform> sets[MAX_SETS];
	for (NvFlowUint i = 0; i < p_params->numDescriptorWrites; i++) {
		const NvFlowDescriptorWrite &w = p_params->descriptorWrites[i];
		const NvFlowResource &r = p_params->resources[i];
		const uint32_t set = w.write.vulkan.set;
		ERR_CONTINUE(set >= (uint32_t)MAX_SETS);
		RD::Uniform u;
		u.binding = w.write.vulkan.binding;
		Buffer *buffer = r.bufferTransient ? reinterpret_cast<BufferTransient *>(r.bufferTransient)->buffer : nullptr;
		Texture *texture = r.textureTransient ? reinterpret_cast<TextureTransient *>(r.textureTransient)->texture : nullptr;
		Sampler *sampler = reinterpret_cast<Sampler *>(r.sampler);
		switch (w.type) {
			case eNvFlowDescriptorType_constantBuffer: {
				ERR_CONTINUE(buffer == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
				u.append_id(buffer->uniform);
			} break;
			case eNvFlowDescriptorType_structuredBuffer:
			case eNvFlowDescriptorType_rwStructuredBuffer: {
				ERR_CONTINUE(buffer == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.append_id(buffer->storage);
			} break;
			case eNvFlowDescriptorType_texture: {
				ERR_CONTINUE(texture == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.append_id(texture->rid);
			} break;
			case eNvFlowDescriptorType_rwTexture: {
				ERR_CONTINUE(texture == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.append_id(texture->rid);
			} break;
			case eNvFlowDescriptorType_sampler: {
				ERR_CONTINUE(sampler == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
				u.append_id(sampler->rid);
			} break;
			case eNvFlowDescriptorType_textureSampler: {
				ERR_CONTINUE(sampler == nullptr || texture == nullptr);
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
				u.append_id(sampler->rid);
				u.append_id(texture->rid);
			} break;
			default: {
				ERR_PRINT_ONCE(vformat("Flow: descriptor type %d isn't supported on RenderingDevice.", (int)w.type));
				return;
			}
		}
		sets[set].push_back(u);
	}
	RID uniform_sets[MAX_SETS];
	for (int s = 0; s < MAX_SETS; s++) {
		if (!sets[s].is_empty()) {
			uniform_sets[s] = c->rd->uniform_set_create(sets[s], p->shader, s);
			if (!uniform_sets[s].is_valid()) {
				ERR_PRINT_ONCE(vformat("Flow: couldn't bind pass '%s'.", p_params->debugLabel ? p_params->debugLabel : ""));
				for (int k = 0; k < s; k++) {
					if (uniform_sets[k].is_valid()) {
						c->rd->free(uniform_sets[k]);
					}
				}
				return;
			}
		}
	}
	const RD::ComputeListID list = c->rd->compute_list_begin();
	c->rd->compute_list_bind_compute_pipeline(list, p->pipeline);
	for (int s = 0; s < MAX_SETS; s++) {
		if (uniform_sets[s].is_valid()) {
			c->rd->compute_list_bind_uniform_set(list, uniform_sets[s], s);
		}
	}
	c->rd->compute_list_dispatch(list, p_params->gridDim.x, p_params->gridDim.y, p_params->gridDim.z);
	c->rd->compute_list_end();
	// Freed once the GPU is done with them (RenderingDevice defers it).
	for (int s = 0; s < MAX_SETS; s++) {
		if (uniform_sets[s].is_valid()) {
			c->rd->free(uniform_sets[s]);
		}
	}
	c->passes_this_frame++;
}

void NV_FLOW_ABI addPassCopyBuffer(NvFlowContext *p_context, const NvFlowPassCopyBufferParams *p_params) {
	Context *c = ctx(p_context);
	Buffer *src = reinterpret_cast<BufferTransient *>(p_params->src)->buffer;
	Buffer *dst = reinterpret_cast<BufferTransient *>(p_params->dst)->buffer;
	const RID from = src->storage.is_valid() ? src->storage : src->uniform;
	const uint32_t size = (uint32_t)p_params->numBytes;
	if (size == 0) {
		return;
	}
	if (dst->storage.is_valid()) {
		c->rd->buffer_copy(from, dst->storage, (uint32_t)p_params->srcOffset, (uint32_t)p_params->dstOffset, size);
	}
	if (dst->uniform.is_valid()) {
		c->rd->buffer_copy(from, dst->uniform, (uint32_t)p_params->srcOffset, (uint32_t)p_params->dstOffset, size);
	}
	if (dst->memory == eNvFlowMemoryType_readback) {
		dst->readback_pending = true;
	}
}

void NV_FLOW_ABI addPassCopyBufferToTexture(NvFlowContext *p_context, const NvFlowPassCopyBufferToTextureParams *p_params) {
	ERR_PRINT_ONCE("Flow: buffer-to-texture copies aren't implemented on RenderingDevice.");
}

// Flow's CPU-readback path copies the (sparse) grid density into a readback
// buffer through exactly this call; RenderingDevice has no image-to-buffer
// copy, so the pass is recorded as the internal copy shader writing the
// texels as words instead. The shadow fills when flush() schedules the
// buffer's async readback, gated by the frame fence like every other copy.
void NV_FLOW_ABI addPassCopyTextureToBuffer(NvFlowContext *p_context, const NvFlowPassCopyTextureToBufferParams *p_params) {
	Context *c = ctx(p_context);
	Texture *src = reinterpret_cast<TextureTransient *>(p_params->src)->texture;
	Buffer *dst = reinterpret_cast<BufferTransient *>(p_params->dst)->buffer;
	if (!src->rid.is_valid() || dst->storage.is_null()) {
		ERR_PRINT_ONCE("Flow: texture-to-buffer copy needs a storage-capable texture and buffer.");
		return;
	}
	const NvFlowUint3 &ext = p_params->textureExtent;
	if (ext.x == 0 || ext.y == 0 || ext.z == 0) {
		return;
	}
	uint32_t words_per_texel = 0;
	switch (src->desc.format) {
		case eNvFlowFormat_r16g16b16a16_float:
			words_per_texel = 2;
			break;
		case eNvFlowFormat_r32g32b32a32_float:
			words_per_texel = 4;
			break;
		case eNvFlowFormat_r32_float:
			words_per_texel = 1;
			break;
	default:
		ERR_PRINT_ONCE(vformat("Flow: readback copy doesn't support texture format %d.", (int)src->desc.format));
		return;
	}
	RID copy_shader;
	const RID pipeline = copy_tex_to_buf_pipeline(c, words_per_texel, to_rd_format(src->desc.format), &copy_shader);
	if (!pipeline.is_valid()) {
		return;
	}
	RD::Uniform src_u(RD::UNIFORM_TYPE_IMAGE, 0, src->rid);
	RD::Uniform dst_u(RD::UNIFORM_TYPE_STORAGE_BUFFER, 1, dst->storage);
	LocalVector<RD::Uniform> uniforms;
	uniforms.push_back(src_u);
	uniforms.push_back(dst_u);
	const RID set = c->rd->uniform_set_create(uniforms, copy_shader, 0);
	if (!set.is_valid()) {
		ERR_PRINT_ONCE("Flow: couldn't bind the texture-to-buffer copy pass.");
		return;
	}
	// Pitches in words; Vulkan copy rules keep 4-byte alignment throughout.
	const uint32_t row_words = (uint32_t)p_params->bufferRowPitch / 4;
	const uint32_t depth_words = (uint32_t)p_params->bufferDepthPitch / 4;
	uint32_t push[12] = {};
	push[0] = p_params->textureOffset.x;
	push[1] = p_params->textureOffset.y;
	push[2] = p_params->textureOffset.z;
	push[4] = ext.x;
	push[5] = ext.y;
	push[6] = ext.z;
	push[8] = (uint32_t)p_params->bufferOffset / 4;
	push[9] = row_words;
	push[10] = depth_words;
	push[11] = words_per_texel;
	const RD::ComputeListID list = c->rd->compute_list_begin();
	c->rd->compute_list_bind_compute_pipeline(list, pipeline);
	c->rd->compute_list_bind_uniform_set(list, set, 0);
	c->rd->compute_list_set_push_constant(list, push, sizeof(push));
	c->rd->compute_list_dispatch(list, (ext.x + 3) / 4, (ext.y + 3) / 4, ext.z);
	c->rd->compute_list_end();
	c->rd->free(set);
	c->passes_this_frame++;
	if (dst->memory == eNvFlowMemoryType_readback) {
		dst->readback_pending = true;
	}
}

void NV_FLOW_ABI addPassCopyTexture(NvFlowContext *p_context, const NvFlowPassCopyTextureParams *p_params) {
	Context *c = ctx(p_context);
	Texture *src = reinterpret_cast<TextureTransient *>(p_params->src)->texture;
	Texture *dst = reinterpret_cast<TextureTransient *>(p_params->dst)->texture;
	const NvFlowUint3 &so = p_params->srcOffset;
	const NvFlowUint3 &d = p_params->dstOffset;
	const NvFlowUint3 &e = p_params->extent;
	c->rd->texture_copy(src->rid, dst->rid, Vector3(so.x, so.y, so.z), Vector3(d.x, d.y, d.z), Vector3(e.x, e.y, e.z), p_params->srcMipLevel, p_params->dstMipLevel, 0, 0);
}

/* ------------------------------- the rest -------------------------------- */

void NV_FLOW_ABI getContextConfig(NvFlowContext *p_context, NvFlowContextConfig *r_config) {
	// Flow hands over its Vulkan bytecode (SPIR-V) and Vulkan bindings, which
	// is what RenderingDevice takes on every backend.
	r_config->api = eNvFlowContextApi_vulkan;
	r_config->textureBinding = eNvFlowTextureBindingType_separateSampler;
}

NvFlowBool32 NV_FLOW_ABI isFeatureSupported(NvFlowContext *p_context, NvFlowContextFeature p_feature) {
	return NV_FLOW_FALSE;
}

NvFlowUint64 NV_FLOW_ABI getCurrentFrame(NvFlowContext *p_context) {
	return ctx(p_context)->frame;
}

NvFlowUint64 NV_FLOW_ABI getLastFrameCompleted(NvFlowContext *p_context) {
	return ctx(p_context)->completed;
}

NvFlowLogPrint_t NV_FLOW_ABI getLogPrint(NvFlowContext *p_context) {
	return log_print;
}

void NV_FLOW_ABI executeTasks(NvFlowContext *p_context, NvFlowUint p_count, NvFlowUint p_granularity, NvFlowContextThreadPoolTask_t p_task, void *p_userdata) {
	for (NvFlowUint i = 0; i < p_count; i++) {
		p_task(i, 0, nullptr, p_userdata);
	}
}

} // namespace

Context *create(RenderingDevice *p_rd) {
	ERR_FAIL_NULL_V(p_rd, nullptr);
	Context *c = memnew(Context);
	c->rd = p_rd;
	c->callbacks = memnew(Callbacks);
	c->callbacks->context = c;
	NvFlowSamplerDesc desc = {};
	desc.addressModeU = eNvFlowSamplerAddressMode_border;
	desc.addressModeV = eNvFlowSamplerAddressMode_border;
	desc.addressModeW = eNvFlowSamplerAddressMode_border;
	desc.filterMode = eNvFlowSamplerFilterMode_linear;
	c->default_sampler = sampler_create(c, desc);
	c->fence = p_rd->storage_buffer_create(4);
	return c;
}

void destroy(Context *p_context) {
	if (p_context == nullptr) {
		return;
	}
	Context *c = p_context;
	c->callbacks->context = nullptr;
	for (BufferTransient *t : c->buffer_transients) {
		memdelete(t);
	}
	for (TextureTransient *t : c->texture_transients) {
		memdelete(t);
	}
	for (BufferAcquire *a : c->buffer_acquires) {
		memdelete(a);
	}
	for (TextureAcquire *a : c->texture_acquires) {
		memdelete(a);
	}
	while (!c->buffers.is_empty()) {
		buffer_free(c, c->buffers[c->buffers.size() - 1]);
	}
	while (!c->textures.is_empty()) {
		texture_free(c, c->textures[c->textures.size() - 1]);
	}
	for (Sampler *s : c->samplers) {
		c->rd->free(s->rid);
		memdelete(s);
	}
	for (Pipeline *p : c->pipelines) {
		if (p->pipeline.is_valid()) {
			c->rd->free(p->pipeline);
		}
		if (p->shader.is_valid()) {
			c->rd->free(p->shader);
		}
		memdelete(p);
	}
	for (int i = 0; i < 3; i++) {
		CopyTextureToBuffer &variant = c->copy_tex_to_buf[i];
		if (variant.shader.is_valid()) {
			c->rd->free(variant.shader);
		}
		for (int k = 0; k < 3; k++) {
			if (variant.pipelines[k].is_valid()) {
				c->rd->free(variant.pipelines[k]);
			}
		}
	}
	c->rd->free(c->fence);
	// In-flight async callbacks still hold the Callbacks object; it outlives
	// the context with a null context pointer and is reclaimed with them.
	memdelete(c->callbacks);
	memdelete(c);
}

NvFlowContextInterface *get_interface() {
	static NvFlowContextInterface iface = { NV_FLOW_REFLECT_INTERFACE_INIT(NvFlowContextInterface) };
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		iface.getContextConfig = getContextConfig;
		iface.isFeatureSupported = isFeatureSupported;
		iface.getCurrentFrame = getCurrentFrame;
		iface.getLastFrameCompleted = getLastFrameCompleted;
		iface.getCurrentGlobalFrame = getCurrentFrame;
		iface.getLastGlobalFrameCompleted = getLastFrameCompleted;
		iface.getLogPrint = getLogPrint;
		iface.executeTasks = executeTasks;
		iface.createBuffer = createBuffer;
		iface.destroyBuffer = destroyBuffer;
		iface.getBufferTransient = getBufferTransient;
		iface.registerBufferAsTransient = registerBufferAsTransient;
		iface.aliasBufferTransient = aliasBufferTransient;
		iface.enqueueAcquireBuffer = enqueueAcquireBuffer;
		iface.getAcquiredBuffer = getAcquiredBuffer;
		iface.mapBuffer = mapBuffer;
		iface.unmapBuffer = unmapBuffer;
		iface.getBufferTransientById = getBufferTransientById;
		iface.getBufferExternalHandle = getBufferExternalHandle;
		iface.closeBufferExternalHandle = closeBufferExternalHandle;
		iface.createBufferFromExternalHandle = createBufferFromExternalHandle;
		iface.createTexture = createTexture;
		iface.destroyTexture = destroyTexture;
		iface.getTextureTransient = getTextureTransient;
		iface.registerTextureAsTransient = registerTextureAsTransient;
		iface.aliasTextureTransient = aliasTextureTransient;
		iface.enqueueAcquireTexture = enqueueAcquireTexture;
		iface.getAcquiredTexture = getAcquiredTexture;
		iface.getTextureTransientById = getTextureTransientById;
		iface.createSampler = createSampler;
		iface.getDefaultSampler = getDefaultSampler;
		iface.destroySampler = destroySampler;
		iface.createComputePipeline = createComputePipeline;
		iface.destroyComputePipeline = destroyComputePipeline;
		iface.addPassCompute = addPassCompute;
		iface.addPassCopyBuffer = addPassCopyBuffer;
		iface.addPassCopyBufferToTexture = addPassCopyBufferToTexture;
		iface.addPassCopyTextureToBuffer = addPassCopyTextureToBuffer;
		iface.addPassCopyTexture = addPassCopyTexture;
	}
	return &iface;
}

NvFlowContext *get_flow_context(Context *p_context) {
	return reinterpret_cast<NvFlowContext *>(p_context);
}

void flush(Context *p_context) {
	Context *c = p_context;
	// Acquired transients become owned resources.
	for (BufferAcquire *a : c->buffer_acquires) {
		if (a->buffer == nullptr && a->transient != nullptr) {
			a->buffer = a->transient->buffer;
			a->transient = nullptr;
			a->buffer->refs++;
		}
	}
	for (TextureAcquire *a : c->texture_acquires) {
		if (a->texture == nullptr && a->transient != nullptr) {
			a->texture = a->transient->texture;
			a->transient = nullptr;
			a->texture->refs++;
		}
	}
	for (BufferTransient *t : c->buffer_transients) {
		memdelete(t);
	}
	c->buffer_transients.clear();
	for (TextureTransient *t : c->texture_transients) {
		memdelete(t);
	}
	c->texture_transients.clear();

	// This frame's readbacks, then a fence that marks the frame done once the
	// GPU has finished it (async callbacks fire in submission order).
	for (Buffer *b : c->buffers) {
		if (b->readback_pending && b->storage.is_valid()) {
			b->readback_pending = false;
			c->rd->buffer_get_data_async(b->storage, callable_mp(c->callbacks, &Callbacks::on_readback).bind(b->id));
		}
	}
	c->rd->buffer_get_data_async(c->fence, callable_mp(c->callbacks, &Callbacks::on_fence).bind(c->frame));

	for (Buffer *b : c->dead_buffers) {
		if (b->refs <= 0 && !b->pooled) {
			buffer_free(c, b);
		}
	}
	c->dead_buffers.clear();
	for (Texture *t : c->dead_textures) {
		if (t->refs <= 0 && !t->pooled) {
			texture_free(c, t);
		}
	}
	c->dead_textures.clear();

	// Idle pool resources.
	for (int i = (int)c->buffers.size() - 1; i >= 0; i--) {
		Buffer *b = c->buffers[i];
		if (b->pooled && b->refs == 0 && b->last_frame + POOL_LIFETIME < c->frame) {
			buffer_free(c, b);
		}
	}
	for (int i = (int)c->textures.size() - 1; i >= 0; i--) {
		Texture *t = c->textures[i];
		if (t->pooled && t->refs == 0 && t->last_frame + POOL_LIFETIME < c->frame) {
			texture_free(c, t);
		}
	}

	c->stats.passes = c->passes_this_frame;
	c->passes_this_frame = 0;
	c->stats.current_frame = c->frame;
	c->stats.last_completed_frame = c->completed;
	c->frame++;
}

void *import_texture(Context *p_context, uint64_t p_rid, uint32_t p_width, uint32_t p_height, int p_format) {
	Context *c = p_context;
	Texture *t = memnew(Texture);
	t->external = true;
	t->rid = RID::from_uint64(p_rid);
	t->desc.textureType = eNvFlowTextureType_2d;
	t->desc.usageFlags = eNvFlowTextureUsage_texture | eNvFlowTextureUsage_rwTexture;
	t->desc.format = (NvFlowFormat)p_format;
	t->desc.width = p_width;
	t->desc.height = p_height;
	t->desc.depth = 1;
	t->desc.mipLevels = 1;
	t->last_frame = c->frame;
	c->textures.push_back(t);
	c->stats.textures++;
	// Gone at the next flush; it was only ever borrowed for this frame.
	c->dead_textures.push_back(t);
	TextureTransient *tt = memnew(TextureTransient);
	tt->desc = t->desc;
	tt->texture = t;
	c->texture_transients.push_back(tt);
	return tt;
}

uint64_t transient_texture_rid(void *p_transient) {
	if (p_transient == nullptr) {
		return 0;
	}
	return reinterpret_cast<TextureTransient *>(p_transient)->texture->rid.get_id();
}

Stats get_stats(Context *p_context) {
	Stats s = p_context->stats;
	return s;
}

} // namespace FlowContextRD

#endif // GODOT_PHYSX_FLOW
