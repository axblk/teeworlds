/* race Class by Sushi and Redix */
#ifndef GAME_RACE_H
#define GAME_RACE_H

// helper class
class IRace
{
public:
	static void FormatTime(char *pBuf, int Size, int Time, int Precision = 3);
	static void FormatTimeDiff(char *pBuf, int Size, int Time, int Precision = 3, bool ForceSign = true);
};

#endif
