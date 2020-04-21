/* copyright (c) 2007 rajh, race mod stuff */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/entities/pickup.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/score.h>
#include <game/race.h>
#include "race.h"

CGameControllerRACE::CGameControllerRACE(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Race";
	m_GameFlags = GAMEFLAG_RACE;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aRace[i].Reset();
	
	SetRunning();
}

CGameControllerRACE::~CGameControllerRACE()
{
}

void CGameControllerRACE::RegisterChatCommands(CCommandManager *pManager)
{
	IGameController::RegisterChatCommands(pManager);

	pManager->AddCommand("info", "Information about the mod", "", CGameContext::ChatConInfo, GameServer());
	pManager->AddCommand("rank", "Show the rank of a specific player", "?r[name]", CGameContext::ChatConRank, GameServer());
	pManager->AddCommand("top5", "Show the top 5", "?i[start]", CGameContext::ChatConTop5, GameServer());
	if(Config()->m_SvShowOthers)
		pManager->AddCommand("show_others", "Show/Hide other players", "", CGameContext::ChatConShowOthers, GameServer());
	// TODO: hide this?
	pManager->AddCommand("help", "Print help text", "", CGameContext::ChatConHelp, GameServer());
}

void CGameControllerRACE::OnCharacterSpawn(CCharacter *pChr)
{
	IGameController::OnCharacterSpawn(pChr);

	pChr->SetActiveWeapon(WEAPON_HAMMER);
	ResetPickups(pChr->GetPlayer()->GetCID());
}

int CGameControllerRACE::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	int ClientID = pVictim->GetPlayer()->GetCID();
	StopRace(ClientID);
	m_aRace[ClientID].Reset();
	return 0;
}

void CGameControllerRACE::DoWincheck()
{
	/*if(m_GameOverTick == -1 && !m_Warmup)
	{
		if((Config()->m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= Config()->m_SvTimelimit*Server()->TickSpeed()*60))
			EndRound();
	}*/
}

void CGameControllerRACE::Tick()
{
	IGameController::Tick();
	DoWincheck();

	bool PureTuning = GameServer()->IsPureTuning();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aRace[i].m_RaceState == RACE_STARTED && !PureTuning)
			StopRace(i);

		if(m_aRace[i].m_RaceState == RACE_STARTED && (Server()->Tick() - m_aRace[i].m_StartTick) % Server()->TickSpeed() == 0)
			SendTime(i, i);

		int SpecID = GameServer()->m_apPlayers[i] ? GameServer()->m_apPlayers[i]->GetSpectatorID() : -1;
		if(SpecID != -1 && Config()->m_SvShowTimes && m_aRace[SpecID].m_RaceState == RACE_STARTED &&
			(Server()->Tick() - m_aRace[SpecID].m_StartTick) % Server()->TickSpeed() == 0)
			SendTime(SpecID, i);
	}
}

void CGameControllerRACE::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameDataRace *pGameDataRace = static_cast<CNetObj_GameDataRace *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATARACE, 0, sizeof(CNetObj_GameDataRace)));
	if(!pGameDataRace)
		return;

	int MapRecord = GameServer()->Score()->GetRecord();
	pGameDataRace->m_BestTime = (MapRecord == 0) ? -1 : MapRecord;
	pGameDataRace->m_Precision = 3;
	pGameDataRace->m_RaceFlags = RACEFLAG_HIDE_KILLMSG|RACEFLAG_KEEP_WANTED_WEAPON;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aRace[i].m_RaceState != RACE_STARTED)
			continue;

		if(i == SnappingClient || (Config()->m_SvShowTimes && (SnappingClient == -1 || i == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID())))
		{
			CNetObj_PlayerInfoRace *pPlayerInfoRace = static_cast<CNetObj_PlayerInfoRace *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFORACE, i, sizeof(CNetObj_PlayerInfoRace)));
			if(!pPlayerInfoRace)
				return;

			pPlayerInfoRace->m_RaceStartTick = m_aRace[i].m_StartTick;
		}
	}
}

void CGameControllerRACE::SendTime(int ClientID, int To)
{
	if(Server()->GetClientVersion(ClientID) >= CGameContext::MIN_RACE_CLIENTVERSION)
		return;

	CRaceData *p = &m_aRace[ClientID];
	char aBuf[128] = {0};
	char aTimeBuf[64];

	bool Checkpoint = p->m_CpTick != -1 && p->m_CpTick + Server()->TickSpeed() * 2 > Server()->Tick();
	if(Checkpoint)
	{
		char aDiff[64];
		IRace::FormatTimeDiff(aDiff, sizeof(aDiff), p->m_CpDiff, false);
		const char *pColor = (p->m_CpDiff <= 0) ? "^090" : "^900";
		str_format(aBuf, sizeof(aBuf), "%s%s\\n^999", pColor, aDiff);
	}

	int Time = GetTime(ClientID);
	str_format(aTimeBuf, sizeof(aTimeBuf), "%02d:%02d", Time / (60 * 1000), (Time / 1000) % 60);
	str_append(aBuf, aTimeBuf, sizeof(aBuf));
	GameServer()->SendBroadcast(aBuf, To);
}

void CGameControllerRACE::OnCheckpoint(int ID, int z)
{
	CRaceData *p = &m_aRace[ID];
	const CPlayerData *pBest = GameServer()->Score()->PlayerData(ID);
	if(p->m_RaceState != RACE_STARTED)
		return;

	p->m_aCpCurrent[z] = GetTime(ID);

	if(pBest->m_Time && pBest->m_aCpTime[z] != 0 && p->m_CpTick + Server()->TickSpeed() / 2 <= Server()->Tick())
	{
		p->m_CpDiff = p->m_aCpCurrent[z] - pBest->m_aCpTime[z];
		p->m_CpTick = Server()->Tick();

		CNetMsg_Sv_Checkpoint CP;
		CP.m_Diff = p->m_CpDiff;
		Server()->SendPackMsg(&CP, MSGFLAG_VITAL, ID);
	}
}

void CGameControllerRACE::OnRaceStart(int ID)
{
	CRaceData *p = &m_aRace[ID];
	CCharacter *pChr = GameServer()->GetPlayerChar(ID);

	if(p->m_RaceState != RACE_NONE)
	{
		// reset pickups
		if(!pChr->HasWeapon(WEAPON_GRENADE))
			ResetPickups(ID);
	}
	
	p->m_RaceState = RACE_STARTED;
	p->m_StartTick = Server()->Tick();
	p->m_AddTime = 0.f;
}

void CGameControllerRACE::SendFinish(int ID, int FinishTime, int Diff, bool NewRecord, int To)
{
	CNetMsg_Sv_RaceFinish FinishMsg;
	FinishMsg.m_ClientID = ID;
	FinishMsg.m_Time = FinishTime;
	FinishMsg.m_Diff = Diff;
	FinishMsg.m_RecordPersonal = NewRecord;
	FinishMsg.m_RecordServer = false; // TODO
	Server()->SendPackMsg(&FinishMsg, MSGFLAG_VITAL, To);

	char aBuf[128];
	char aBuf2[128];
	IRace::FormatTimeLong(aBuf2, sizeof(aBuf2), FinishTime, true);
	str_format(aBuf, sizeof(aBuf), "%s finished in: %s", Server()->ClientName(ID), aBuf2);
	str_format(aBuf2, sizeof(aBuf2), "New record: %d.%03d second(s) better", -Diff / 1000, -Diff % 1000);

	if(To == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(!GameServer()->m_apPlayers[i] || !Server()->ClientIngame(i) || Server()->GetClientVersion(i) >= CGameContext::MIN_RACE_CLIENTVERSION)
				continue;

			GameServer()->SendChat(-1, CHAT_ALL, i, aBuf);
			if(Diff < 0)
				GameServer()->SendChat(-1, CHAT_ALL, i, aBuf2);
		}
	}
	else if(Server()->GetClientVersion(To) < CGameContext::MIN_RACE_CLIENTVERSION)
	{
		GameServer()->SendChat(-1, CHAT_ALL, To, aBuf);
		if(Diff < 0)
			GameServer()->SendChat(-1, CHAT_ALL, To, aBuf2);
	}
}

void CGameControllerRACE::OnRaceEnd(int ID, int FinishTime)
{
	CRaceData *p = &m_aRace[ID];
	p->m_RaceState = RACE_FINISHED;

	if(!FinishTime)
		return;

	// TODO:
	// move all this into the scoring classes so the selected
	// scoring backend can decide how to handle the situation

	int Diff = 0;
	bool NewRecord = true;
	if(GameServer()->Score()->PlayerData(ID)->m_Time > 0)
	{
		Diff = FinishTime - GameServer()->Score()->PlayerData(ID)->m_Time;
		NewRecord = Diff < 0;
	}

	// save the score
	GameServer()->Score()->OnPlayerFinish(ID, FinishTime, p->m_aCpCurrent);
	SendFinish(ID, FinishTime, Diff, NewRecord, Config()->m_SvShowTimes ? -1 : ID);
}

bool CGameControllerRACE::IsStart(vec2 Pos, int Team) const
{
	return GameServer()->Collision()->CheckIndexEx(Pos, TILE_BEGIN);
}

bool CGameControllerRACE::IsEnd(vec2 Pos, int Team) const
{
	return GameServer()->Collision()->CheckIndexEx(Pos, TILE_END);
}

void CGameControllerRACE::OnPhysicsStep(int ID, vec2 Pos, float IntraTick)
{
	int Cp = GameServer()->Collision()->CheckCheckpoint(Pos);
	if(Cp != -1)
		OnCheckpoint(ID, Cp);

	float IntraTime = 1000.f / Server()->TickSpeed() * IntraTick;
	int Team = GameServer()->m_apPlayers[ID]->GetTeam();
	if(CanStartRace(ID) && IsStart(Pos, Team))
	{
		OnRaceStart(ID);
		m_aRace[ID].m_AddTime -= IntraTime;
	}
	else if(CanEndRace(ID) && IsEnd(Pos, Team))
	{
		m_aRace[ID].m_AddTime += IntraTime;
		OnRaceEnd(ID, GetTimeExact(ID));
	}
}

bool CGameControllerRACE::CanStartRace(int ID) const
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ID);
	bool AllowRestart = Config()->m_SvAllowRestartOld && !pChr->HasWeapon(WEAPON_GRENADE) && !pChr->Armor();
	return (m_aRace[ID].m_RaceState == RACE_NONE || AllowRestart) && GameServer()->IsPureTuning();
}

bool CGameControllerRACE::CanEndRace(int ID) const
{
	return m_aRace[ID].m_RaceState == RACE_STARTED;
}

void CGameControllerRACE::ResetPickups(int ClientID)
{
	CPickup *pPickup = static_cast<CPickup *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_PICKUP));
	for(; pPickup; pPickup = (CPickup *)pPickup->TypeNext())
		pPickup->Respawn(ClientID);
}

int CGameControllerRACE::GetTime(int ID) const
{
	return (Server()->Tick() - m_aRace[ID].m_StartTick) * 1000 / Server()->TickSpeed();
}

int CGameControllerRACE::GetTimeExact(int ID) const
{
	return GetTime(ID) + round_to_int(m_aRace[ID].m_AddTime);
}
