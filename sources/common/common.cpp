#include "common.h"

String split(Holder<PointerRange<const char>> &buf)
{
	for (uint32 i = 0; i < buf.size(); i++)
	{
		if (buf[i] == '\n')
		{
			const String s = String(PointerRange(buf.data(), buf.data() + i));
			*buf = PointerRange(buf.data() + i + 1, buf.end());
			return s;
		}
	}
	return "";
}
