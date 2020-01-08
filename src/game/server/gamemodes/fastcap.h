#ifndef GAME_SERVER_GAMEMODES_CTF_H
#define GAME_SERVER_GAMEMODES_CTF_H
#include "race.h"

class CGameControllerFC : public CGameControllerRACE
{
	class CFlag *m_apFlags[2];
	class CFlag *m_apPlFlags[MAX_CLIENTS];

	bool IsOwnFlagStand(vec2 Pos, int Team) const;
	bool IsEnemyFlagStand(vec2 Pos, int Team) const;

	virtual bool CanBeMovedOnBalance(int ClientID) const { return false; }

protected:
	virtual bool IsStart(vec2 Pos, int Team) const { return IsEnemyFlagStand(Pos, Team); }
	virtual bool IsEnd(vec2 Pos, int Team) const { return IsOwnFlagStand(Pos, Team); }

	virtual void OnRaceStart(int ID);
	virtual void OnRaceEnd(int ID, int FinishTime);

public:
	CGameControllerFC(class CGameContext *pGameServer);
	
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	
	virtual bool IsFastCap() const { return true; }

	virtual bool CanStartRace(int ID) const;
	
	virtual void Snap(int SnappingClient);
};

#endif
