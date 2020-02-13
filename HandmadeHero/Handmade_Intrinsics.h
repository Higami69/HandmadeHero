#pragma once
#include "math.h"

inline int32 RoundReal32ToInt32(real32 value)
{
	int32 result = (int32)roundf(value);
	return result;
}

inline uint32 RoundReal32ToUint32(real32 value)
{
	uint32 result = (uint32)roundf(value);
	return result;
}

inline int32 FloorReal32ToInt32(real32 value)
{
	int32 result = (int32)floorf(value);
	return result;
}

inline int32 TruncateReal32ToInt32(real32 value)
{
	int32 result = (int32)(value);
	return result;
}

inline real32 Sin(real32 angle)
{
	real32 result = sinf(angle);
	return result;
}

inline real32 Cos(real32 angle)
{
	real32 result = cosf(angle);
	return result;
}

inline real32 Atan2(real32 y, real32 x)
{
	real32 result = atan2f(y, x);
	return result;
}

struct bit_scan_result
{
	bool32 found;
	uint32 index;
};
inline bit_scan_result FindLeastSignificantSetBit(uint32 value)
{
	bit_scan_result result = {};

#if COMPILER_MSVC
	result.found = _BitScanForward((unsigned long*)&result.index, value);
#else
	for (uint32 test = 0; test < 32; ++test)
	{
		if (value & (1 << test))
		{
			result.index = test;
			result.found = true;
			break;
		}
	}
#endif

	return result;
}