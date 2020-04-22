#include <engine/shared/config.h>

#include <game/race.h>

#include "gamecontext.h"
#include "score/file_score.h"
#ifdef CONF_SQLITE
#include "score/sqlite_score.h"
#endif

#include "score.h"

void IScoreBackend::CheckpointsFromString(int *pCpTime, const char *pStr, const char *pDelim)
{
	for(int i = 0; pStr && i < NUM_CHECKPOINTS; i++)
	{
		pCpTime[i] = str_toint(pStr);
		pStr = str_find(pStr, pDelim);
		if(pStr)
			pStr++;
	}
}

void IScoreBackend::CheckpointsToString(char *pBuf, int BufSize, const int *pCpTime, const char *pDelim)
{
	pBuf[0] = 0;
	char aTime[16];
	int End = NUM_CHECKPOINTS-1;
	for(; End > 0 && pCpTime[End] == 0; End--);
	for(int i = 0; i <= End; i++)
	{
		if(i > 0)
			str_append(pBuf, pDelim, BufSize);
		str_format(aTime, sizeof(aTime), "%d", pCpTime[i]);
		str_append(pBuf, aTime, BufSize);
	}
}

CScore::CScore(CGameContext *pGameServer) :
	m_pGameServer(pGameServer),
	m_pServer(pGameServer->Server()),
	m_MapID(-1),
	m_CurrentRecord(0)
{
	if(str_comp(GameServer()->Config()->m_SvScore, "file") == 0)
		m_pBackend = new CFileScore(this, GameServer()->Storage());
#ifdef CONF_SQLITE
	else if(str_comp(GameServer()->Config()->m_SvScore, "sqlite") == 0)
		m_pBackend = new CSQLiteScore(this, GameServer()->Engine(), GameServer()->Storage(), GameServer()->Config()->m_SvSQLiteDatabase);
#endif
	else
		m_pBackend = new CFileScore(this, GameServer()->Storage());
}

void CScore::UpdateThrottling(int ClientID)
{
	m_LastRequest[ClientID] = Server()->Tick();
}

bool CScore::IsThrottled(int ClientID)
{
	return m_LastRequest[ClientID] != -1 && m_LastRequest[ClientID] + Server()->TickSpeed() * 3 > Server()->Tick();
}

bool CScore::UpdateRecord(int Time)
{
	bool NewRecord = m_CurrentRecord == 0 || Time < m_CurrentRecord;
	if(NewRecord)
		m_CurrentRecord = Time;
	return NewRecord;
}

void CScore::SendFinish(int ID, int FinishTime, int Diff, bool RecordPersonal, bool RecordServer, int To)
{
	CNetMsg_Sv_RaceFinish FinishMsg;
	FinishMsg.m_ClientID = ID;
	FinishMsg.m_Time = FinishTime;
	FinishMsg.m_Diff = Diff;
	FinishMsg.m_RecordPersonal = RecordPersonal;
	FinishMsg.m_RecordServer = RecordServer;
	Server()->SendPackMsg(&FinishMsg, MSGFLAG_VITAL, To);

	char aBuf[256];
	char aTime[32];
	IRace::FormatTime(aTime, sizeof(aTime), FinishTime, 3);

	if(RecordPersonal || RecordServer)
	{
		if(RecordServer)
			str_format(aBuf, sizeof(aBuf), "'%s' has set a new map record: %s", Server()->ClientName(ID), aTime);
		else // RecordPersonal
			str_format(aBuf, sizeof(aBuf), "'%s' has set a new personal record: %s", Server()->ClientName(ID), aTime);
		
		if(Diff < 0)
		{
			char aImprovement[64];
			IRace::FormatTimeDiff(aTime, sizeof(aTime), absolute(Diff), 3, false);
			str_format(aImprovement, sizeof(aImprovement), " (%s seconds faster)", aTime);
			str_append(aBuf, aImprovement, sizeof(aBuf));
		}
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "'%s' finished in: %s", Server()->ClientName(ID), aTime);
	}

	if(To == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && Server()->ClientIngame(i) && Server()->GetClientVersion(i) < CGameContext::MIN_RACE_CLIENTVERSION)
				GameServer()->SendChat(-1, CHAT_ALL, i, aBuf);
		}
	}
	else if(Server()->GetClientVersion(To) < CGameContext::MIN_RACE_CLIENTVERSION)
	{
		GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
	}
}

void CScore::OnRequestFinished(int Type, IScoreBackend::CRequestData *pUserData, bool Error)
{
	bool MapChanged = m_MapID != pUserData->m_MapID;
	if(Error)
	{
		delete pUserData;
		return;
	}

	if(Type == IScoreBackend::REQTYPE_LOAD_MAP)
	{
		IScoreBackend::CMapData *pData = (IScoreBackend::CMapData*)pUserData;
		MapChanged = str_comp(pData->m_aMapName, Server()->GetMapName()) != 0;
		if(!MapChanged)
		{
			m_MapID = pData->m_MapID;
			UpdateRecord(pData->m_BestTime);

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(Server()->ClientIngame(i))
					OnPlayerInit(i);
			}
		}
	}
	else if(Type == IScoreBackend::REQTYPE_LOAD_PLAYER)
	{
		IScoreBackend::CScoreData *pData = (IScoreBackend::CScoreData*)pUserData;
		int ClientID = Server()->GetClientIDFromConnID(pData->m_ConnID);
		if(ClientID != -1 && Server()->GetPlayerID(ClientID) == -1 && pData->m_PlayerID != -1)
		{
			dbg_msg("score", "player init: %d:%d", ClientID, pData->m_PlayerID);
			Server()->SetPlayerID(ClientID, pData->m_PlayerID);
		}

		if(!MapChanged && ClientID != -1 && pData->m_Time > 0)
		{
			dbg_msg("score", "loaded player time: %d (%d)", ClientID, pData->m_Time);
			m_aPlayerCache[ClientID].SetTime(pData->m_Time, pData->m_aCpTime);
			if(GameServer()->Config()->m_SvLoadBest)
				m_aPlayerCache[ClientID].UpdateCurTime(pData->m_Time);
		}
	}
	else if(Type == IScoreBackend::REQTYPE_SAVE_SCORE)
	{
		IScoreBackend::CScoreData *pData = (IScoreBackend::CScoreData*)pUserData;
		int ClientID = Server()->GetClientIDFromConnID(pData->m_ConnID);
		if(ClientID != -1 && Server()->GetPlayerID(ClientID) == -1 && pData->m_PlayerID != -1)
		{
			dbg_msg("score", "player init: %d:%d", ClientID, pData->m_PlayerID);
			Server()->SetPlayerID(ClientID, pData->m_PlayerID);
		}
		if(ClientID != -1)
		{
			dbg_msg("score", "saved time: %d (%d)", ClientID, pData->m_Time);
			// TODO: check backend instead of cache?
			int Diff = m_aPlayerCache[ClientID].m_Time == 0 ? 0 : pData->m_Time - m_aPlayerCache[ClientID].m_Time;
			bool RecordPersonal = m_aPlayerCache[ClientID].UpdateTime(pData->m_Time, pData->m_aCpTime);
			bool RecordServer = UpdateRecord(pData->m_Time);

			SendFinish(ClientID, pData->m_Time, Diff, RecordPersonal, RecordServer,
				GameServer()->Config()->m_SvShowTimes ? -1 : ClientID);
		}
	}
	else if(Type == IScoreBackend::REQTYPE_SHOW_RANK)
	{
		IScoreBackend::CRankData *pData = (IScoreBackend::CRankData*)pUserData;
		int RequestingClientID = Server()->GetClientIDFromConnID(pData->m_RequestingConnID);
		if(!MapChanged && RequestingClientID != -1)
		{
			char aBuf[128];
			char aTime[32];
			int To = RequestingClientID;
			bool Own = str_comp(Server()->ClientName(RequestingClientID), pData->m_aSearchName) == 0;

			if(pData->m_Num > 1)
			{
				if(pData->m_Num > IScoreBackend::MAX_SEARCH_RECORDS)
					str_format(aBuf, sizeof(aBuf), "----- First %d results for \"%s\" -----", IScoreBackend::MAX_SEARCH_RECORDS, pData->m_aSearchName);
				else
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" -----", pData->m_aSearchName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);

				for(int i = 0; i < min((int)IScoreBackend::MAX_SEARCH_RECORDS, pData->m_Num); i++)
				{
					const IScoreBackend::CRecordData *pRec = &pData->m_aRecords[i];
					IRace::FormatTime(aTime, sizeof(aTime), pRec->m_Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);
					GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
				}
				GameServer()->SendChat(-1, CHAT_ALL, To, "------------------------------");
			}
			else if(pData->m_Num == 1)
			{
				const IScoreBackend::CRecordData *pRec = &pData->m_aRecords[0];
				IRace::FormatTime(aTime, sizeof(aTime), pRec->m_Time);
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);

				if(GameServer()->Config()->m_SvShowTimes)
				{
					To = -1;
					if(!Own)
					{
						char aBuf2[32];
						str_format(aBuf2, sizeof(aBuf2), " (requested by %s)", Server()->ClientName(RequestingClientID));
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
					str_format(aBuf, sizeof(aBuf), "%s is not ranked", pData->m_aSearchName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
			}
		}
	}
	else if(Type == IScoreBackend::REQTYPE_SHOW_TOP5)
	{
		IScoreBackend::CRankData *pData = (IScoreBackend::CRankData*)pUserData;
		int RequestingClientID = Server()->GetClientIDFromConnID(pData->m_RequestingConnID);
		if(!MapChanged && RequestingClientID != -1)
		{
			char aBuf[128];
			char aTime[32];
			GameServer()->SendChat(-1, CHAT_ALL, RequestingClientID, "----------- Top 5 -----------");
			for(int i = 0; i < min((int)IScoreBackend::MAX_TOP_RECORDS, pData->m_Num); i++)
			{
				IRace::FormatTime(aTime, sizeof(aTime), pData->m_aRecords[i].m_Time);
				str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s",
					pData->m_aRecords[i].m_Rank, pData->m_aRecords[i].m_aPlayerName, aTime);
				GameServer()->SendChat(-1, CHAT_ALL, RequestingClientID, aBuf);
			}
			GameServer()->SendChat(-1, CHAT_ALL, RequestingClientID, "------------------------------");
			if(pData->m_TotalEntries > IScoreBackend::MAX_TOP_RECORDS)
			{
				str_format(aBuf, sizeof(aBuf), "Total records: %d", pData->m_TotalEntries);
				GameServer()->SendChat(-1, CHAT_ALL, RequestingClientID, aBuf);
			}
		}
	}

	delete pUserData;
}

void CScore::OnMapLoad()
{
	m_MapID = -1;
	m_CurrentRecord = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aPlayerCache[i].Reset();
		m_LastRequest[i] = -1;
	}

	if(!m_pBackend->Ready())
		return;

	IScoreBackend::CMapData *pData = new IScoreBackend::CMapData();
	str_copy(pData->m_aMapName, Server()->GetMapName(), sizeof(pData->m_aMapName));

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_LOAD_MAP, pData);
}

void CScore::OnPlayerInit(int ClientID)
{
	m_aPlayerCache[ClientID].Reset();

	if(!m_pBackend->Ready() || m_MapID == -1)
		return;

	IScoreBackend::CScoreData *pData = new IScoreBackend::CScoreData();
	pData->m_MapID = m_MapID;
	pData->m_ConnID = Server()->GetConnID(ClientID);
	str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	pData->m_PlayerID = Server()->GetPlayerID(ClientID);

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_LOAD_PLAYER, pData);
}

void CScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	if(!m_pBackend->Ready() || m_MapID == -1)
		return;

	IScoreBackend::CScoreData *pData = new IScoreBackend::CScoreData();
	pData->m_MapID = m_MapID;
	pData->m_ConnID = Server()->GetConnID(ClientID);
	str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	pData->m_PlayerID = Server()->GetPlayerID(ClientID);
	pData->m_Time = Time;
	mem_copy(pData->m_aCpTime, pCpTime, sizeof(pData->m_aCpTime));

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SAVE_SCORE, pData);
}

void CScore::ShowRank(int RequestingClientID, const char *pName)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp(Server()->ClientName(i), pName) == 0)
		{
			ShowRank(RequestingClientID, i);
			return;
		}
	}

	UpdateThrottling(RequestingClientID);

	IScoreBackend::CRankData *pData = new IScoreBackend::CRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingConnID = Server()->GetConnID(RequestingClientID);
	str_copy(pData->m_aSearchName, pName, sizeof(pData->m_aSearchName));

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_RANK, pData);
}

void CScore::ShowRank(int RequestingClientID, int ClientID)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	UpdateThrottling(RequestingClientID);

	IScoreBackend::CRankData *pData = new IScoreBackend::CRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingConnID = Server()->GetConnID(RequestingClientID);
	str_copy(pData->m_aSearchName, Server()->ClientName(ClientID), sizeof(pData->m_aSearchName));
	pData->m_PlayerID = Server()->GetPlayerID(ClientID);

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_RANK, pData);
}

void CScore::ShowTop5(int RequestingClientID, int Debut)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	UpdateThrottling(RequestingClientID);

	IScoreBackend::CRankData *pData = new IScoreBackend::CRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingConnID = Server()->GetConnID(RequestingClientID);
	pData->m_Start = Debut - 1;

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_TOP5, pData);
}
