#ifndef GAME_SERVER_SCORE_SQLITESCORE_H
#define GAME_SERVER_SCORE_SQLITESCORE_H

#include <base/tl/array.h>

#include <engine/shared/jobs.h>

#include "../score.h"

struct sqlite3;

struct CSqlExecData
{
	typedef int(*FQueryFunc)(sqlite3 *pDB, bool Error, void *pUserData);

	CSqlExecData(FQueryFunc pFunc, int Flags, void *pUserData = 0)
		: m_pFunc(pFunc), m_Flags(Flags), m_pUserData(pUserData), m_HandlerResult(0) { }

	CJob m_Job;
	FQueryFunc m_pFunc;
	int m_Flags;
	void *m_pUserData;
	int m_HandlerResult;
};

struct CRecordData
{
	char m_aPlayerName[MAX_NAME_LENGTH];
	int m_Time;
	int m_Rank;
};

class CSQLiteScore : public IScore
{
	struct CLoadMapData
	{
		// in
		char m_aMapName[128];
		// out
		int m_MapID;
		int m_BestTime;
	};

	struct CLoadPlayerData
	{
		// in
		int m_ClientID;
		char m_aPlayerName[MAX_NAME_LENGTH];
		int m_MapID;
		// out
		int m_PlayerID;
		int m_Time;
		int m_aCpTime[NUM_CHECKPOINTS];
	};

	struct CShowTop5Data
	{
		// in
		int m_ClientID;
		int m_MapID;
		int m_Num; // +out
		// out
		CRecordData m_aRecords[5];
	};

	struct CSaveScoreData
	{
		// in
		int m_PlayerID; // +out
		int m_ClientID;
		char m_aPlayerName[MAX_NAME_LENGTH];
		int m_MapID;
		int m_Time;
		int m_aCpTime[NUM_CHECKPOINTS];
	};

	struct CShowRankData
	{
		// in
		char m_RequestingClientID;
		int m_MapID;
		// search param
		int m_PlayerID;
		char m_aName[MAX_NAME_LENGTH];
		// out
		int m_Num;
		CRecordData m_aRecords[5];
	};

	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	static char m_aDBFilename[512];

	bool m_DbValid;
	int m_MapID;
	int m_aPlayerID[MAX_PLAYERS];

	array<CSqlExecData*> m_lPendingActions;
	
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	static int ExecSqlFunc(void *pUser);

	static int CreateTablesHandler(sqlite3 *pDB, bool Error, void *pUser);
	static int LoadMapHandler(sqlite3 *pDB, bool Error, void *pUser);
	static int LoadPlayerHandler(sqlite3 *pDB, bool Error, void *pUser);
	static int SaveScoreHandler(sqlite3 *pDB, bool Error, void *pUser);
	static int ShowRankHandler(sqlite3 *pDB, bool Error, void *pUser);
	static int ShowTop5Handler(sqlite3 *pDB, bool Error, void *pUser);
	
	void StartSqlJob(CSqlExecData::FQueryFunc pFunc, int Flags, void *pUserData = 0);
	void OnHandlerFinished(const CSqlExecData *pAction);
	
public:
	
	CSQLiteScore(CGameContext *pGameServer);
	~CSQLiteScore();

	void OnMapLoad();
	void Tick();
	
	void OnPlayerInit(int ClientID, bool PrintRank);
	void OnPlayerLeave(int ClientID) { m_aPlayerID[ClientID] = -1; };
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);

	void ShowTop5(int ClientID, int Debut=1);
	void ShowRank(int ClientID, const char *pName);
	void ShowRank(int ClientID);
};

#endif
