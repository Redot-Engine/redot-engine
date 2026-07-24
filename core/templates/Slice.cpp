#include "core/templates/Slice.h"
#include "core/typedefs.h"

Slice Slice::nil = {};

bool Slice::copy(Slice dst, Slice src) noexcept
{
	uintptr_t s = (uintptr_t)src.data;
	uintptr_t d = (uintptr_t)dst.data;
	size_t n = MIN(src.length, dst.length);
	size_t i;
	bool srcTailDoesntOverlapDstHead = s <= (d - n);
	const uint8_t *u8src = (const uint8_t*)src.data;
	uint8_t *u8dst = (uint8_t*)dst.data;
	if (src.length > dst.length) { return false; }
	if (srcTailDoesntOverlapDstHead)
	{
		i = dst.length;
		while (i > 0)
		{
			switch (i)
			{
			case 4:
				i -= 4;
				*((uint32_t*)&(u8dst[i])) = *((const uint32_t*)&(u8src[i]));
				break;
			case 7:
			case 3:
				i -= 1;
				u8dst[i] = u8src[i];
				[[fallthrough]];
			case 6:
			case 2:
				i -= 2;
				*((uint16_t*)&(u8dst[i])) = *((const uint16_t*)&(u8src[i]));
				break;
			case 5:
			case 1:
				i -= 1;
				u8dst[i] = u8src[i];
				break;
			default:
				i -= 8;
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				[[fallthrough]];
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
				i -= 8;
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				[[fallthrough]];
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
				i -= 8;
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				[[fallthrough]];
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				i -= 8;
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < dst.length;)
		{
			switch (dst.length - i)
			{
			case 4:
				*((uint32_t*)&(u8dst[i])) = *((const uint32_t*)&(u8src[i]));
				i += 4;
				break;
			case 7:
			case 3:
				u8dst[i] = u8src[i];
				i += 1;
				[[fallthrough]];
			case 6:
			case 2:
				*((uint16_t*)&(u8dst[i])) = *((const uint16_t*)&(u8src[i]));
				i += 2;
				break;
			case 5:
			case 1:
				u8dst[i] = u8src[i];
				i += 1;
				break;
			default:
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				i += 8;
				[[fallthrough]];
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				i += 8;
				[[fallthrough]];
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				i += 8;
				[[fallthrough]];
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				*((uint64_t*)&(u8dst[i])) = *((const uint64_t*)&(u8src[i]));
				i += 8;
				break;
			}
		}
	}
	return true;
}

void Slice::set(Slice dst, uint8_t n) noexcept
{
	uint16_t n16 = 0x0101 * n;
	uint32_t n32 = 0x01010101 * n;
	uint64_t n64 = 0x0101010101010101 * n;
	for (size_t i = 0; i < dst.length; i += 1)
	{
		switch (dst.length - i)
		{
		case 4:
			*((uint32_t*)get(dst, 1, i)) = n32;
			i += 4;
			break;
		case 7:
		case 3:
			*((uint8_t*)get(dst, 1, i)) = n;
			i += 1;
			[[fallthrough]];
		case 6:
		case 2:
			*((uint16_t*)get(dst, 1, i)) = n16;
			i += 2;
			break;
		case 5:
		case 1:
			*((uint8_t*)get(dst, 1, i)) = n;
			i += 1;
			break;
		default:
			*((uint64_t*)get(dst, 1, i)) = n64;
			i += 8;
			[[fallthrough]];
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
			*((uint64_t*)get(dst, 1, i)) = n64;
			i += 8;
			[[fallthrough]];
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
			*((uint64_t*)get(dst, 1, i)) = n64;
			i += 8;
			[[fallthrough]];
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			*((uint64_t*)get(dst, 1, i)) = n64;
			i += 8;
			break;
		}
	}
}
