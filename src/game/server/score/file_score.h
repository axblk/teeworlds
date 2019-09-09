/* copyright (c) 2008 rajh and gregwar. Score stuff */

#ifndef GAME_SERVER_SCORE_FILESCORE_H
#define GAME_SERVER_SCORE_FILESCORE_H

#include <base/tl/sorted_array.h>
#include "../score.h"

class CFileScore : public IScore
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;
	
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

	char m_aMap[64];
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }
	
	CPlayerScore *SearchScoreByID(int ID, int *pPosition=0);
	CPlayerScore *SearchScoreByName(const char *pName, int *pPosition, bool ExactMatch);

	void ProcessJobs(bool Block);
	static void SaveScoreThread(void *pUser);

	void WriteEntry(IOHANDLE File, const CPlayerScore *pEntry) const;
	IOHANDLE OpenFile(int Flags) const;
	
	bool CheckSpamProtection(int ClientID);

public:
	CFileScore(CGameContext *pGameServer);
	~CFileScore();

	void OnMapLoad();
	void Tick() { ProcessJobs(false); }
	
	void OnPlayerInit(int ClientID, bool PrintRank);
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);
	
	void ShowTop5(int ClientID, int Debut=1);
	void ShowRank(int ClientID, const char *pName);
	void ShowRank(int ClientID);
};

#endif
