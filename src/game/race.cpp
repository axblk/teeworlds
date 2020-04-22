/* race class by Sushi and Redix */

#include <base/math.h>
#include <base/system.h>

#include "race.h"

inline void AppendDecimals(char *pBuf, int Size, int Time, int Precision)
{
	if(Precision > 0)
	{
		char aInvalid[] = ".---";
		char aMSec[] = {
			'.',
			(char)('0' + (Time / 100) % 10),
			(char)('0' + (Time / 10) % 10),
			(char)('0' + Time % 10),
			0
		};
		char *pDecimals = Time < 0 ? aInvalid : aMSec;
		pDecimals[min(Precision, 3)+1] = 0;
		str_append(pBuf, pDecimals, Size);
	}
}

void IRace::FormatTime(char *pBuf, int Size, int Time, int Precision)
{
	if(Time < 0)
		str_copy(pBuf, "-:--", Size);
	else
		str_format(pBuf, Size, "%02d:%02d", Time / (60 * 1000), (Time / 1000) % 60);
	AppendDecimals(pBuf, Size, Time, Precision);
}

void IRace::FormatTimeDiff(char *pBuf, int Size, int Time, int Precision, bool ForceSign)
{
	const char *pPositive = ForceSign ? "+" : "";
	const char *pSign = Time < 0 ? "-" : pPositive;
	Time = absolute(Time);
	str_format(pBuf, Size, "%s%d", pSign, Time / 1000);
	AppendDecimals(pBuf, Size, Time, Precision);
}
