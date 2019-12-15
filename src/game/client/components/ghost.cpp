/* (c) Rajh, Redix and Sushi. */

#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include "controls.h"
#include "menus.h"
#include "players.h"
#include "skins.h"
#include "ghost.h"

const char * const CGhost::ms_pGhostDir = "ghosts";

CGhost::CGhost() : m_Loaded(false), m_NewRenderTick(-1), m_StartRenderTick(-1), m_Recording(false), m_Rendering(false), m_SymmetricMap(false) {}

void CGhost::CGhostPath::Copy(const CGhostPath &Other)
{
	Reset(Other.m_ChunkSize);
	SetSize(Other.Size());
	for(int i = 0; i < m_lChunks.size(); i++)
		mem_copy(m_lChunks[i], Other.m_lChunks[i], sizeof(CGhostCharacter) * m_ChunkSize);
}

void CGhost::CGhostPath::Reset(int ChunkSize)
{
	for(int i = 0; i < m_lChunks.size(); i++)
		mem_free(m_lChunks[i]);
	m_lChunks.clear();
	m_ChunkSize = ChunkSize;
	m_NumItems = 0;
}

void CGhost::CGhostPath::SetSize(int Items)
{
	int Chunks = m_lChunks.size();
	int NeededChunks = (Items + m_ChunkSize - 1) / m_ChunkSize;

	if(NeededChunks > Chunks)
	{
		m_lChunks.set_size(NeededChunks);
		for(int i = Chunks; i < NeededChunks; i++)
			m_lChunks[i] = (CGhostCharacter*)mem_alloc(sizeof(CGhostCharacter) * m_ChunkSize, 1);
	}

	m_NumItems = Items;
}

void CGhost::CGhostPath::Add(CGhostCharacter Char)
{
	SetSize(m_NumItems + 1);
	*Get(m_NumItems - 1) = Char;
}

CGhostCharacter *CGhost::CGhostPath::Get(int Index)
{
	if(Index < 0 || Index >= m_NumItems)
		return 0;

	int Chunk = Index / m_ChunkSize;
	int Pos = Index % m_ChunkSize;
	return &m_lChunks[Chunk][Pos];
}

void CGhost::GetPath(char *pBuf, int Size, const char *pPlayerName, int Time) const
{
	const char *pMap = Client()->GetCurrentMapName();
	unsigned Crc = Client()->GetMapCrc();

	char aPlayerName[MAX_NAME_LENGTH];
	str_copy(aPlayerName, pPlayerName, sizeof(aPlayerName));
	str_sanitize_filename(aPlayerName);

	if(Time < 0)
		str_format(pBuf, Size, "%s/%s_%08x_%s_tmp.gho", ms_pGhostDir, pMap, Crc, aPlayerName);
	else
		str_format(pBuf, Size, "%s/%s_%08x_%s_%d.%03d.gho", ms_pGhostDir, pMap, Crc, aPlayerName, Time / 1000, Time % 1000);
}

void CGhost::AddInfos(const CNetObj_Character *pChar)
{
	int NumTicks = m_CurGhost.m_Path.Size();

	// do not start writing to file as long as we still touch the start line
	if(Config()->m_ClRaceSaveGhost && !m_GhostRecorder.IsRecording() && NumTicks > 0)
	{
		GetPath(m_aTmpFilename, sizeof(m_aTmpFilename), m_CurGhost.m_aPlayer);
		m_GhostRecorder.Start(m_aTmpFilename, Client()->GetCurrentMapName(), Client()->GetMapCrc(), m_CurGhost.m_aPlayer);

		m_GhostRecorder.WriteData(GHOSTDATA_TYPE_START_TICK, (const char*)&m_CurGhost.m_StartTick, sizeof(int));
		m_GhostRecorder.WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&m_CurGhost.m_Skin, sizeof(CGhostSkin));
		if(m_CurGhost.m_Team != -1)
			m_GhostRecorder.WriteData(GHOSTDATA_TYPE_TEAM, (const char*)&m_CurGhost.m_Team, sizeof(int));
		for(int i = 0; i < NumTicks; i++)
			m_GhostRecorder.WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)m_CurGhost.m_Path.Get(i), sizeof(CGhostCharacter));
	}

	CGhostCharacter GhostChar;
	CGhostTools::GetGhostCharacter(&GhostChar, pChar);
	m_CurGhost.m_Path.Add(GhostChar);
	if(m_GhostRecorder.IsRecording())
		m_GhostRecorder.WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)&GhostChar, sizeof(CGhostCharacter));
}

int CGhost::GetSlot() const
{
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		if(m_aActiveGhosts[i].Empty())
			return i;
	return -1;
}

int CGhost::FreeSlots() const
{
	int Num = 0;
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		if(m_aActiveGhosts[i].Empty())
			Num++;
	return Num;
}

void CGhost::MirrorChar(CNetObj_Character *pChar, int Middle)
{
	pChar->m_HookDx = -pChar->m_HookDx;
	pChar->m_VelX = -pChar->m_VelX;
	pChar->m_HookX = 2 * Middle - pChar->m_HookX;
	pChar->m_X = 2 * Middle - pChar->m_X;
	pChar->m_Angle = -pChar->m_Angle - pi*256.f;
	pChar->m_Direction = -pChar->m_Direction;
}

void CGhost::OnNewSnapshot(bool Predicted)
{
	if(!(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_RACE)|| !Config()->m_ClRaceGhost || Client()->State() != IClient::STATE_ONLINE)
		return;

	if(!m_Loaded)
		LoadGhosts();

	const CNetObj_PlayerInfoRace *pRaceInfo = m_pClient->m_Snap.m_paPlayerInfosRace[m_pClient->m_LocalClientID];
	if(!pRaceInfo || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	int RaceTick = pRaceInfo->m_RaceStartTick;
	int RenderTick = m_NewRenderTick;

	static int s_LastRaceTick = -1;

	if(s_LastRaceTick != RaceTick && Client()->GameTick() - RaceTick < Client()->GameTickSpeed())
	{
		if(m_Rendering) // race restarted: stop rendering
			StopRender();
		if(s_LastRaceTick == -1) // no restart: reset rendering preparations
			m_NewRenderTick = -1;
		if(m_GhostRecorder.IsRecording()) // race restarted: stop recording
			m_GhostRecorder.Stop(0, -1);
		int StartTick = RaceTick;
		StartRecord(StartTick);
		RenderTick = StartTick;
	}

	if(m_Recording)
		AddInfos(m_pClient->m_Snap.m_pLocalCharacter);

	s_LastRaceTick = RaceTick;

	// only restart rendering if it did not change since last tick to prevent stuttering
	if(m_NewRenderTick != -1 && m_NewRenderTick == RenderTick)
	{
		StartRender(RenderTick);
		RenderTick = -1;
	}
	m_NewRenderTick = RenderTick;
}

void CGhost::OnRender()
{
	// Play the ghost
	if(!m_Rendering)
		return;

	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);

	int PlaybackTick = Client()->PredGameTick() - m_StartRenderTick;

	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
	{
		CGhostData *pGhost = &m_aActiveGhosts[i];
		if(pGhost->Empty())
			continue;

		int GhostTick = pGhost->m_StartTick + PlaybackTick;
		while(pGhost->m_PlaybackPos >= 0 && pGhost->m_Path.Get(pGhost->m_PlaybackPos)->m_Tick < GhostTick)
		{
			if(pGhost->m_PlaybackPos < pGhost->m_Path.Size() - 1)
				pGhost->m_PlaybackPos++;
			else
				pGhost->m_PlaybackPos = -1;
		}

		if(pGhost->m_PlaybackPos < 0)
			continue;

		int CurPos = pGhost->m_PlaybackPos;
		int PrevPos = max(0, CurPos - 1);
		if(pGhost->m_Path.Get(PrevPos)->m_Tick > GhostTick)
			continue;

		CNetObj_Character Player, Prev;
		CGhostTools::GetNetObjCharacter(&Player, pGhost->m_Path.Get(CurPos));
		CGhostTools::GetNetObjCharacter(&Prev, pGhost->m_Path.Get(PrevPos));

		if(pGhost->m_Mirror && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS)
		{
			MirrorChar(&Player, m_Middle);
			MirrorChar(&Prev, m_Middle);
		}

		int TickDiff = Player.m_Tick - Prev.m_Tick;
		float IntraTick = 0.f;
		if(TickDiff > 0)
			IntraTick = (GhostTick - Prev.m_Tick - 1 + Client()->PredIntraGameTick()) / TickDiff;

		Player.m_AttackTick += Client()->GameTick() - GhostTick;

		m_pClient->m_pPlayers->RenderHook(&Prev, &Player, &pGhost->m_RenderInfo, -2, IntraTick);
		m_pClient->m_pPlayers->RenderPlayer(&Prev, &Player, 0, &pGhost->m_RenderInfo, -2, IntraTick);
		if(Config()->m_ClGhostNamePlates)
			RenderGhostNamePlate(&Prev, &Player, IntraTick, pGhost->m_aPlayer);
	}
}

void CGhost::RenderGhostNamePlate(const CNetObj_Character *pPrev, const CNetObj_Character *pPlayer, float IntraTick, const char *pName)
{
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pPlayer->m_X, pPlayer->m_Y), IntraTick);
	float FontSize = 18.0f + 20.0f * Config()->m_ClNameplatesSize / 100.0f;

	// render name plate
	float a = 0.5f;
	if(Config()->m_ClGhostNameplatesAlways == 0)
		a = clamp(0.5f-powf(distance(m_pClient->m_pControls->m_TargetPos, Pos)/200.0f,16.0f), 0.0f, 0.5f);

	float tw = TextRender()->TextWidth(0, FontSize, pName, -1, -1.0f);
	
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.5f*a);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, a);
	TextRender()->Text(0, Pos.x-tw/2.0f, Pos.y-FontSize-38.0f, FontSize, pName, -1);

	// reset color;
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
}

void CGhost::InitRenderInfos(CGhostData *pGhost)
{
	char aSkinPartName[24];
	CTeeRenderInfo *pRenderInfo = &pGhost->m_RenderInfo;
	pRenderInfo->m_Size = 64;

	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		IntsToStr(pGhost->m_Skin.m_aaSkinPartNames[p], 6, aSkinPartName);
		int ID = m_pClient->m_pSkins->FindSkinPart(p, aSkinPartName, false);
		if(ID < 0)
		{
			if(p == SKINPART_MARKING || p == SKINPART_DECORATION)
				ID = m_pClient->m_pSkins->FindSkinPart(p, "", false);
			else
				ID = m_pClient->m_pSkins->FindSkinPart(p, "standard", false);
			if(ID < 0)
				ID = 0;
		}

		const CSkins::CSkinPart *pSkinPart = m_pClient->m_pSkins->GetSkinPart(p, ID);
		if(pGhost->m_Skin.m_aUseCustomColors[p])
		{
			pRenderInfo->m_aTextures[p] = pSkinPart->m_ColorTexture;
			pRenderInfo->m_aColors[p] = m_pClient->m_pSkins->GetColorV4(pGhost->m_Skin.m_aSkinPartColors[p], p==SKINPART_MARKING);
		}
		else
		{
			pRenderInfo->m_aTextures[p] = pSkinPart->m_OrgTexture;
			pRenderInfo->m_aColors[p] = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		pRenderInfo->m_aColors[p] *= 0.5f;
	}
}

void CGhost::StartRecord(int Tick)
{
	m_Recording = true;
	m_CurGhost.Reset();
	m_CurGhost.m_StartTick = Tick;

	const CGameClient::CClientData *pData = &m_pClient->m_aClients[m_pClient->m_LocalClientID];
	if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS)
		m_CurGhost.m_Team = pData->m_Team;
	str_copy(m_CurGhost.m_aPlayer, Config()->m_PlayerName, sizeof(m_CurGhost.m_aPlayer));
	CGhostTools::GetGhostSkin(&m_CurGhost.m_Skin, pData->m_aaSkinPartNames, pData->m_aUseCustomColors, pData->m_aSkinPartColors);
	InitRenderInfos(&m_CurGhost);
}

void CGhost::StopRecord(int Time)
{
	m_Recording = false;
	bool RecordingToFile = m_GhostRecorder.IsRecording();

	if(RecordingToFile)
		m_GhostRecorder.Stop(m_CurGhost.m_Path.Size(), Time);

	const CGhostEntry *pOwnGhost = m_pClient->m_pMenus->GetOwnGhost();
	if(Time > 0 && (!pOwnGhost || Time < pOwnGhost->m_Time))
	{
		if(pOwnGhost && pOwnGhost->Active())
			Unload(pOwnGhost->m_Slot);

		// add to active ghosts
		int Slot = GetSlot();
		if(Slot != -1)
			m_aActiveGhosts[Slot] = m_CurGhost;

		// create ghost entry
		CGhostEntry Entry;
		if(RecordingToFile)
			GetPath(Entry.m_aFilename, sizeof(Entry.m_aFilename), m_CurGhost.m_aPlayer, Time);
		str_copy(Entry.m_aPlayer, m_CurGhost.m_aPlayer, sizeof(Entry.m_aPlayer));
		Entry.m_Time = Time;
		Entry.m_Slot = Slot;

		// save new ghost file
		if(Entry.HasFile())
			Storage()->RenameFile(m_aTmpFilename, Entry.m_aFilename, IStorage::TYPE_SAVE);

		// add entry to menu list
		m_pClient->m_pMenus->UpdateOwnGhost(Entry);
	}
	else if(RecordingToFile) // no new record
		Storage()->RemoveFile(m_aTmpFilename, IStorage::TYPE_SAVE);

	m_aTmpFilename[0] = 0;

	m_CurGhost.Reset();
}

void CGhost::StartRender(int Tick)
{
	m_Rendering = true;
	m_StartRenderTick = Tick;
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		m_aActiveGhosts[i].m_PlaybackPos = 0;
}

void CGhost::StopRender()
{
	m_Rendering = false;
	m_NewRenderTick = -1;
}

int CGhost::Load(const char *pFilename)
{
	// default skin params
	static const char s_aaSkinPartNames[NUM_SKINPARTS][24] = {"standard", "", "", "standard", "standard", "standard"};
	static const int s_aUseCustomColors[NUM_SKINPARTS] = {1, 0, 0, 1, 1, 0};
	static const int s_aSkinPartColors[NUM_SKINPARTS] = {0x1B6F74, 0, 0, 0x1B759E, 0x1C873E, 0};

	int Slot = GetSlot();
	if(Slot == -1)
		return -1;

	if(m_GhostLoader.Load(pFilename, Client()->GetCurrentMapName()) != 0)
		return -1;

	const CGhostHeader *pHeader = m_GhostLoader.GetHeader();

	int NumTicks = bytes_be_to_uint(pHeader->m_aNumTicks);
	int Time = bytes_be_to_uint(pHeader->m_aTime);
	if(NumTicks <= 0 || Time <= 0)
	{
		m_GhostLoader.Close();
		return -1;
	}

	// select ghost
	CGhostData *pGhost = &m_aActiveGhosts[Slot];
	pGhost->Reset();
	pGhost->m_Path.SetSize(NumTicks);

	str_copy(pGhost->m_aPlayer, pHeader->m_aOwner, sizeof(pGhost->m_aPlayer));

	int Index = 0;
	bool FoundSkin = false;
	bool NoTick = false;
	bool Error = false;

	int Type;
	while(!Error && m_GhostLoader.ReadNextType(&Type))
	{
		if(Index == NumTicks && (Type == GHOSTDATA_TYPE_CHARACTER || Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK))
		{
			Error = true;
			break;
		}

		if(Type == GHOSTDATA_TYPE_SKIN && !FoundSkin)
		{
			FoundSkin = true;
			if(!m_GhostLoader.ReadData(Type, (char*)&pGhost->m_Skin, sizeof(CGhostSkin)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_SKIN_06 && !FoundSkin)
		{
			CGhostSkin_06 Skin06;
			if(!m_GhostLoader.ReadData(Type, (char*)&Skin06, sizeof(CGhostSkin_06)))
			{
				Error = true;
			}
			else
			{
				static const int s_aTeamColors[2] = { 65387, 10223467 };
				for(int i = 0; i < 2; i++)
					if(Skin06.m_UseCustomColor && Skin06.m_ColorBody == s_aTeamColors[i] && Skin06.m_ColorFeet == s_aTeamColors[i])
						pGhost->m_Team = i;

				char aSkinName[24];
				IntsToStr(Skin06.m_aSkin, 6, aSkinName);

				int Skin = m_pClient->m_pSkins->Find(aSkinName, false);
				if(Skin != -1)
				{
					FoundSkin = true;
					const CSkins::CSkin *pNinja = m_pClient->m_pSkins->Get(Skin);
					for(int p = 0; p < NUM_SKINPARTS; p++)
					{
						StrToInts(pGhost->m_Skin.m_aaSkinPartNames[p], 6, pNinja->m_apParts[p]->m_aName);
						pGhost->m_Skin.m_aSkinPartColors[p] = pNinja->m_aPartColors[p];
						int UseCustomColor = Skin06.m_UseCustomColor;
						if(UseCustomColor)
						{
							if(p == SKINPART_FEET)
								pGhost->m_Skin.m_aSkinPartColors[p] = Skin06.m_ColorFeet;
							else if(p == SKINPART_BODY || p == SKINPART_DECORATION)
								pGhost->m_Skin.m_aSkinPartColors[p] = Skin06.m_ColorBody;
							else
								UseCustomColor = 0;
						}
						pGhost->m_Skin.m_aUseCustomColors[p] = pNinja->m_aUseCustomColors[p] || UseCustomColor;
					}
				}
			}
			
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK)
		{
			NoTick = true;
			if(!m_GhostLoader.ReadData(Type, (char*)pGhost->m_Path.Get(Index++), sizeof(CGhostCharacter_NoTick)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER)
		{
			if(!m_GhostLoader.ReadData(Type, (char*)pGhost->m_Path.Get(Index++), sizeof(CGhostCharacter)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_START_TICK)
		{
			if(!m_GhostLoader.ReadData(Type, (char*)&pGhost->m_StartTick, sizeof(int)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_TEAM)
		{
			if(!m_GhostLoader.ReadData(Type, (char*)&pGhost->m_Team, sizeof(int)))
				Error = true;
		}
	}

	m_GhostLoader.Close();

	if(Error || Index != NumTicks)
	{
		pGhost->Reset();
		return -1;
	}

	if(NoTick)
	{
		int StartTick = 0;
		for(int i = 1; i < NumTicks; i++) // estimate start tick
			if(pGhost->m_Path.Get(i)->m_AttackTick != pGhost->m_Path.Get(i - 1)->m_AttackTick)
				StartTick = pGhost->m_Path.Get(i)->m_AttackTick - i;
		for(int i = 0; i < NumTicks; i++)
			pGhost->m_Path.Get(i)->m_Tick = StartTick + i;
	}

	if(pGhost->m_StartTick == -1)
		pGhost->m_StartTick = pGhost->m_Path.Get(0)->m_Tick;

	if(!FoundSkin)
		CGhostTools::GetGhostSkin(&pGhost->m_Skin, s_aaSkinPartNames, s_aUseCustomColors, s_aSkinPartColors);
	InitRenderInfos(pGhost);

	if(AutoMirroring())
		pGhost->AutoMirror(m_pClient->m_aClients[m_pClient->m_LocalClientID].m_Team);

	return Slot;
}

void CGhost::Unload(int Slot)
{
	m_aActiveGhosts[Slot].Reset();
}

void CGhost::UnloadAll()
{
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		Unload(i);
}

void CGhost::SaveGhost(CGhostEntry *pEntry)
{
	int Slot = pEntry->m_Slot;
	if(!pEntry->Active() || pEntry->HasFile() || m_aActiveGhosts[Slot].Empty() || m_GhostRecorder.IsRecording())
		return;

	CGhostData *pGhost = &m_aActiveGhosts[Slot];

	int NumTicks = pGhost->m_Path.Size();
	GetPath(pEntry->m_aFilename, sizeof(pEntry->m_aFilename), pEntry->m_aPlayer, pEntry->m_Time);
	m_GhostRecorder.Start(pEntry->m_aFilename, Client()->GetCurrentMapName(), Client()->GetMapCrc(), pEntry->m_aPlayer);

	m_GhostRecorder.WriteData(GHOSTDATA_TYPE_START_TICK, (const char*)&pGhost->m_StartTick, sizeof(int));
	m_GhostRecorder.WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&pGhost->m_Skin, sizeof(CGhostSkin));
	if(pGhost->m_Team != -1)
		m_GhostRecorder.WriteData(GHOSTDATA_TYPE_TEAM, (const char*)&pGhost->m_Team, sizeof(int));
	for(int i = 0; i < NumTicks; i++)
		m_GhostRecorder.WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)pGhost->m_Path.Get(i), sizeof(CGhostCharacter));

	m_GhostRecorder.Stop(NumTicks, pEntry->m_Time);
}

void CGhost::ConGPlay(IConsole::IResult *pResult, void *pUserData)
{
	CGhost *pGhost = (CGhost *)pUserData;
	pGhost->StartRender(pGhost->Client()->PredGameTick());
}

void CGhost::OnInit()
{
	m_GhostLoader.Init(Console(), Storage());
	m_GhostRecorder.Init(Console(), Storage());
}

void CGhost::OnConsoleInit()
{
	Console()->Register("gplay", "", CFGFLAG_CLIENT, ConGPlay, this, "");
}

void CGhost::OnMessage(int MsgType, void *pRawMsg)
{
	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_LocalClientID)
		{
			if(m_Recording)
				StopRecord();
			StopRender();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RACEFINISH)
	{
		CNetMsg_Sv_RaceFinish *pMsg = (CNetMsg_Sv_RaceFinish *)pRawMsg;
		if(pMsg->m_ClientID == m_pClient->m_LocalClientID)
		{
			if(m_Recording)
				StopRecord(pMsg->m_Time);
			StopRender();
		}
	}
}

void CGhost::OnReset()
{
	m_Loaded = false;
	m_SymmetricMap = false;
	m_Middle = -1;
	StopRecord();
	StopRender();
	UnloadAll();
}

void CGhost::LoadGhosts()
{
	m_Loaded = true;
	m_pClient->m_pMenus->GhostlistPopulate(false);

	if(!(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS))
		return;

	// symmetry check
	CTile *pGameTiles = static_cast<CTile *>(Layers()->Map()->GetData(Layers()->GameLayer()->m_Data));

	int aFlagIndex[2] = {-1, -1};
	int Width = m_pClient->Collision()->GetWidth();
	int Height = m_pClient->Collision()->GetHeight();
	for(int i = 0; i < Width*Height; i++)
	{
		if(pGameTiles[i].m_Index - ENTITY_OFFSET == ENTITY_FLAGSTAND_RED)
			aFlagIndex[TEAM_RED] = i;
		else if(pGameTiles[i].m_Index - ENTITY_OFFSET == ENTITY_FLAGSTAND_BLUE)
			aFlagIndex[TEAM_BLUE] = i;
		i += pGameTiles[i].m_Skip;
	}

	ivec2 RedPos = ivec2(aFlagIndex[TEAM_RED] % Width, aFlagIndex[TEAM_RED] / Width);
	ivec2 BluePos = ivec2(aFlagIndex[TEAM_BLUE] % Width, aFlagIndex[TEAM_BLUE] / Width);
	int MiddleLeft = (RedPos.x + BluePos.x) / 2;
	int MiddleRight = (RedPos.x + BluePos.x + 1) / 2;
	int Half = min(MiddleLeft, Width - MiddleRight - 1);

	m_Middle = (RedPos.x + BluePos.x) * 32 / 2 + 16;

	if(RedPos.y != BluePos.y)
		return;

	for(int y = 0; y < Height; y++)
	{
		int LeftOffset = y * Width + MiddleLeft;
		int RightOffset = y * Width + MiddleRight;
		for(int x = 0; x <= Half; x++)
		{
			int LeftIndex = pGameTiles[LeftOffset - x].m_Index;
			int RightIndex = pGameTiles[RightOffset + x].m_Index;
			if(LeftIndex != RightIndex && LeftIndex <= 128 && RightIndex <= 128)
				return;
		}
	}

	m_SymmetricMap = true;
}

bool CGhost::AutoMirroring() const
{
	return Config()->m_ClGhostAutoMirror && (m_SymmetricMap || Config()->m_ClGhostForceMirror);
}

void CGhost::OnTeamJoin(int Team)
{
	if(AutoMirroring())
		for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
			m_aActiveGhosts[i].AutoMirror(Team);
}
