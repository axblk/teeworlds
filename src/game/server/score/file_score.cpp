/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include "file_score.h"

static LOCK gs_ScoreLock = 0;

CFileScore::CPlayerScore::CPlayerScore(const char *pName, int Time, int *pCpTime)
{
	m_Time = Time;
	mem_copy(m_aCpTime, pCpTime, sizeof(m_aCpTime));
	str_copy(m_aName, pName, sizeof(m_aName));
}

CFileScore::CFileScore(IScoreResponseListener *pListener, IStorage *pStorage) : IScoreBackend(pListener), m_pStorage(pStorage)
{
	m_MapID = 0;
	m_aMap[0] = 0;
	m_PlayerCounter = 0;

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
	char aBuf2[1024];
	str_append(aBuf, pEntry->m_aName, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	str_format(aBuf2, sizeof(aBuf2), "%d", pEntry->m_Time);
	str_append(aBuf, aBuf2, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	IScoreBackend::CheckpointsToString(aBuf2, sizeof(aBuf2), pEntry->m_aCpTime, " ");
	str_append(aBuf, aBuf2, sizeof(aBuf));
	str_append(aBuf, pNewLine, sizeof(aBuf));
	io_write(File, aBuf, str_length(aBuf));
}

IOHANDLE CFileScore::OpenFile(int Flags) const
{
	char aFilename[256];
	str_format(aFilename, sizeof(aFilename), "records/%s_record.dtb", m_aMap);
	return m_pStorage->OpenFile(aFilename, Flags, IStorage::TYPE_SAVE);
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
		{
			m_lTop.add_unsorted(pJob->m_NewData);
		}
		else if(pJob->m_Type == JOBTYPE_UPDATE_SCORE)
		{
			pJob->m_pEntry->m_Time = pJob->m_NewData.m_Time;
			mem_copy(pJob->m_pEntry->m_aCpTime, pJob->m_NewData.m_aCpTime, sizeof(pJob->m_pEntry->m_aCpTime));
		}
	}

	m_lTop.sort_range();

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

CFileScore::CPlayerScore *CFileScore::SearchScoreByPlayerID(int PlayerID, int *pPosition)
{
	int Pos = 1;
	for(sorted_array<CPlayerScore>::range r = m_lTop.all(); !r.empty(); r.pop_front(), Pos++)
	{
		if(r.front().m_ID == PlayerID)
		{
			if(pPosition)
				*pPosition = Pos;
			return &r.front();
		}
	}
	return 0;
}

CFileScore::CPlayerScore *CFileScore::SearchScoreByName(const char *pName, int *pPosition)
{
	int Pos = 1;
	for(sorted_array<CPlayerScore>::range r = m_lTop.all(); !r.empty(); r.pop_front(), Pos++)
	{
		if(str_comp(r.front().m_aName, pName) == 0)
		{
			if(pPosition)
				*pPosition = Pos;
			return &r.front();
		}
	}
	return 0;
}

void CFileScore::AddRequest(int Type, CRequestData *pRequestData)
{
	int Result = -1;
	if(Type == REQTYPE_LOAD_MAP)
		Result = LoadMapHandler((CLoadMapData*)pRequestData);
	else if(Type == REQTYPE_LOAD_PLAYER)
		Result = LoadPlayerHandler((CLoadPlayerData*)pRequestData);
	else if(Type == REQTYPE_SAVE_SCORE)
		Result = SaveScoreHandler((CSaveScoreData*)pRequestData);
	else if(Type == REQTYPE_SHOW_RANK)
		Result = ShowRankHandler((CShowRankData*)pRequestData);
	else if(Type == REQTYPE_SHOW_TOP5)
		Result = ShowTop5Handler((CShowTop5Data*)pRequestData);

	bool Error = Result != 0;
	Listener()->OnRequestFinished(Type, pRequestData, Error);
}

int CFileScore::LoadMapHandler(CLoadMapData *pData)
{
	ProcessJobs(true);

	lock_wait(gs_ScoreLock);

	m_MapID++;
	pData->m_MapID = m_MapID;
	str_copy(m_aMap, pData->m_aMapName, sizeof(m_aMap));
	m_PlayerCounter = 0;
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
			else if(Type == 2)
			{
				IScoreBackend::CheckpointsFromString(Tmp.m_aCpTime, pLine, " ");
				Tmp.m_ID = ++m_PlayerCounter;
				m_lTop.add(Tmp);
			}
		}
		io_close(File);
	}
	lock_unlock(gs_ScoreLock);

	if(m_lTop.size())
		pData->m_BestTime = m_lTop[0].m_Time;

	return 0;
}

int CFileScore::LoadPlayerHandler(CLoadPlayerData *pData)
{
	const CPlayerScore *pPlayer = (pData->m_PlayerID != -1)
		? SearchScoreByPlayerID(pData->m_PlayerID)
		: SearchScoreByName(pData->m_aPlayerName);

	if(pPlayer)
	{
		if(pData->m_PlayerID == -1)
			pData->m_PlayerID = pPlayer->m_ID;
		pData->m_Time = pPlayer->m_Time;
		mem_copy(pData->m_aCpTime, pPlayer->m_aCpTime, sizeof(pData->m_aCpTime));
	}
	return 0;
}

int CFileScore::SaveScoreHandler(CSaveScoreData *pData)
{
	CScoreJob Job;
	Job.m_NewData = CPlayerScore(pData->m_aPlayerName, pData->m_Time, pData->m_aCpTime);
	Job.m_pEntry = pData->m_PlayerID == -1 ? 0 : SearchScoreByPlayerID(pData->m_PlayerID);
	if(Job.m_pEntry)
	{
		if(Job.m_pEntry->m_Time <= Job.m_NewData.m_Time)
			return 0;
	}
	else
	{
		pData->m_PlayerID = ++m_PlayerCounter;
		Job.m_NewData.m_ID = pData->m_PlayerID;
	}

	Job.m_Type = Job.m_pEntry ? JOBTYPE_UPDATE_SCORE : JOBTYPE_ADD_NEW;
	m_lJobQueue.add(Job);
	ProcessJobs(false);
	return 0;
}

int CFileScore::ShowRankHandler(CShowRankData *pData)
{
	int Pos;
	const CPlayerScore *pPlayer = (pData->m_PlayerID != -1)
		? SearchScoreByPlayerID(pData->m_PlayerID, &Pos)
		: SearchScoreByName(pData->m_aName, &Pos);

	if(pPlayer)
	{
		CRecordData *pRecord = &pData->m_aRecords[pData->m_Num++];
		str_copy(pRecord->m_aPlayerName, pPlayer->m_aName, sizeof(pRecord->m_aPlayerName));
		pRecord->m_Time = pPlayer->m_Time;
		pRecord->m_Rank = Pos;
	}
	
	if(!pPlayer && pData->m_PlayerID == -1)
	{
		Pos = 1;
		for(sorted_array<CPlayerScore>::range r = m_lTop.all(); !r.empty(); r.pop_front(), Pos++)
		{
			if(str_find_nocase(r.front().m_aName, pData->m_aName))
			{
				if(pData->m_Num < MAX_SEARCH_RECORDS)
				{
					CRecordData *pRecord = &pData->m_aRecords[pData->m_Num];
					str_copy(pRecord->m_aPlayerName, r.front().m_aName, sizeof(pRecord->m_aPlayerName));
					pRecord->m_Time = r.front().m_Time;
					pRecord->m_Rank = Pos;
				}
				pData->m_Num++;
			}
		}
	}
	return 0;
}

int CFileScore::ShowTop5Handler(CShowTop5Data *pData)
{
	pData->m_TotalRecords = m_lTop.size();
	for(; pData->m_Num < MAX_TOP_RECORDS && pData->m_Start + pData->m_Num < pData->m_TotalRecords; pData->m_Num++)
	{
		const CPlayerScore *r = &m_lTop[pData->m_Start + pData->m_Num];
		CRecordData *pRecord = &pData->m_aRecords[pData->m_Num];
		str_copy(pRecord->m_aPlayerName, r->m_aName, sizeof(pRecord->m_aPlayerName));
		pRecord->m_Time = r->m_Time;
		pRecord->m_Rank = pData->m_Start + pData->m_Num + 1;
	}
	return 0;
}
