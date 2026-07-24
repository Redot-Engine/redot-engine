#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

struct Slice
{
	void *data;
	size_t length;

	static Slice nil;

	constexpr static inline void *get(
		Slice self,
		size_t index,
		size_t size
	) noexcept;

	constexpr static inline bool subslice(
		Slice *dst,
		Slice src,
		size_t begin,
		size_t count
	) noexcept;

	static inline bool set(Slice dst, Slice src, size_t index) noexcept;
	static bool copy(Slice dst, Slice src) noexcept;
	static void set(Slice dst, uint8_t n) noexcept;
};

constexpr inline void *Slice::get(
	Slice self,
	size_t index,
	size_t size
) noexcept
{
	void *ret = nullptr;
	if ((size * index) < self.length)
	{
		ret = ((uint8_t *)self.data) + (size * index);
	}
	return ret;
}

constexpr inline bool Slice::subslice(
	Slice *dst,
	Slice src,
	size_t begin,
	size_t count
) noexcept
{
	bool ret = (src.length - count) < begin;
	assert(dst);
	if (!ret)
	{
		dst->data = &(((uint8_t*)src.data)[begin]);
		dst->length = count;
	}
	return ret;
}

inline bool Slice::set(Slice dst, Slice src, size_t index) noexcept
{
	Slice tmp = {};
	if (!subslice(&tmp, dst, index, src.length)) { return false; }
	return Slice::copy(tmp, src);
}

#define sliceAt(slice, t, i) ((t*)Slice::get(slice, i, sizeof(t)))
#define subslice_t(dst, src, t, begin, count) subslice( \
	(dst), \
	(src), \
	sizeof(t) * (begin), \
	sizeof(t) * (count) \
)
#define sliceCount(self, t) ((self).length / sizeof(t))
