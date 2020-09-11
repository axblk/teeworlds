/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include "nameplates.h"
#include "controls.h"

void CNamePlates::RenderNameplate(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPlayerInfo,
	int ClientID
	)
{
	vec2 Position;
	if(m_pClient->ShouldUsePredicted() &&
		m_pClient->ShouldUsePredictedChar(ClientID))
	{
		Position = m_pClient->PredictedCharPos(ClientID);
	}
	else
	{
		Position = m_pClient->UnpredictedCharPos(ClientID);
	}

	float FontSize = 18.0f + 20.0f * Config()->m_ClNameplatesSize / 100.0f;
	// render name plate
	if(m_pClient->m_LocalClientID != ClientID)
	{
		float a = 1;
		if(Config()->m_ClNameplatesAlways == 0)
			a = clamp(1-powf(distance(m_pClient->m_pControls->m_TargetPos, Position)/200.0f,16.0f), 0.0f, 1.0f);


		char aName[64];
		str_format(aName, sizeof(aName), "%s", Config()->m_ClShowsocial ? m_pClient->m_aClients[ClientID].m_aName: "");

		// TODO: ADDBACK: draw nameplate

		TextRender()->TextColor(CUI::ms_DefaultTextColor);
		TextRender()->TextSecondaryColor(CUI::ms_DefaultTextOutlineColor);
	}
}

void CNamePlates::OnRender()
{
	if(!Config()->m_ClNameplates || Client()->State() < IClient::STATE_ONLINE)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// only render active characters
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);

		if(pInfo)
		{
			RenderNameplate(
				&m_pClient->m_Snap.m_aCharacters[i].m_Prev,
				&m_pClient->m_Snap.m_aCharacters[i].m_Cur,
				(const CNetObj_PlayerInfo *)pInfo,
				i);
		}
	}
}
