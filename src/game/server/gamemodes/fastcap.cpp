// copyright (c) 2007 magnus auvinen, see licence.txt for more info
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "fastcap.h"

CGameControllerFC::CGameControllerFC(class CGameContext *pGameServer)
: CGameControllerRACE(pGameServer)
{
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlFlags[i] = 0;
	m_pGameType = "FastCap";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS|GAMEFLAG_RACE;
}

bool CGameControllerFC::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_POWERUP_NINJA)
		return false;

	if(IGameController::OnEntity(Index, Pos))
		return true;

	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if(Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

bool CGameControllerFC::IsOwnFlagStand(vec2 Pos, int Team) const
{
	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];
		if(F && F->GetTeam() == Team && distance(F->GetPos(), Pos) < CFlag::ms_PhysSize + CCharacter::ms_PhysSize)
			return true;
	}

	return false;
}

bool CGameControllerFC::IsEnemyFlagStand(vec2 Pos, int Team) const
{
	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];
		if(F && F->GetTeam() != Team && distance(F->GetPos(), Pos) < CFlag::ms_PhysSize + CCharacter::ms_PhysSize)
			return true;
	}

	return false;
}

void CGameControllerFC::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerRACE::OnCharacterSpawn(pChr);

	// full armor
	pChr->IncreaseArmor(10);

	// give nades
	if(!g_Config.m_SvNoItems)
		pChr->GiveWeapon(WEAPON_GRENADE, 10);
}

bool CGameControllerFC::CanBeMovedOnBalance(int Cid)
{
	return false;
}

void CGameControllerFC::OnRaceStart(int ID)
{
	CGameControllerRACE::OnRaceStart(ID);

	m_apPlFlags[ID] = new CFlag(&GameServer()->m_World, GameServer()->m_apPlayers[ID]->GetTeam()^1, vec2(0,0));
	m_apPlFlags[ID]->Grab(GameServer()->GetPlayerChar(ID));
	GameServer()->CreateSound(GameServer()->GetPlayerChar(ID)->GetPos(), SOUND_CTF_GRAB_EN, CmaskRace(GameServer(), ID));
}

void CGameControllerFC::OnRaceEnd(int ID, int FinishTime)
{
	CGameControllerRACE::OnRaceEnd(ID, FinishTime);

	if(m_apPlFlags[ID])
	{
		if(FinishTime)
		{
			ResetPickups(ID);
			GameServer()->CreateSound(GameServer()->GetPlayerChar(ID)->GetPos(), SOUND_CTF_CAPTURE, CmaskRace(GameServer(), ID));
		}

		GameServer()->m_World.DestroyEntity(m_apPlFlags[ID]);
		m_apPlFlags[ID] = 0;
	}
}

bool CGameControllerFC::CanStartRace(int ID) const
{
	return m_aRace[ID].m_RaceState != RACE_STARTED && GameServer()->IsPureTuning();
}

void CGameControllerFC::Snap(int SnappingClient)
{
	CGameControllerRACE::Snap(SnappingClient);

	CNetObj_GameDataFlag *pGameDataFlag = static_cast<CNetObj_GameDataFlag *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag)));
	if(!pGameDataFlag)
		return;

	pGameDataFlag->m_FlagDropTickRed = 0;
	pGameDataFlag->m_FlagDropTickBlue = 0;

	if(m_apPlFlags[SnappingClient])
	{
		if(m_apPlFlags[SnappingClient]->GetTeam() == TEAM_RED)
		{
			pGameDataFlag->m_FlagCarrierRed = SnappingClient;
			pGameDataFlag->m_FlagCarrierBlue = FLAG_ATSTAND;
		}
		else if(m_apPlFlags[SnappingClient]->GetTeam() == TEAM_BLUE)
		{
			pGameDataFlag->m_FlagCarrierRed = FLAG_ATSTAND;
			pGameDataFlag->m_FlagCarrierBlue = SnappingClient;
		}
	}
	else
	{
		pGameDataFlag->m_FlagCarrierRed = FLAG_ATSTAND;
		pGameDataFlag->m_FlagCarrierBlue = FLAG_ATSTAND;
	}
}
