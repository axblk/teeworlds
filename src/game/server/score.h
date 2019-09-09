#ifndef GAME_SERVER_SCORE_H
#define GAME_SERVER_SCORE_H

#include <base/system.h>
#include <engine/shared/protocol.h>

#define NUM_CHECKPOINTS 25

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
	
	void SetTime(int Time, int *pCpTime)
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

	bool UpdateTime(int Time, int *pCpTime)
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

class IScore
{
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;

	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server() { return m_pServer; }

	int m_LastRequest[MAX_CLIENTS];

protected:
	CPlayerData m_aPlayerData[MAX_CLIENTS];
	int m_CurrentRecord;

	void UpdateThrottling(int ClientID);
	bool IsThrottled(int ClientID);
	
public:
	IScore(CGameContext *pGameServer) : m_pGameServer(pGameServer), m_CurrentRecord(0) { }
	virtual ~IScore() { }
	
	const CPlayerData *PlayerData(int ID) const { return &m_aPlayerData[ID]; }
	int GetRecord() const { return m_CurrentRecord; }

	bool UpdateRecord(int Time)
	{
		bool Check = !m_CurrentRecord || Time < m_CurrentRecord;
		if(Check)
			m_CurrentRecord = Time;
		return Check;
	}

	virtual void OnMapLoad()
	{
		m_CurrentRecord = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			m_aPlayerData[i].Reset();
			m_LastRequest[i] = -1;
		}
	}

	virtual void Tick() { }
	
	virtual void OnPlayerInit(int ClientID) = 0;
	virtual void OnPlayerLeave(int ClientID) { };
	virtual void OnPlayerFinish(int ClientID, int Time, int *pCpTime) = 0;
	
	virtual void ShowTop5(int RequestingClientID, int Debut = 1) = 0;
	virtual void ShowRank(int RequestingClientID, const char *pName) = 0;
	virtual void ShowRank(int RequestingClientID, int ClientID) = 0;
};

#endif
