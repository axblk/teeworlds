#include <engine/external/sqlite/sqlite3.h>

#include <engine/engine.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include <game/race.h>

#include "../gamecontext.h"
#include "sqlite_score.h"

// TODO:
// use sqlite from OS
// add mysql
// better error handling
// display last runs (timestamps)
// total records for top5
// handle gametypes

char CSQLiteScore::m_aDBFilename[512];

CSQLiteScore::CSQLiteScore(CGameContext *pGameServer)
	: IScore(pGameServer), m_pGameServer(pGameServer), m_pServer(pGameServer->Server()), m_DbValid(false), m_MapID(-1)
{
	char m_aFile[512];
	str_format(m_aFile, sizeof(m_aFile), "records/%s", g_Config.m_SvSQLiteDatabase);
	GameServer()->Storage()->GetCompletePath(IStorage::TYPE_SAVE, m_aFile, m_aDBFilename, sizeof(m_aDBFilename));

	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

	CSqlExecData ExecData(CreateTablesHandler, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
	int rc = ExecSqlFunc(&ExecData);
	if(rc == SQLITE_OK && ExecData.m_HandlerResult == 0)
		m_DbValid = true;
}

CSQLiteScore::~CSQLiteScore()
{
}

bool ExecuteStatement(sqlite3 *pDB, const char *pSQL)
{
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(pDB, pSQL, -1, &pStmt, NULL);
	bool Error = rc != SQLITE_OK;
	if(Error)
	{
		dbg_msg("sqlite", "error preparing statement: %s", sqlite3_errmsg(pDB));
	}
	else
	{
		rc = sqlite3_step(pStmt);
		Error = rc != SQLITE_DONE;
		if(Error)
			dbg_msg("sqlite", "error executing statement: %s", sqlite3_errmsg(pDB));
	}
	sqlite3_finalize(pStmt);
	return Error;
}

void CSQLiteScore::StartSqlJob(CSqlExecData::FQueryFunc pFunc, int Flags, CRequestData *pUserData)
{
	CSqlExecData *pExecData = new CSqlExecData(pFunc, Flags, pUserData);
	m_lPendingActions.add(pExecData);
	GameServer()->Engine()->AddJob(&pExecData->m_Job, ExecSqlFunc, pExecData);
}

void CSQLiteScore::OnHandlerFinished(const CSqlExecData *pAction)
{
	bool Error = pAction->m_Job.Result() != SQLITE_OK || pAction->m_HandlerResult != 0;
	bool MapChanged = m_MapID != pAction->m_pUserData->m_MapID;

	if(pAction->m_pFunc == LoadMapHandler)
	{
		CLoadMapData *pData = (CLoadMapData*)pAction->m_pUserData;
		MapChanged = str_comp(pData->m_aMapName, Server()->GetMapName()) != 0;
		if(!Error && !MapChanged)
		{
			m_MapID = pData->m_MapID;
			UpdateRecord(pData->m_BestTime);

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(Server()->ClientIngame(i))
					OnPlayerInit(i);
			}
		}
		delete pData;
	}
	else if(pAction->m_pFunc == LoadPlayerHandler)
	{
		CLoadPlayerData *pData = (CLoadPlayerData*)pAction->m_pUserData;
		bool ClientChanged = !Server()->ClientIngame(pData->m_ClientID) || str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) != 0;
		if(!Error && !MapChanged && !ClientChanged)
		{
			if(pData->m_PlayerID != -1)
			{
				dbg_msg("sqlite", "player init: %d:%d", pData->m_ClientID, pData->m_PlayerID);
				m_aPlayerID[pData->m_ClientID] = pData->m_PlayerID;
				m_aPlayerData[pData->m_ClientID].SetTime(pData->m_Time, pData->m_aCpTime);
				if(g_Config.m_SvLoadBest)
					m_aPlayerData[pData->m_ClientID].UpdateCurTime(pData->m_Time);
			}
		}
		delete pData;
	}
	else if(pAction->m_pFunc == SaveScoreHandler)
	{
		CSaveScoreData *pData = (CSaveScoreData*)pAction->m_pUserData;
		bool ClientChanged = !Server()->ClientIngame(pData->m_ClientID) || str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) != 0;
		if(!Error)
		{
			dbg_msg("sqlite", "saved score: %d", pData->m_PlayerID);
			if(!MapChanged && !ClientChanged && pData->m_PlayerID != -1)
			{
				dbg_msg("sqlite", "player init: %d:%d", pData->m_ClientID, pData->m_PlayerID);
				m_aPlayerID[pData->m_ClientID] = pData->m_PlayerID;
			}
		}
		delete pData;
	}
	else if(pAction->m_pFunc == ShowRankHandler)
	{
		CShowRankData *pData = (CShowRankData*)pAction->m_pUserData;
		bool ClientLeft = !Server()->ClientIngame(pData->m_RequestingClientID);
		if(!Error && !MapChanged && !ClientLeft)
		{
			char aBuf[128];
			char aTime[32];
			int To = pData->m_RequestingClientID;
			bool Own = pData->m_PlayerID != -1 && pData->m_PlayerID == m_aPlayerID[pData->m_RequestingClientID];

			if(pData->m_Num > 1)
			{
				if(pData->m_Num > MAX_SEARCH_RECORDS)
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" (%d of %d) -----", pData->m_aName, MAX_SEARCH_RECORDS, pData->m_Num);
				else
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" -----", pData->m_aName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);

				for(int i = 0; i < min((int)MAX_SEARCH_RECORDS, pData->m_Num); i++)
				{
					const CRecordData *pRec = &pData->m_aRecords[i];
					IRace::FormatTimeLong(aTime, sizeof(aTime), pRec->m_Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);
					GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
				}
				GameServer()->SendChat(-1, CHAT_ALL, To, "------------------------------");
			}
			else if(pData->m_Num == 1)
			{
				const CRecordData *pRec = &pData->m_aRecords[0];
				IRace::FormatTimeLong(aTime, sizeof(aTime), pRec->m_Time);
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);

				if(g_Config.m_SvShowTimes)
				{
					To = -1;
					if(!Own)
					{
						char aBuf2[32];
						str_format(aBuf2, sizeof(aBuf2), " (requested by %s)", Server()->ClientName(pData->m_RequestingClientID));
						str_append(aBuf, aBuf2, sizeof(aBuf));
					}
				}
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
			}
			else // Num = 0
			{
				if(Own)
					str_copy(aBuf, "You are not ranked", sizeof(aBuf));
				else
					str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
			}
		}
		delete pData;
	}
	else if(pAction->m_pFunc == ShowTop5Handler)
	{
		CShowTop5Data *pData = (CShowTop5Data*)pAction->m_pUserData;
		bool ClientLeft = !Server()->ClientIngame(pData->m_RequestingClientID);
		if(!Error && !MapChanged && !ClientLeft)
		{
			char aBuf[128];
			char aTime[32];
			GameServer()->SendChat(-1, CHAT_ALL, pData->m_RequestingClientID, "----------- Top 5 -----------");
			for(int i = 0; i < min((int)MAX_TOP_RECORDS, pData->m_Num); i++)
			{
				IRace::FormatTimeLong(aTime, sizeof(aTime), pData->m_aRecords[i].m_Time);
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
					pData->m_aRecords[i].m_Rank, pData->m_aRecords[i].m_aPlayerName, aTime);
				GameServer()->SendChat(-1, CHAT_ALL, pData->m_RequestingClientID, aBuf);
			}
			GameServer()->SendChat(-1, CHAT_ALL, pData->m_RequestingClientID, "------------------------------");
			if(pData->m_TotalRecords > 5)
			{
				str_format(aBuf, sizeof(aBuf), "Total records: %d", pData->m_TotalRecords);
				GameServer()->SendChat(-1, CHAT_ALL, pData->m_RequestingClientID, aBuf);
			}
		}
		delete pData;
	}
}

int CSQLiteScore::ExecSqlFunc(void *pUser)
{
	CSqlExecData *pData = (CSqlExecData *)pUser;

	sqlite3 *pDB;
	int rc = sqlite3_open_v2(m_aDBFilename, &pDB, pData->m_Flags, NULL);
	if(rc == SQLITE_OK)
		pData->m_HandlerResult = pData->m_pFunc(pDB, pData->m_pUserData);
	else
		dbg_msg("sqlite", "can't open database: %s", sqlite3_errmsg(pDB));

	sqlite3_close(pDB);
	return rc;
}

int CSQLiteScore::CreateTablesHandler(sqlite3 *pDB, CRequestData *pUser)
{
	bool Error = false;
	Error = Error || ExecuteStatement(pDB, "PRAGMA foreign_keys = ON;");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS maps (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, gametype TEXT);");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS races (player INTEGER REFERENCES players, map INTEGER REFERENCES maps, time INTEGER, checkpoints TEXT, timestamp INTEGER);");
	Error = Error || ExecuteStatement(pDB, "CREATE INDEX IF NOT EXISTS raceindex ON races (map, player);");

	return Error ? -1 : 0;
}

int CSQLiteScore::LoadMapHandler(sqlite3 *pDB, CRequestData *pUser)
{
	CLoadMapData *pData = (CLoadMapData *)pUser;
	sqlite3_stmt *pStmt;

	sqlite3_prepare_v2(pDB, "SELECT * FROM maps WHERE name = ?1;", -1, &pStmt, NULL);
	sqlite3_bind_text(pStmt, 1, pData->m_aMapName, -1, SQLITE_STATIC);
	if(sqlite3_step(pStmt) == SQLITE_ROW)
		pData->m_MapID = sqlite3_column_int(pStmt, 0);
	sqlite3_finalize(pStmt);

	if(pData->m_MapID != -1)
	{
		sqlite3_prepare_v2(pDB, "SELECT time FROM races WHERE map = ?1 ORDER BY time ASC LIMIT 1;", -1, &pStmt, NULL);
		sqlite3_bind_int(pStmt, 1, pData->m_MapID);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
			pData->m_BestTime = sqlite3_column_int(pStmt, 0);
		sqlite3_finalize(pStmt);
	}
	else
	{
		sqlite3_prepare_v2(pDB, "INSERT INTO maps (name) VALUES (?1);", -1, &pStmt, NULL);
		sqlite3_bind_text(pStmt, 1, pData->m_aMapName, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_DONE)
			pData->m_MapID = sqlite3_last_insert_rowid(pDB);
		sqlite3_finalize(pStmt);

		dbg_msg("sqlite", "added map: %d:%s", pData->m_MapID, pData->m_aMapName);
	}

	return 0;
}

void CSQLiteScore::OnMapLoad()
{
	IScore::OnMapLoad();

	m_MapID = -1;
	for(int i = 0; i < MAX_PLAYERS; i++)
		m_aPlayerID[i] = -1;

	if(!m_DbValid)
		return;

	CLoadMapData *pData = new CLoadMapData();
	str_copy(pData->m_aMapName, Server()->GetMapName(), sizeof(pData->m_aMapName));
	pData->m_MapID = -1;
	pData->m_BestTime = 0;

	StartSqlJob(LoadMapHandler, SQLITE_OPEN_READWRITE, pData);
}

void CSQLiteScore::Tick()
{
	for(int i = 0; i < m_lPendingActions.size(); i++)
	{
		CSqlExecData *pAction = m_lPendingActions[i];
		if(pAction->m_Job.Status() == CJob::STATE_DONE)
		{
			m_lPendingActions.remove_index(i--);
			OnHandlerFinished(pAction);
			delete pAction;
		}
	}
}

int CSQLiteScore::LoadPlayerHandler(sqlite3 *pDB, CRequestData *pUser)
{
	CLoadPlayerData *pData = (CLoadPlayerData *)pUser;
	sqlite3_stmt *pStmt;

	if(pData->m_PlayerID == -1)
	{
		sqlite3_prepare_v2(pDB, "SELECT * FROM players WHERE name = ?1;", -1, &pStmt, NULL);
		sqlite3_bind_text(pStmt, 1, pData->m_aPlayerName, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
			pData->m_PlayerID = sqlite3_column_int(pStmt, 0);
		sqlite3_finalize(pStmt);
	}

	if(pData->m_PlayerID != -1)
	{
		sqlite3_prepare_v2(pDB, "SELECT time, checkpoints FROM races WHERE map = ?1 AND player = ?2 ORDER BY time ASC LIMIT 1;", -1, &pStmt, NULL);
		sqlite3_bind_int(pStmt, 1, pData->m_MapID);
		sqlite3_bind_int(pStmt, 2, pData->m_PlayerID);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
		{
			pData->m_Time = sqlite3_column_int(pStmt, 0);
			const char *pChTime = (const char*)sqlite3_column_text(pStmt, 1);
			for(int i = 0; pChTime && i < NUM_CHECKPOINTS; i++)
			{
				pData->m_aCpTime[i] = str_toint(pChTime);
				pChTime = str_find(pChTime, ";");
				if(pChTime)
					pChTime++;
			}
		}
		sqlite3_finalize(pStmt);
	}
			
	return 0;
}

void CSQLiteScore::OnPlayerInit(int ClientID)
{
	m_aPlayerData[ClientID].Reset();

	if(!m_DbValid || m_MapID == -1)
		return;

	CLoadPlayerData *pData = new CLoadPlayerData();
	pData->m_ClientID = ClientID;
	str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	pData->m_MapID = m_MapID;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	pData->m_Time = 0;
	mem_zero((void*)pData->m_aCpTime, sizeof(pData->m_aCpTime));

	StartSqlJob(LoadPlayerHandler, SQLITE_OPEN_READONLY, pData);
}

int CSQLiteScore::SaveScoreHandler(sqlite3 *pDB, CRequestData *pUser)
{
	CSaveScoreData *pData = (CSaveScoreData *)pUser;
	sqlite3_stmt *pStmt;

	if(pData->m_PlayerID == -1)
	{
		sqlite3_prepare_v2(pDB, "INSERT INTO players (name) VALUES (?1);", -1, &pStmt, NULL);
		sqlite3_bind_text(pStmt, 1, pData->m_aPlayerName, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_DONE)
			pData->m_PlayerID = sqlite3_last_insert_rowid(pDB);
		sqlite3_finalize(pStmt);

		dbg_msg("sqlite", "added player: %d:%s", pData->m_PlayerID, pData->m_aPlayerName);
	}

	if(pData->m_PlayerID != -1)
	{
		sqlite3_prepare_v2(pDB, "INSERT INTO races (map, player, time, checkpoints) VALUES (?1, ?2, ?3, ?4);", -1, &pStmt, NULL);
		sqlite3_bind_int(pStmt, 1, pData->m_MapID);
		sqlite3_bind_int(pStmt, 2, pData->m_PlayerID);
		sqlite3_bind_int(pStmt, 3, pData->m_Time);
		int End = NUM_CHECKPOINTS-1;
		for(; End > 0 && pData->m_aCpTime[End] == 0; End--);
		char aCp[1024] = {0};
		char aBuf2[32];
		for(int i = 0; i <= End; i++)
		{
			str_format(aBuf2, sizeof(aBuf2), i == 0 ? "%d" : ";%d", pData->m_aCpTime[i]);
			str_append(aCp, aBuf2, sizeof(aCp));
		}
		sqlite3_bind_text(pStmt, 4, aCp, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_DONE)
			dbg_msg("sqlite", "added race: %d %d %d", pData->m_MapID, pData->m_PlayerID, pData->m_Time);
		sqlite3_finalize(pStmt);
	}
			
	return 0;
}

void CSQLiteScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	m_aPlayerData[ClientID].UpdateTime(Time, pCpTime);
	UpdateRecord(Time);

	if(!m_DbValid || m_MapID == -1)
		return;

	CSaveScoreData *pData = new CSaveScoreData();
	pData->m_MapID = m_MapID;
	pData->m_ClientID = -1;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	if(pData->m_PlayerID == -1)
	{
		pData->m_ClientID = ClientID;
		str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	}
	pData->m_Time = Time;
	mem_copy(pData->m_aCpTime, pCpTime, sizeof(pData->m_aCpTime));
	
	StartSqlJob(SaveScoreHandler, SQLITE_OPEN_READWRITE, pData);
}

void CSQLiteScore::RecordFromRow(CRecordData *pRecord, sqlite3_stmt *pStmt)
{
	str_copy(pRecord->m_aPlayerName, (const char*)sqlite3_column_text(pStmt, 0), sizeof(pRecord->m_aPlayerName));
	pRecord->m_Time = sqlite3_column_int(pStmt, 1);
	pRecord->m_Rank = sqlite3_column_int(pStmt, 2);
}

int CSQLiteScore::ShowRankHandler(sqlite3 *pDB, CRequestData *pUser)
{
	CShowRankData *pData = (CShowRankData *)pUser;
	sqlite3_stmt *pStmt;

	sqlite3_prepare_v2(pDB, "CREATE TEMPORARY TABLE _ranking AS SELECT player, time, RANK() OVER (ORDER BY `time` ASC) AS rank FROM (SELECT player, min(time) as time FROM races WHERE map = ?1 GROUP BY player);", -1, &pStmt, NULL);
	sqlite3_bind_int(pStmt, 1, pData->m_MapID);
	sqlite3_step(pStmt);
	sqlite3_finalize(pStmt);

	if(pData->m_PlayerID != -1)
	{
		sqlite3_prepare_v2(pDB, "SELECT name, time, rank FROM _ranking INNER JOIN players ON player = players.id WHERE player = ?1 LIMIT 1;", -1, &pStmt, NULL);
		sqlite3_bind_int(pStmt, 1, pData->m_PlayerID);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
			RecordFromRow(&pData->m_aRecords[pData->m_Num++], pStmt);
		sqlite3_finalize(pStmt);
	}
	else
	{
		sqlite3_prepare_v2(pDB, "SELECT name, time, rank FROM _ranking INNER JOIN players ON player = players.id WHERE name = ?1 LIMIT 1;", -1, &pStmt, NULL);
		sqlite3_bind_text(pStmt, 1, pData->m_aName, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
			RecordFromRow(&pData->m_aRecords[pData->m_Num++], pStmt);
		sqlite3_finalize(pStmt);

		if(pData->m_Num == 0)
		{
			sqlite3_prepare_v2(pDB, "SELECT name, time, rank FROM _ranking INNER JOIN players ON player = players.id WHERE name LIKE '%' || ?1 || '%';", -1, &pStmt, NULL);
			sqlite3_bind_text(pStmt, 1, pData->m_aName, -1, SQLITE_STATIC);
			while(sqlite3_step(pStmt) == SQLITE_ROW)
			{
				if(pData->m_Num < MAX_SEARCH_RECORDS)
					RecordFromRow(&pData->m_aRecords[pData->m_Num], pStmt);
				pData->m_Num++;
			}
			sqlite3_finalize(pStmt);
		}
	}

	ExecuteStatement(pDB, "DROP TABLE _ranking;");

	return 0;
}

void CSQLiteScore::ShowRank(int RequestingClientID, const char *pName)
{
	if(!m_DbValid || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aPlayerID[i] != 0 && str_comp(Server()->ClientName(i), pName) == 0)
		{
			ShowRank(RequestingClientID, i);
			return;
		}
	}

	UpdateThrottling(RequestingClientID);

	CShowRankData *pData = new CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = RequestingClientID;
	pData->m_PlayerID = -1;
	str_copy(pData->m_aName, pName, sizeof(pData->m_aName));
	pData->m_Num = 0;
	
	StartSqlJob(ShowRankHandler, SQLITE_OPEN_READONLY, pData);
}

void CSQLiteScore::ShowRank(int RequestingClientID, int ClientID)
{
	if(!m_DbValid || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	if(m_aPlayerID[ClientID] == -1)
	{
		char aBuf[128];
		if(ClientID == RequestingClientID)
			str_copy(aBuf, "You are not ranked", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "%s is not ranked", Server()->ClientName(ClientID));
		GameServer()->SendChat(-1, CHAT_ALL, RequestingClientID, aBuf);
		return;
	}

	UpdateThrottling(RequestingClientID);

	CShowRankData *pData = new CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = ClientID;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	pData->m_Num = 0;
	
	StartSqlJob(ShowRankHandler, SQLITE_OPEN_READONLY, pData);
}

int CSQLiteScore::ShowTop5Handler(sqlite3 *pDB, CRequestData *pUser)
{
	CShowTop5Data *pData = (CShowTop5Data *)pUser;
	sqlite3_stmt *pStmt;

	sqlite3_prepare_v2(pDB, "SELECT name, time, RANK() OVER (ORDER BY `time` ASC) AS rank FROM (SELECT player, min(time) as time FROM races WHERE map = ?1 GROUP BY player) INNER JOIN players ON player = players.id LIMIT ?2, ?3;", -1, &pStmt, NULL);
	sqlite3_bind_int(pStmt, 1, pData->m_MapID);
	sqlite3_bind_int(pStmt, 2, pData->m_Start);
	sqlite3_bind_int(pStmt, 3, MAX_TOP_RECORDS);

	while(sqlite3_step(pStmt) == SQLITE_ROW && pData->m_Num < MAX_TOP_RECORDS)
		RecordFromRow(&pData->m_aRecords[pData->m_Num++], pStmt);
	sqlite3_finalize(pStmt);

	return 0;
}

void CSQLiteScore::ShowTop5(int RequestingClientID, int Debut)
{
	if(!m_DbValid || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	UpdateThrottling(RequestingClientID);

	CShowTop5Data *pData = new CShowTop5Data();
	pData->m_RequestingClientID = RequestingClientID;
	pData->m_MapID = m_MapID;
	pData->m_Start = Debut;
	pData->m_Num = 0;
	pData->m_TotalRecords = 0;
	
	StartSqlJob(ShowTop5Handler, SQLITE_OPEN_READONLY, pData);
}
