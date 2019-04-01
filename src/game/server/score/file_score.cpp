/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/race.h>

#include "../gamecontext.h"
#include "file_score.h"

// TODO: remove score ip or use ip hash

static LOCK gs_ScoreLock = 0;

CFileScore::CPlayerScore::CPlayerScore(const char *pName, int Time, int *pCpTime)
{
	m_Time = Time;
	mem_copy(m_aCpTime, pCpTime, sizeof(m_aCpTime));
	str_copy(m_aName, pName, sizeof(m_aName));
	//str_copy(m_aIP, pIP, sizeof(m_aIP));
}

CFileScore::CFileScore(CGameContext *pGameServer) : m_pGameServer(pGameServer), m_pServer(pGameServer->Server())
{
	m_aMap[0] = 0;

	if(gs_ScoreLock == 0)
		gs_ScoreLock = lock_create();
}

CFileScore::~CFileScore()
{
	lock_wait(gs_ScoreLock);
	lock_unlock(gs_ScoreLock);
	lock_destroy(gs_ScoreLock);
}

void CFileScore::WriteEntry(IOHANDLE File, const CPlayerScore *pEntry) const
{
#if defined(CONF_FAMILY_WINDOWS)
	const char* pNewLine = "\r\n";
#else
	const char* pNewLine = "\n";
#endif

	char aBuf[1024] = { 0 };
	char aBuf2[128];
	str_append(aBuf, pEntry->m_aName, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	str_format(aBuf2, sizeof(aBuf2), "%d", pEntry->m_Time);
	str_append(aBuf, aBuf2, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	//str_append(aBuf, pEntry->m_aIP, sizeof(aBuf));
	//str_append(aBuf, pNewLine, sizeof(aBuf));
	for(int c = 0; c < NUM_CHECKPOINTS; c++)
	{
		str_format(aBuf2, sizeof(aBuf2), "%d ", pEntry->m_aCpTime[c]);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	str_append(aBuf, pNewLine, sizeof(aBuf));
	io_write(File, aBuf, str_length(aBuf));
}

IOHANDLE CFileScore::OpenFile(int Flags) const
{
	char aFilename[256];
	str_format(aFilename, sizeof(aFilename), "records/%s_record.dtb", m_aMap);
	return m_pGameServer->Storage()->OpenFile(aFilename, Flags, IStorage::TYPE_SAVE);
}

void CFileScore::SaveScoreThread(void *pUser)
{
	const CFileScore *pSelf = (CFileScore *)pUser;
	lock_wait(gs_ScoreLock);
	IOHANDLE File = pSelf->OpenFile(IOFLAG_WRITE);
	if(File)
	{
		int t = 0;
		for(sorted_array<CPlayerScore>::range r = pSelf->m_lTop.all(); !r.empty(); r.pop_front())
		{
			pSelf->WriteEntry(File, &r.front());
			t++;
			if(t%50 == 0)
				thread_sleep(1);
		}
		io_close(File);
	}
	lock_unlock(gs_ScoreLock);
}

void CFileScore::ProcessJobs(bool Block)
{
	if(m_lJobQueue.size() == 0)
		return;

	if(Block)
		lock_wait(gs_ScoreLock);
	else if(lock_trylock(gs_ScoreLock))
		return;

	for(int i = 0; i < m_lJobQueue.size(); i++)
	{
		CScoreJob *pJob = &m_lJobQueue[i];
		if(pJob->m_Type == JOBTYPE_ADD_NEW)
			m_lTop.add(pJob->m_NewData);
		else if(pJob->m_Type == JOBTYPE_UPDATE_SCORE)
		{
			pJob->m_pEntry->m_Time = pJob->m_NewData.m_Time;
			mem_copy(pJob->m_pEntry->m_aCpTime, pJob->m_NewData.m_aCpTime, sizeof(pJob->m_pEntry->m_aCpTime));
			str_copy(pJob->m_pEntry->m_aName, pJob->m_NewData.m_aName, sizeof(pJob->m_pEntry->m_aName));
			m_lTop.sort_range();
		}
		/*else if(pJob->m_Type == JOBTYPE_UPDATE_IP)
			str_copy(pJob->m_pEntry->m_aIP, pJob->m_NewData.m_aIP, sizeof(pJob->m_pEntry->m_aIP));*/
	}

	m_lJobQueue.clear();
	lock_unlock(gs_ScoreLock);

	if(Block)
		SaveScoreThread(this);
	else
	{
		void *pSaveThread = thread_init(SaveScoreThread, this);
		thread_detach(pSaveThread);
	}
}

void CFileScore::OnMapLoad()
{
	IScore::OnMapLoad();

	// process pending jobs before loading new map
	ProcessJobs(true);

	lock_wait(gs_ScoreLock);

	str_copy(m_aMap, m_pServer->GetMapName(), sizeof(m_aMap));
	m_lTop.clear();

	IOHANDLE File = OpenFile(IOFLAG_READ);
	if(File)
	{
		CLineReader LineReader;
		LineReader.Init(File);
		CPlayerScore Tmp;
		int LinesPerItem = 3;
		char *pLine;
		for(int LineCount = 0; (pLine = LineReader.Get()); LineCount++)
		{
			int Type = LineCount % LinesPerItem;
			if(Type == 0)
			{
				mem_zero(&Tmp, sizeof(Tmp));
				str_copy(Tmp.m_aName, pLine, sizeof(Tmp.m_aName));
			}
			else if(Type == 1)
			{
				Tmp.m_Time = str_toint(pLine);
			}
			/*else if(Type == 2)
			{
				str_copy(Tmp.m_aIP, pLine, sizeof(Tmp.m_aIP));
			}*/
			else if(Type == 2)
			{
				const char *pTime = pLine;
				for(int i = 0; pTime && i < NUM_CHECKPOINTS; i++)
				{
					Tmp.m_aCpTime[i] = str_toint(pTime);
					pTime = str_find(pTime, " ");
					if(pTime)
						pTime++;
				}
				m_lTop.add(Tmp);
			}
		}
		io_close(File);
	}
	lock_unlock(gs_ScoreLock);

	// save the current best score
	if(m_lTop.size())
		UpdateRecord(m_lTop[0].m_Time);
}

CFileScore::CPlayerScore *CFileScore::SearchScoreByID(int ID, int *pPosition)
{
	/*if(g_Config.m_SvScoreIP)
	{
		char aIP[16];
		Server()->GetClientAddr(ID, aIP, sizeof(aIP));

		int Pos = 1;
		for(sorted_array<CPlayerScore>::range r = m_lTop.all(); !r.empty(); r.pop_front())
		{
			if(str_comp(r.front().m_aIP, aIP) == 0)
			{
				if(pPosition)
					*pPosition = Pos;
				return &r.front();
			}
			Pos++;
		}
	}*/

	return SearchScoreByName(Server()->ClientName(ID), pPosition, true);
}

CFileScore::CPlayerScore *CFileScore::SearchScoreByName(const char *pName, int *pPosition, bool ExactMatch)
{
	CPlayerScore *pPlayer = 0;
	int Pos = 1;
	int Found = 0;
	for(sorted_array<CPlayerScore>::range r = m_lTop.all(); !r.empty(); r.pop_front())
	{
		if(str_comp(r.front().m_aName, pName) == 0)
		{
			if(pPosition)
				*pPosition = Pos;
			return &r.front();
		}
		if(!ExactMatch && str_find_nocase(r.front().m_aName, pName))
		{
			if(pPosition)
				*pPosition = Pos;
			Found++;
			pPlayer = &r.front();
		}
		Pos++;
	}
	if(Found > 1)
	{
		if(pPosition)
			*pPosition = -1;
		return 0;
	}
	return pPlayer;
}

void CFileScore::OnPlayerInit(int ClientID, bool PrintRank)
{
	m_aPlayerData[ClientID].Reset();
	CPlayerScore *pPlayer = SearchScoreByID(ClientID);

	/*CScoreJob Job;
	Job.m_Type = JOBTYPE_UPDATE_IP;
	Server()->GetClientAddr(ClientID, Job.m_NewData.m_aIP, sizeof(Job.m_NewData.m_aIP));
	if(pPlayer && str_comp(pPlayer->m_aIP, Job.m_NewData.m_aIP) != 0)
	{
		Job.m_pEntry = pPlayer;
		m_lJobQueue.add(Job);
		ProcessJobs(false);
	}*/

	// set score
	if(pPlayer)
	{
		m_aPlayerData[ClientID].SetTime(pPlayer->m_Time, pPlayer->m_aCpTime);
		if(g_Config.m_SvLoadBest)
			m_aPlayerData[ClientID].UpdateCurTime(pPlayer->m_Time);
	}
}

void CFileScore::OnPlayerFinish(int ClientID, int Time, int *pCpTime)
{
	bool NewPlayerRecord = m_aPlayerData[ClientID].UpdateTime(Time, pCpTime);
	UpdateRecord(Time);

	/*if(NewRecord && g_Config.m_SvShowTimes)
		GameServer()->SendRecord(-1);*/

	if(!NewPlayerRecord)
		return;

	CScoreJob Job;
	Job.m_NewData = CPlayerScore(Server()->ClientName(ClientID), Time, pCpTime);
	//Server()->GetClientAddr(ClientID, Job.m_NewData.m_aIP, sizeof(Job.m_NewData.m_aIP));

	Job.m_pEntry = SearchScoreByID(ClientID);
	if(Job.m_pEntry)
		Job.m_Type = JOBTYPE_UPDATE_SCORE;
	else
		Job.m_Type = JOBTYPE_ADD_NEW;

	m_lJobQueue.add(Job);
	ProcessJobs(false);
}

void CFileScore::ShowTop5(int ClientID, int Debut)
{
	if(CheckSpamProtection(ClientID))
		return;

	char aBuf[512];
	char aTime[64];
	GameServer()->SendChat(-1, CHAT_ALL, ClientID, "----------- Top 5 -----------");
	for(int i = 0; i < 5 && i + Debut - 1 < m_lTop.size(); i++)
	{
		const CPlayerScore *r = &m_lTop[i+Debut-1];
		IRace::FormatTimeLong(aTime, sizeof(aTime), r->m_Time);
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", i + Debut, r->m_aName, aTime);
		GameServer()->SendChat(-1, CHAT_ALL, ClientID, aBuf);
	}
	GameServer()->SendChat(-1, CHAT_ALL, ClientID, "------------------------------");
	if(m_lTop.size() > 5)
	{
		str_format(aBuf, sizeof(aBuf), "Total records : %d", m_lTop.size());
		GameServer()->SendChat(-1, CHAT_ALL, ClientID, aBuf);
	}
	m_LastPrintInChat[ClientID] = Server()->Tick();
}

void CFileScore::ShowRank(int ClientID, const char *pName)
{
	if(CheckSpamProtection(ClientID))
		return;

	int Pos = 0;
	char aBuf[512];

	CPlayerScore *pScore = SearchScoreByName(pName, &Pos, false);

	int To = ClientID;

	if(pScore && Pos > -1)
	{
		if(g_Config.m_SvShowTimes)
			To = -1;
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), pScore->m_Time);
		if(To != -1)
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", Pos, pScore->m_aName, aTime);
		else
			str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s (%s)", Pos, pScore->m_aName, aTime, Server()->ClientName(ClientID));
	}
	else if(Pos == -1)
		str_format(aBuf, sizeof(aBuf), "Several players were found.");
	else
		str_format(aBuf, sizeof(aBuf), "%s is not ranked", pName);

	GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
	m_LastPrintInChat[ClientID] = Server()->Tick();
}

void CFileScore::ShowRank(int ClientID)
{
	if(CheckSpamProtection(ClientID))
		return;

	int To = ClientID;
	int Pos = 0;
	char aBuf[512];

	const CPlayerScore *pScore = SearchScoreByID(ClientID, &Pos);

	if(pScore && Pos > -1)
	{
		if(g_Config.m_SvShowTimes)
			To = -1;
		char aTime[64];
		IRace::FormatTimeLong(aTime, sizeof(aTime), pScore->m_Time);
		str_format(aBuf, sizeof(aBuf), "%d. %s Time: %s", Pos, pScore->m_aName, aTime);
	}
	else if(Pos == -1)
		str_format(aBuf, sizeof(aBuf), "Several players were found.");
	else
		str_format(aBuf, sizeof(aBuf), "You are not ranked");

	GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
	m_LastPrintInChat[ClientID] = Server()->Tick();
}

bool CFileScore::CheckSpamProtection(int ClientID)
{
	return g_Config.m_SvSpamprotection && m_LastPrintInChat[ClientID] &&
		m_LastPrintInChat[ClientID]+Server()->TickSpeed()*3 > Server()->Tick();
}
