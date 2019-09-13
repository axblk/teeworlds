#ifndef GAME_SERVER_SCORE_H
#define GAME_SERVER_SCORE_H

#include <base/system.h>
#include <engine/shared/protocol.h>

#define NUM_CHECKPOINTS 25

class IScoreResponseListener;

class IScoreBackend
{
	IScoreResponseListener *m_pListener;

public:
	enum
	{
		MAX_SEARCH_RECORDS=5,
		MAX_TOP_RECORDS=5,

		REQTYPE_LOAD_MAP=0,
		REQTYPE_LOAD_PLAYER,
		REQTYPE_SAVE_SCORE,
		REQTYPE_SHOW_RANK,
		REQTYPE_SHOW_TOP5
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
		virtual ~CRequestData() { }
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
		int m_RequestingClientID;
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

	static void CheckpointsFromString(int *pCpTime, const char *pStr, const char *pDelim = ";");
	static void CheckpointsToString(char *pBuf, int BufSize, const int *pCpTime, const char *pDelim = ";");

	IScoreResponseListener *Listener() { return m_pListener; }

	IScoreBackend(IScoreResponseListener *pListener) : m_pListener(pListener) { }
	virtual ~IScoreBackend() { }

	virtual bool Ready() const = 0;
	virtual void Tick() = 0;
	virtual void AddRequest(int Type, CRequestData *pRequestData = 0) = 0;
};

class IScoreResponseListener
{
public:
	virtual void OnRequestFinished(int Type, IScoreBackend::CRequestData *pRequestData, bool Error) = 0;
};

class CPlayerData
{
public:
	CPlayerData()
	{
		Reset();
	}
	
	void Reset()
	{
		m_Time = 0;
		m_CurTime = 0;
		mem_zero(m_aCpTime, sizeof(m_aCpTime));
	}
	
	void SetTime(int Time, const int *pCpTime)
	{
		m_Time = Time;
		mem_copy(m_aCpTime, pCpTime, sizeof(m_aCpTime));
	}

	void UpdateCurTime(int Time)
	{
		if(!m_CurTime || Time < m_CurTime)
			m_CurTime = Time;
	}
	
	bool CheckTime(int Time) const
	{
		return !m_Time || Time < m_Time;
	}

	bool UpdateTime(int Time, const int *pCpTime)
	{
		UpdateCurTime(Time);
		bool Check = CheckTime(Time);
		if(Check)
			SetTime(Time, pCpTime);
		return Check;
	}

	int m_Time;
	int m_CurTime;
	int m_aCpTime[NUM_CHECKPOINTS];
};

class CScore : public IScoreResponseListener
{
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	IScoreBackend *m_pBackend;

	int m_MapID;
	int m_aPlayerID[MAX_CLIENTS];

	int m_LastRequest[MAX_CLIENTS];

	CPlayerData m_aPlayerCache[MAX_CLIENTS];
	int m_CurrentRecord;

	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	void UpdateThrottling(int ClientID);
	bool IsThrottled(int ClientID);

	bool UpdateRecord(int Time);
	
public:
	CScore(CGameContext *pGameServer);
	virtual ~CScore() { }
	
	const CPlayerData *PlayerData(int ID) const { return &m_aPlayerCache[ID]; }
	int GetRecord() const { return m_CurrentRecord; }

	void Tick() { m_pBackend->Tick(); }

	void OnRequestFinished(int Type, IScoreBackend::CRequestData *pRequestData, bool Error);

	void OnMapLoad();
	void OnPlayerInit(int ClientID);
	void OnPlayerLeave(int ClientID);
	void OnPlayerFinish(int ClientID, int Time, int *pCpTime);
	
	void ShowRank(int RequestingClientID, const char *pName);
	void ShowRank(int RequestingClientID, int ClientID);
	void ShowTop5(int RequestingClientID, int Debut = 1);
};

#endif
