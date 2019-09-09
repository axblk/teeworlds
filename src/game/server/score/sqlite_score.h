#ifndef GAME_SERVER_SCORE_SQLITESCORE_H
#define GAME_SERVER_SCORE_SQLITESCORE_H

#include <base/tl/array.h>

#include <engine/shared/jobs.h>

#include "../score.h"

struct sqlite3;

class CSQLiteScore : public IScore
{
	enum
	{
		MAX_SEARCH_RECORDS=5,
		MAX_TOP_RECORDS=5
	};

	struct CRecordData
	{
		char m_aPlayerName[MAX_NAME_LENGTH];
		int m_Time;
		int m_Rank;
	};

	struct CRequestData
	{
		// in/out
		int m_MapID;
	};

	struct CLoadMapData : CRequestData
	{
		// in
		char m_aMapName[128];
		// out
		int m_BestTime;
	};

	struct CLoadPlayerData : CRequestData
	{
		// in
		int m_ClientID;
		char m_aPlayerName[MAX_NAME_LENGTH];
		// out
		int m_PlayerID;
		int m_Time;
		int m_aCpTime[NUM_CHECKPOINTS];
	};

	struct CSaveScoreData : CRequestData
	{
		// in
		int m_PlayerID; // +out
		int m_ClientID;
		char m_aPlayerName[MAX_NAME_LENGTH];
		int m_Time;
		int m_aCpTime[NUM_CHECKPOINTS];
	};

	struct CShowRankData : CRequestData
	{
		// in
		char m_RequestingClientID;
		// search param
		int m_PlayerID;
		char m_aName[MAX_NAME_LENGTH];
		// out
		int m_Num;
		CRecordData m_aRecords[MAX_SEARCH_RECORDS];
	};

	struct CShowTop5Data : CRequestData
	{
		// in
		int m_RequestingClientID;
		int m_Start;
		// out
		int m_Num;
		CRecordData m_aRecords[MAX_TOP_RECORDS];
		int m_TotalRecords;
	};

	struct CSqlExecData
	{
		typedef int(*FQueryFunc)(sqlite3 *pDB, CSQLiteScore::CRequestData *pUserData);

		CSqlExecData(FQueryFunc pFunc, int Flags, CSQLiteScore::CRequestData *pUserData = 0)
			: m_pFunc(pFunc), m_Flags(Flags), m_pUserData(pUserData), m_HandlerResult(-1) { }

		CJob m_Job;
		FQueryFunc m_pFunc;
		int m_Flags;
		CSQLiteScore::CRequestData *m_pUserData;
		int m_HandlerResult;
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

	void StartSqlJob(CSqlExecData::FQueryFunc pFunc, int Flags, CRequestData *pUserData = 0);
	void OnHandlerFinished(const CSqlExecData *pAction);

	static int ExecSqlFunc(void *pUser);
	static void RecordFromRow(CRecordData *pRecord, struct sqlite3_stmt *pStmt);

	static int CreateTablesHandler(sqlite3 *pDB, CRequestData *pUser);
	static int LoadMapHandler(sqlite3 *pDB, CRequestData *pUser);
	static int LoadPlayerHandler(sqlite3 *pDB, CRequestData *pUser);
	static int SaveScoreHandler(sqlite3 *pDB, CRequestData *pUser);
	static int ShowRankHandler(sqlite3 *pDB, CRequestData *pUser);
	static int ShowTop5Handler(sqlite3 *pDB, CRequestData *pUser);
	
public:
	
	CSQLiteScore(CGameContext *pGameServer);
	~CSQLiteScore();

	void OnMapLoad();
	void Tick();
	
	void OnPlayerInit(int ClientID);
	void OnPlayerLeave(int ClientID) { m_aPlayerID[ClientID] = -1; };
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);

	void ShowTop5(int RequestingClientID, int Debut=1);
	void ShowRank(int RequestingClientID, const char *pName);
	void ShowRank(int RequestingClientID, int ClientID);
};

#endif
