#ifdef CONF_SQLITE
#include <sqlite3.h>

#include <engine/shared/config.h>
#include <engine/engine.h>
#include <engine/storage.h>

#include "sqlite_score.h"

// TODO:
// add mysql
// better error handling
// display last runs (timestamps)
// total records for top5
// handle gametypes

char CSQLiteScore::m_aDBFilename[512];

bool ExecuteSQL(sqlite3 *pDB, const char *pSQL)
{
	char *pErrorMsg = 0;
	bool Success = sqlite3_exec(pDB, pSQL, NULL, 0, &pErrorMsg) == SQLITE_OK;
	if(!Success)
	{
		dbg_msg("sqlite", "error running statements: %s", pErrorMsg);
		sqlite3_free(pErrorMsg);
	}
    return Success;
}

void RecordFromRow(IScoreBackend::CRecordData *pRecord, sqlite3_stmt *pStmt)
{
	str_copy(pRecord->m_aPlayerName, (const char*)sqlite3_column_text(pStmt, 0), sizeof(pRecord->m_aPlayerName));
	pRecord->m_Time = sqlite3_column_int(pStmt, 1);
	pRecord->m_Rank = sqlite3_column_int(pStmt, 2);
}

CSQLiteScore::CSQLiteScore(IScoreResponseListener *pListener, IEngine *pEngine, IStorage *pStorage, const char *pDatabase)
	: IScoreBackend(pListener), m_pEngine(pEngine), m_pStorage(pStorage), m_DBValid(false)
{
	char m_aFile[512];
	str_format(m_aFile, sizeof(m_aFile), "records/%s", pDatabase);
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, m_aFile, m_aDBFilename, sizeof(m_aDBFilename));

	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

	CSqlExecData ExecData(-1, CreateTablesHandler, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
	int rc = ExecSqlFunc(&ExecData);
	if(rc == SQLITE_OK && ExecData.m_HandlerResult == 0)
		m_DBValid = true;
}

CSQLiteScore::~CSQLiteScore()
{
}

void CSQLiteScore::Tick()
{
	for(int i = 0; i < m_lPendingRequests.size(); i++)
	{
		CSqlExecData *pRequest = m_lPendingRequests[i];
		if(pRequest->m_Job.Status() == CJob::STATE_DONE)
		{
			m_lPendingRequests.remove_index(i--);
			bool Error = pRequest->m_Job.Result() != SQLITE_OK || pRequest->m_HandlerResult != 0;
			Listener()->OnRequestFinished(pRequest->m_Type, pRequest->m_pRequestData, Error);
			delete pRequest;
		}
	}
}

void CSQLiteScore::AddRequest(int Type, CRequestData *pRequestData)
{
	CSqlExecData *pRequest = 0;
	switch(Type)
	{
	case REQTYPE_LOAD_MAP:
		pRequest = new CSqlExecData(Type, LoadMapHandler, SQLITE_OPEN_READWRITE, pRequestData);
		break;
	case REQTYPE_LOAD_PLAYER:
		pRequest = new CSqlExecData(Type, LoadPlayerHandler, SQLITE_OPEN_READONLY, pRequestData);
		break;
	case REQTYPE_SAVE_SCORE:
		pRequest = new CSqlExecData(Type, SaveScoreHandler, SQLITE_OPEN_READWRITE, pRequestData);
		break;
	case REQTYPE_SHOW_RANK:
		pRequest = new CSqlExecData(Type, ShowRankHandler, SQLITE_OPEN_READONLY, pRequestData);
		break;
	case REQTYPE_SHOW_TOP5:
		pRequest = new CSqlExecData(Type, ShowTop5Handler, SQLITE_OPEN_READONLY, pRequestData);
		break;
	default:
		Listener()->OnRequestFinished(Type, pRequestData, true);
		return;
	}
	m_lPendingRequests.add(pRequest);
	m_pEngine->AddJob(&pRequest->m_Job, ExecSqlFunc, pRequest);
}

int CSQLiteScore::ExecSqlFunc(void *pUser)
{
	CSqlExecData *pData = (CSqlExecData *)pUser;

	sqlite3 *pDB;
	int rc = sqlite3_open_v2(m_aDBFilename, &pDB, pData->m_Flags, NULL);
	if(rc == SQLITE_OK)
		pData->m_HandlerResult = pData->m_pfnFunc(pDB, pData->m_pRequestData);
	else
		dbg_msg("sqlite", "can't open database: %s", sqlite3_errmsg(pDB));

	sqlite3_close(pDB);
	return rc;
}

int CSQLiteScore::CreateTablesHandler(sqlite3 *pDB, CRequestData *pRequestData)
{
	const char *pInitSQL = "PRAGMA foreign_keys = ON;"
		"CREATE TABLE IF NOT EXISTS maps (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, gametype TEXT);"
		"CREATE TABLE IF NOT EXISTS players (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
		"CREATE TABLE IF NOT EXISTS races (player INTEGER REFERENCES players, map INTEGER REFERENCES maps, time INTEGER, checkpoints TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);"
		"CREATE INDEX IF NOT EXISTS raceindex ON races (map, player);";

	return ExecuteSQL(pDB, pInitSQL) ? 0 : -1;
}

int CSQLiteScore::LoadMapHandler(sqlite3 *pDB, CRequestData *pRequestData)
{
	CMapData *pData = (CMapData*)pRequestData;
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

int CSQLiteScore::LoadPlayerHandler(sqlite3 *pDB, CRequestData *pRequestData)
{
	CScoreData *pData = (CScoreData*)pRequestData;
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
			const char *pCpStr = (const char*)sqlite3_column_text(pStmt, 1);
			IScoreBackend::CheckpointsFromString(pData->m_aCpTime, pCpStr);
		}
		sqlite3_finalize(pStmt);
	}
			
	return 0;
}

int CSQLiteScore::SaveScoreHandler(sqlite3 *pDB, CRequestData *pRequestData)
{
	CScoreData *pData = (CScoreData*)pRequestData;
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
		char aCp[1024] = {0};
		IScoreBackend::CheckpointsToString(aCp, sizeof(aCp), pData->m_aCpTime);
		sqlite3_bind_text(pStmt, 4, aCp, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_DONE)
			dbg_msg("sqlite", "added race: %d %d %d", pData->m_MapID, pData->m_PlayerID, pData->m_Time);
		sqlite3_finalize(pStmt);
	}
			
	return 0;
}

int CSQLiteScore::ShowRankHandler(sqlite3 *pDB, CRequestData *pRequestData)
{
	CRankData *pData = (CRankData*)pRequestData;
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
		sqlite3_bind_text(pStmt, 1, pData->m_aSearchName, -1, SQLITE_STATIC);
		if(sqlite3_step(pStmt) == SQLITE_ROW)
			RecordFromRow(&pData->m_aRecords[pData->m_Num++], pStmt);
		sqlite3_finalize(pStmt);

		if(pData->m_Num == 0)
		{
			sqlite3_prepare_v2(pDB, "SELECT name, time, rank FROM _ranking INNER JOIN players ON player = players.id WHERE name LIKE '%' || ?1 || '%' LIMIT ?2;", -1, &pStmt, NULL);
			sqlite3_bind_text(pStmt, 1, pData->m_aSearchName, -1, SQLITE_STATIC);
			sqlite3_bind_int(pStmt, 2, MAX_SEARCH_RECORDS + 1);
			while(sqlite3_step(pStmt) == SQLITE_ROW && pData->m_Num <= MAX_SEARCH_RECORDS)
			{
				if(pData->m_Num < MAX_SEARCH_RECORDS)
					RecordFromRow(&pData->m_aRecords[pData->m_Num], pStmt);
				pData->m_Num++;
			}
			sqlite3_finalize(pStmt);
		}
	}

	ExecuteSQL(pDB, "DROP TABLE _ranking;");

	return 0;
}

int CSQLiteScore::ShowTop5Handler(sqlite3 *pDB, CRequestData *pRequestData)
{
	CRankData *pData = (CRankData*)pRequestData;
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

#endif
