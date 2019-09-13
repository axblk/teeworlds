#include <engine/shared/config.h>

#include <game/race.h>

#include "gamecontext.h"
#include "score/file_score.h"
#include "score/sqlite_score.h"

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
	if(str_comp(g_Config.m_SvScore, "file") == 0)
		m_pBackend = new CFileScore(this, GameServer()->Storage());
	else
		m_pBackend = new CSQLiteScore(this, GameServer()->Engine(), GameServer()->Storage());
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
	bool Check = !m_CurrentRecord || Time < m_CurrentRecord;
	if(Check)
		m_CurrentRecord = Time;
	return Check;
}

void CScore::OnRequestFinished(int Type, IScoreBackend::CRequestData *pUserData, bool Error)
{
	bool MapChanged = m_MapID != pUserData->m_MapID;

	if(Type == IScoreBackend::REQTYPE_LOAD_MAP)
	{
		IScoreBackend::CLoadMapData *pData = (IScoreBackend::CLoadMapData*)pUserData;
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
	}
	else if(Type == IScoreBackend::REQTYPE_LOAD_PLAYER)
	{
		IScoreBackend::CLoadPlayerData *pData = (IScoreBackend::CLoadPlayerData*)pUserData;
		bool ClientChanged = !Server()->ClientIngame(pData->m_ClientID) || str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) != 0;
		if(!Error && !MapChanged && !ClientChanged)
		{
			if(pData->m_PlayerID != -1)
			{
				dbg_msg("score", "player init: %d:%d", pData->m_ClientID, pData->m_PlayerID);
				m_aPlayerID[pData->m_ClientID] = pData->m_PlayerID;
				m_aPlayerCache[pData->m_ClientID].SetTime(pData->m_Time, pData->m_aCpTime);
				if(g_Config.m_SvLoadBest)
					m_aPlayerCache[pData->m_ClientID].UpdateCurTime(pData->m_Time);
			}
		}
	}
	else if(Type == IScoreBackend::REQTYPE_SAVE_SCORE)
	{
		IScoreBackend::CSaveScoreData *pData = (IScoreBackend::CSaveScoreData*)pUserData;
		bool ClientChanged = !Server()->ClientIngame(pData->m_ClientID) || str_comp(Server()->ClientName(pData->m_ClientID), pData->m_aPlayerName) != 0;
		if(!Error)
		{
			dbg_msg("score", "saved score: %d", pData->m_PlayerID);
			if(!MapChanged && !ClientChanged && pData->m_PlayerID != -1)
			{
				dbg_msg("score", "player init: %d:%d", pData->m_ClientID, pData->m_PlayerID);
				m_aPlayerID[pData->m_ClientID] = pData->m_PlayerID;
			}
		}
	}
	else if(Type == IScoreBackend::REQTYPE_SHOW_RANK)
	{
		IScoreBackend::CShowRankData *pData = (IScoreBackend::CShowRankData*)pUserData;
		bool ClientLeft = !Server()->ClientIngame(pData->m_RequestingClientID);
		if(!Error && !MapChanged && !ClientLeft)
		{
			char aBuf[128];
			char aTime[32];
			int To = pData->m_RequestingClientID;
			bool Own = pData->m_PlayerID != -1 && pData->m_PlayerID == m_aPlayerID[pData->m_RequestingClientID];

			if(pData->m_Num > 1)
			{
				if(pData->m_Num > IScoreBackend::MAX_SEARCH_RECORDS)
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" (%d of %d) -----", pData->m_aName, IScoreBackend::MAX_SEARCH_RECORDS, pData->m_Num);
				else
					str_format(aBuf, sizeof(aBuf), "----- Results for \"%s\" -----", pData->m_aName);
				GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);

				for(int i = 0; i < min((int)IScoreBackend::MAX_SEARCH_RECORDS, pData->m_Num); i++)
				{
					const IScoreBackend::CRecordData *pRec = &pData->m_aRecords[i];
					IRace::FormatTimeLong(aTime, sizeof(aTime), pRec->m_Time);
					str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", pRec->m_Rank, pRec->m_aPlayerName, aTime);
					GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
				}
				GameServer()->SendChat(-1, CHAT_ALL, To, "------------------------------");
			}
			else if(pData->m_Num == 1)
			{
				const IScoreBackend::CRecordData *pRec = &pData->m_aRecords[0];
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
	}
	else if(Type == IScoreBackend::REQTYPE_SHOW_TOP5)
	{
		IScoreBackend::CShowTop5Data *pData = (IScoreBackend::CShowTop5Data*)pUserData;
		bool ClientLeft = !Server()->ClientIngame(pData->m_RequestingClientID);
		if(!Error && !MapChanged && !ClientLeft)
		{
			char aBuf[128];
			char aTime[32];
			GameServer()->SendChat(-1, CHAT_ALL, pData->m_RequestingClientID, "----------- Top 5 -----------");
			for(int i = 0; i < min((int)IScoreBackend::MAX_TOP_RECORDS, pData->m_Num); i++)
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
		m_aPlayerID[i] = -1;
	}

	if(!m_pBackend->Ready())
		return;

	IScoreBackend::CLoadMapData *pData = new IScoreBackend::CLoadMapData();
	str_copy(pData->m_aMapName, Server()->GetMapName(), sizeof(pData->m_aMapName));
	pData->m_MapID = -1;
	pData->m_BestTime = 0;

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_LOAD_MAP, pData);
}

void CScore::OnPlayerInit(int ClientID)
{
	m_aPlayerCache[ClientID].Reset();

	if(!m_pBackend->Ready() || m_MapID == -1)
		return;

	IScoreBackend::CLoadPlayerData *pData = new IScoreBackend::CLoadPlayerData();
	pData->m_ClientID = ClientID;
	str_copy(pData->m_aPlayerName, Server()->ClientName(ClientID), sizeof(pData->m_aPlayerName));
	pData->m_MapID = m_MapID;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	pData->m_Time = 0;
	mem_zero((void*)pData->m_aCpTime, sizeof(pData->m_aCpTime));

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_LOAD_PLAYER, pData);
}

void CScore::OnPlayerLeave(int ClientID)
{
	m_aPlayerID[ClientID] = -1;
}

void CScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	m_aPlayerCache[ClientID].UpdateTime(Time, pCpTime);
	UpdateRecord(Time);

	if(!m_pBackend->Ready() || m_MapID == -1)
		return;

	IScoreBackend::CSaveScoreData *pData = new IScoreBackend::CSaveScoreData();
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

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SAVE_SCORE, pData);
}

void CScore::ShowRank(int RequestingClientID, const char *pName)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
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

	IScoreBackend::CShowRankData *pData = new IScoreBackend::CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = RequestingClientID;
	pData->m_PlayerID = -1;
	str_copy(pData->m_aName, pName, sizeof(pData->m_aName));
	pData->m_Num = 0;

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_RANK, pData);
}

void CScore::ShowRank(int RequestingClientID, int ClientID)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
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

	IScoreBackend::CShowRankData *pData = new IScoreBackend::CShowRankData();
	pData->m_MapID = m_MapID;
	pData->m_RequestingClientID = ClientID;
	pData->m_PlayerID = m_aPlayerID[ClientID];
	pData->m_Num = 0;

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_RANK, pData);
}

void CScore::ShowTop5(int RequestingClientID, int Debut)
{
	if(!m_pBackend->Ready() || m_MapID == -1 || IsThrottled(RequestingClientID))
		return;

	UpdateThrottling(RequestingClientID);

	IScoreBackend::CShowTop5Data *pData = new IScoreBackend::CShowTop5Data();
	pData->m_RequestingClientID = RequestingClientID;
	pData->m_MapID = m_MapID;
	pData->m_Start = Debut - 1;
	pData->m_Num = 0;
	pData->m_TotalRecords = 0;

	m_pBackend->AddRequest(IScoreBackend::REQTYPE_SHOW_TOP5, pData);
}
