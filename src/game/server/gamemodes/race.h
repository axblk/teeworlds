/* copyright (c) 2007 rajh and gregwar. Score stuff */
#ifndef GAME_SERVER_GAMEMODES_RACE_H
#define GAME_SERVER_GAMEMODES_RACE_H

#include <game/server/gamecontroller.h>
#include <game/server/score.h>

class CGameControllerRACE : public IGameController
{
	void SendTime(int ClientID, int To);
	void SendFinish(int ID, int FinishTime, int Diff, bool NewRecord, int To);
	void OnCheckpoint(int ID, int z);

	int GetTime(int ID) const;
	int GetTimeExact(int ID) const;

protected:
	struct CRaceData
	{
		int m_RaceState;
		int m_StartTick;
		int m_aCpCurrent[NUM_CHECKPOINTS];
		int m_CpTick;
		int m_CpDiff;

		float m_AddTime;

		void Reset()
		{
			m_RaceState = RACE_NONE;
			m_StartTick = -1;
			mem_zero(m_aCpCurrent, sizeof(m_aCpCurrent));
			m_CpTick = -1;
			m_CpDiff = 0;
			m_AddTime = 0.f;
		}
	} m_aRace[MAX_CLIENTS];

	virtual bool IsStart(vec2 Pos, int Team) const;
	virtual bool IsEnd(vec2 Pos, int Team) const;
	virtual bool CanEndRace(int ID) const;

	virtual void OnRaceStart(int ID);
	virtual void OnRaceEnd(int ID, int FinishTime);

	void ResetPickups(int ClientID);

public:
	enum
	{
		RACE_NONE = 0,
		RACE_STARTED,
		RACE_FINISHED,
	};
	
	CGameControllerRACE(class CGameContext *pGameServer);
	~CGameControllerRACE();

	virtual void DoWincheck();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnCharacterSpawn(CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual bool IsFriendlyFire(int ClientID1, int ClientID2) const { return ClientID1 != ClientID2; };

	void OnPhysicsStep(int ID, vec2 Pos, float IntraTick);

	virtual bool CanStartRace(int ID) const;

	void StopRace(int ID) { OnRaceEnd(ID, 0); }

	int GetRaceState(int ID) const { return m_aRace[ID].m_RaceState; }
};

#endif
