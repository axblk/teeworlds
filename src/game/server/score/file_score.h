/* copyright (c) 2008 rajh and gregwar. Score stuff */

#ifndef GAME_SERVER_SCORE_FILESCORE_H
#define GAME_SERVER_SCORE_FILESCORE_H

#include <base/tl/sorted_array.h>

#include "../score.h"

class CFileScore : public IScoreBackend
{
	class IStorage *m_pStorage;

	class CPlayerScore
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		int m_Time;
		int m_aCpTime[NUM_CHECKPOINTS];

		CPlayerScore() {};
		CPlayerScore(const char *pName, int Time, int *apCpTime);

		bool operator<(const CPlayerScore& other) const { return (this->m_Time < other.m_Time); }
	};

	enum MyEnum
	{
		JOBTYPE_ADD_NEW=0,
		JOBTYPE_UPDATE_SCORE
	};

	struct CScoreJob
	{
		int m_Type;
		CPlayerScore *m_pEntry;
		CPlayerScore m_NewData;
	};
	
	sorted_array<CPlayerScore> m_lTop;
	array<CScoreJob> m_lJobQueue;

	int m_MapID;
	char m_aMap[64];

	CPlayerScore *SearchScoreByName(const char *pName, int *pPosition = 0);

	void ProcessJobs(bool Block);
	static void SaveScoreThread(void *pUser);

	void WriteEntry(IOHANDLE File, const CPlayerScore *pEntry) const;
	IOHANDLE OpenFile(int Flags) const;

	int LoadMapHandler(CMapData *pRequestData);
	int LoadPlayerHandler(CScoreData *pRequestData);
	int SaveScoreHandler(CScoreData *pRequestData);
	int ShowRankHandler(CRankData *pRequestData);
	int ShowTop5Handler(CRankData *pRequestData);

public:
	CFileScore(IScoreResponseListener *pListener, IStorage *pStorage);
	~CFileScore();

	bool Ready() const { return true; };
	void Tick() { ProcessJobs(false); }
	void AddRequest(int Type, CRequestData *pRequestData = 0);
};

#endif
