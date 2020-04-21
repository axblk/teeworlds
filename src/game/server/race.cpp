#include <base/math.h>
#include <base/color.h>

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/race.h>
#include <game/version.h>

#include "gamecontext.h"
#include "player.h"

#include "entities/character.h"

#include "gamemodes/race.h"
#include "score.h"

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int CID2 = clamp(pResult->GetInteger(1), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID1] && pSelf->m_apPlayers[CID2])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(CID1);
		if(pChr)
		{
			pChr->SetPos(pSelf->m_apPlayers[CID2]->m_ViewPos);
			pSelf->RaceController()->StopRace(CID1);
		}
		else
			pSelf->m_apPlayers[CID1]->m_ViewPos = pSelf->m_apPlayers[CID2]->m_ViewPos;
	}
}

void CGameContext::ConTeleportTo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID])
	{
		CCharacter* pChr = pSelf->GetPlayerChar(CID);
		vec2 TelePos = vec2(pResult->GetInteger(1), pResult->GetInteger(2));
		if(pChr)
		{
			pChr->SetPos(TelePos);
			pSelf->RaceController()->StopRace(CID);
		}
		else
			pSelf->m_apPlayers[CID]->m_ViewPos = TelePos;
	}
}

void CGameContext::ConGetPos(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[CID])
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s pos: %d @ %d", pSelf->Server()->ClientName(CID),
			(int)pSelf->m_apPlayers[CID]->m_ViewPos.x, (int)pSelf->m_apPlayers[CID]->m_ViewPos.y);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "race", aBuf);
	}
}

void CGameContext::ChatConInfo(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	CGameContext *pSelf = (CGameContext *)pComContext->m_pContext;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Race mod %s (C)Rajh, Redix and Sushi", RACE_VERSION);
	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, aBuf);
}

void CGameContext::ChatConTop5(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	CGameContext *pSelf = (CGameContext *)pComContext->m_pContext;

	if(!pSelf->Config()->m_SvShowTimes)
	{
		pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "Showing the Top5 is not allowed on this server.");
		return;
	}

	if(pResult->NumArguments() > 0)
		pSelf->Score()->ShowTop5(pComContext->m_ClientID, max(1, pResult->GetInteger(0)));
	else
		pSelf->Score()->ShowTop5(pComContext->m_ClientID);
}

void CGameContext::ChatConRank(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	CGameContext *pSelf = (CGameContext *)pComContext->m_pContext;

	if(pSelf->Config()->m_SvShowTimes && pResult->NumArguments() > 0)
	{
		char aStr[256];
		str_copy(aStr, pResult->GetString(0), sizeof(aStr));
		str_clean_whitespaces(aStr);
		pSelf->Score()->ShowRank(pComContext->m_ClientID, aStr);
	}
	else
		pSelf->Score()->ShowRank(pComContext->m_ClientID, pComContext->m_ClientID);
}

void CGameContext::ChatConShowOthers(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	CGameContext *pSelf = (CGameContext *)pComContext->m_pContext;

	if(!pSelf->Config()->m_SvShowOthers)
		pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "This command is not allowed on this server.");
	else
		pSelf->m_apPlayers[pComContext->m_ClientID]->ToggleShowOthers();
}

void CGameContext::ChatConHelp(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	CGameContext *pSelf = (CGameContext *)pComContext->m_pContext;

	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "---Command List---");
	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "\"/info\" Information about the mod");
	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "\"/rank\" Show your rank");
	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "\"/rank NAME\" Show the rank of a specific player");
	pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "\"/top5 X\" Show the top 5");
	if(pSelf->Config()->m_SvShowOthers)
		pSelf->SendChat(-1, CHAT_ALL, pComContext->m_ClientID, "\"/show_others\" Show/Hide other players");
}

void CGameContext::LoadMapSettings()
{
	if(m_Layers.SettingsLayer())
	{
		CMapItemLayerTilemap *pLayer = m_Layers.SettingsLayer();
		CTile *pTiles = static_cast<CTile *>(m_Layers.Map()->GetData(pLayer->m_Data));
		char *pCommand = new char[pLayer->m_Width+1];
		pCommand[pLayer->m_Width] = 0;

		for(int i = 0; i < pLayer->m_Height; i++)
		{
			for(int j = 0; j < pLayer->m_Width; j++)
				pCommand[j] = pTiles[i*pLayer->m_Width+j].m_Index;
			Console()->ExecuteLineFlag(pCommand, CFGFLAG_MAPSETTINGS);
		}

		delete[] pCommand;
		m_Layers.Map()->UnloadData(pLayer->m_Data);
	}
}

int64 CmaskRace(CGameContext *pGameServer, int Owner)
{
	int64 Mask = CmaskOne(Owner);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pGameServer->m_apPlayers[i] && pGameServer->m_apPlayers[i]->ShowOthers())
			Mask = Mask | CmaskOne(i);
	}

	return Mask;
}
