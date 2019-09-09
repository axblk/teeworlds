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
// spam protection
// handle gametypes

char CSQLiteScore::m_aDBFilename[512];

CSQLiteScore::CSQLiteScore(CGameContext *pGameServer)
	: m_pGameServer(pGameServer), m_pServer(pGameServer->Server()), m_DbValid(false), m_MapID(-1)
{
	GameServer()->Storage()->GetCompletePath(IStorage::TYPE_SAVE, "records/race.db", m_aDBFilename, sizeof(m_aDBFilename));

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

int CSQLiteScore::CreateTablesHandler(sqlite3 *pDB, bool Error, void *pUser)
{
	Error = Error || ExecuteStatement(pDB, "PRAGMA foreign_keys = ON;");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS maps (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, gametype TEXT);");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);");
	Error = Error || ExecuteStatement(pDB, "CREATE TABLE IF NOT EXISTS races (player INTEGER REFERENCES players, map INTEGER REFERENCES maps, time INTEGER, checkpoints TEXT, timestamp INTEGER);");
	Error = Error || ExecuteStatement(pDB, "CREATE INDEX IF NOT EXISTS raceindex ON races (map, player);");

	return Error ? -1 : 0;
}

void CSQLiteScore::StartSqlJob(CSqlExecData::FQueryFunc pFunc, int Flags, void *pUserData)
{
	CSqlExecData *pExecData = new CSqlExecData(pFunc, Flags, pUserData);
	m_lPendingActions.add(pExecData);
	GameServer()->Engine()->AddJob(&pExecData->m_Job, ExecSqlFunc, pExecData);
}

int CSQLiteScore::LoadMapHandler(sqlite3 *pDB, bool Error, void *pUser)
{
	CLoadMapData *pData = (CLoadMapData *)pUser;
	if(Error)
		return -1;

	pData->m_MapID = -1;
	pData->m_BestTime = 0;

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

	CLoadMapData Data;
	str_copy(Data.m_aMapName, m_pServer->GetMapName(), sizeof(Data.m_aMapName));
	CSqlExecData ExecData(LoadMapHandler, SQLITE_OPEN_READWRITE, &Data);
	int rc = ExecSqlFunc(&ExecData);
	if(rc == SQLITE_OK && ExecData.m_HandlerResult == 0)
	{
		m_MapID = Data.m_MapID;
		UpdateRecord(Data.m_BestTime);
	}

	dbg_msg("sqlite", "loaded map: %d", m_MapID);
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

void CSQLiteScore::OnHandlerFinished(const CSqlExecData *pAction)
{
	bool Error = pAction->m_Job.Result() != SQLITE_OK || pAction->m_HandlerResult != 0;
	if(pAction->m_pFunc == ShowTop5Handler)
	{
		CShowTop5Data *pData = (CShowTop5Data*)pAction->m_pUserData;
		if(!Error)
		{
			char aBuf[128];
			char aTime[32];
			GameServer()->SendChat(-1, CHAT_ALL, pData->m_ClientID, "----------- Top 5 -----------");
			for(int i = 0; i < pData->m_Num; i++)
			{
				IRace::FormatTimeLong(aTime, sizeof(aTime), pData->m_aRecords[i].m_Time);
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
					pData->m_aRecords[i].m_Rank, pData->m_aRecords[i].m_aPlayerName, aTime);
				GameServer()->SendChat(-1, CHAT_ALL, pData->m_ClientID, aBuf);
			}
			GameServer()->SendChat(-1, CHAT_ALL, pData->m_ClientID, "------------------------------");
		}
		delete pData;
	}
	else if(pAction->m_pFunc == LoadPlayerHandler)
	{
		CLoadPlayerData *pData = (CLoadPlayerData*)pAction->m_pUserData;
		if(!Error)
		{
			if(pData->m_PlayerID != -1 && str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) == 0)
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
		if(!Error)
		{
			dbg_msg("sqlite", "saved score: %d", pData->m_PlayerID);
			if(pData->m_PlayerID != -1 && pData->m_ClientID != -1 && str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) == 0)
				m_aPlayerID[pData->m_ClientID] = pData->m_PlayerID;
		}
		delete pData;
	}
	else if(pAction->m_pFunc == ShowRankHandler)
	{
		CShowRankData *pData = (CShowRankData*)pAction->m_pUserData;
		if(!Error)
		{
			char aBuf[128];
			char aTime[32];
			char aRequestingPlayer[32];
			str_format(aRequestingPlayer, sizeof(aRequestingPlayer), " (by %s)", Server()->ClientName(pData->m_RequestingClientID));
			int To = g_Config.m_SvShowTimes ? -1 : pData->m_RequestingClientID;

			if(pData->m_Num > 1)
			{
				To = pData->m_RequestingClientID;
				if(pData->m_Num > 5)
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" (5 of %d) -----", pData->m_aName, pData->m_Num);
				else
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" -----", pData->m_aName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);

				for(int i = 0; i < min(5, pData->m_Num); i++)
				{
					const CRecordData *pRec = &pData->m_aRecords[i];
					IRace::FormatTimeLong(aTime, sizeof(aTime), pRec->m_Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);
					GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
				}
				GameServer()->SendChat(-1, CHAT_ALL, To, "------------------------------");
			}
			else
			{
				if(pData->m_Num == 0)
				{
					if(pData->m_PlayerID != -1)
					{
						To = pData->m_RequestingClientID;
						str_copy(aBuf, "You are not ranked", sizeof(aBuf));
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aName);
					}
				}
				else if(pData->m_Num == 1)
				{
					const CRecordData *pRec = &pData->m_aRecords[0];
					IRace::FormatTimeLong(aTime, sizeof(aTime), pRec->m_Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);
				}

				if(To == -1 && pData->m_PlayerID == -1)
					str_append(aBuf, aRequestingPlayer, sizeof(aBuf));

				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
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
	bool Error = rc != SQLITE_OK;
	if(Error)
		dbg_msg("sqlite", "can't open database: %s", sqlite3_errmsg(pDB));

	pData->m_HandlerResult = pData->m_pFunc(pDB, Error, pData->m_pUserData);
	sqlite3_close(pDB);
	return rc;
}

int CSQLiteScore::LoadPlayerHandler(sqlite3 *pDB, bool Error, void *pUser)
{
	CLoadPlayerData *pData = (CLoadPlayerData *)pUser;
	if(Error)
		return -1;

	pData->m_PlayerID = -1;
	pData->m_Time = 0;
	mem_zero((void*)pData->m_aCpTime, sizeof(pData->m_aCpTime));

	sqlite3_stmt *pStmt;
	sqlite3_prepare_v2(pDB, "SELECT * FROM players WHERE name = ?1;", -1, &pStmt, NULL);
	sqlite3_bind_text(pStmt, 1, pData->m_aPlayerName, -1, SQLITE_STATIC);
	if(sqlite3_step(pStmt) == SQLITE_ROW)
		pData->m_PlayerID = sqlite3_column_int(pStmt, 0);
	sqlite3_finalize(pStmt);

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

void CSQLiteScore::OnPlayerInit(int ClientID, bool PrintRank)
{
	m_aPlayerData[ClientID].Reset();

	if(!m_DbValid || m_MapID == -1)
		return;

	CLoadPlayerData *pData = new CLoadPlayerData();
	pData->m_ClientID = ClientID;
	str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	pData->m_MapID = m_MapID;

	StartSqlJob(LoadPlayerHandler, SQLITE_OPEN_READONLY, pData);
}

int CSQLiteScore::SaveScoreHandler(sqlite3 *pDB, bool Error, void *pUser)
{
	CSaveScoreData *pData = (CSaveScoreData *)pUser;
	if(Error)
		return -1;

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

void RecordFromRow(CRecordData *pRecord, sqlite3_stmt *pStmt)
{
	str_copy(pRecord->m_aPlayerName, (const char*)sqlite3_column_text(pStmt, 0), sizeof(pRecord->m_aPlayerName));
	pRecord->m_Time = sqlite3_column_int(pStmt, 1);
	pRecord->m_Rank = sqlite3_column_int(pStmt, 2);
}

int CSQLiteScore::ShowRankHandler(sqlite3 *pDB, bool Error, void *pUser)
{
	CShowRankData *pData = (CShowRankData *)pUser;
	if(Error)
		return -1;

	pData->m_Num = 0;

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
				if(pData->m_Num < 5)
					RecordFromRow(&pData->m_aRecords[pData->m_Num], pStmt);
				pData->m_Num++;
			}
			sqlite3_finalize(pStmt);
		}
	}

	ExecuteStatement(pDB, "DROP TABLE _ranking;");

	return 0;
}

void CSQLiteScore::ShowRank(int ClientID, const char *pName)
{
	if(!m_DbValid || m_MapID == -1)
		return;

	CShowRankData *pData = new CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = ClientID;
	pData->m_PlayerID = -1;
	str_copy(pData->m_aName, pName, sizeof(pData->m_aName));
	
	StartSqlJob(ShowRankHandler, SQLITE_OPEN_READONLY, pData);
}

void CSQLiteScore::ShowRank(int ClientID)
{
	if(!m_DbValid || m_MapID == -1)
		return;

	if(m_aPlayerID[ClientID] == -1)
	{
		GameServer()->SendChat(-1, CHAT_ALL, ClientID, "You are not ranked");
		return;
	}

	CShowRankData *pData = new CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = ClientID;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	
	StartSqlJob(ShowRankHandler, SQLITE_OPEN_READONLY, pData);
}

int CSQLiteScore::ShowTop5Handler(sqlite3 *pDB, bool Error, void *pUser)
{
	CShowTop5Data *pData = (CShowTop5Data *)pUser;
	if(Error)
		return -1;

	int Start = pData->m_Num - 1;
	pData->m_Num = 0;

	sqlite3_stmt *pStmt;
	sqlite3_prepare_v2(pDB, "SELECT name, time, RANK() OVER (ORDER BY `time` ASC) AS rank FROM (SELECT player, min(time) as time FROM races WHERE map = ?1 GROUP BY player) INNER JOIN players ON player = players.id LIMIT ?2, 5;", -1, &pStmt, NULL);
	sqlite3_bind_int(pStmt, 1, pData->m_MapID);
	sqlite3_bind_int(pStmt, 2, Start);

	while(sqlite3_step(pStmt) == SQLITE_ROW && pData->m_Num < 5)
		RecordFromRow(&pData->m_aRecords[pData->m_Num++], pStmt);
	sqlite3_finalize(pStmt);

	return 0;
}

void CSQLiteScore::ShowTop5(int ClientID, int Debut)
{
	if(!m_DbValid || m_MapID == -1)
		return;

	CShowTop5Data *pData = new CShowTop5Data();
	pData->m_ClientID = ClientID;
	pData->m_MapID = m_MapID;
	pData->m_Num = Debut;
	
	StartSqlJob(ShowTop5Handler, SQLITE_OPEN_READONLY, pData);
}
