/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>

#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "ghost.h"
#include "menus.h"

/*
TODO:
button for saving
decrease toggle button size
improve footer
enable ghosts when following someone
add ghost settings
*/

int CMenus::GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	int Length = str_length(pName);
	if(IsDir || Length < 4 || str_comp(pName+Length-4, ".gho") != 0)
		return 0;

	CGhostFile File;
	str_format(File.m_aFilename, sizeof(File.m_aFilename), "%s/%s", CGhost::ms_pGhostDir, pName);

	if(pSelf->m_pClient->m_pGhost->GhostLoader()->GetGhostInfo(File.m_aFilename, &File.m_Header))
		pSelf->m_lGhostFiles.add(File);
	return 0;
}

int CMenus::ScanGhostsThread(void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	pSelf->m_lGhostFiles.clear();
	pSelf->Storage()->ListDirectory(IStorage::TYPE_ALL, CGhost::ms_pGhostDir, GhostlistFetchCallback, pSelf);
	return 0;
}

void CMenus::GhostlistUpdate()
{
	m_LoadingGhosts = false;

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	int Own = -1;
	for(int i = 0; i < m_lGhostFiles.size(); i++)
	{
		CGhostHeader *pHeader = &m_lGhostFiles[i].m_Header;
		int Time = bytes_be_to_uint(pHeader->m_aTime);
		if(Time > 0 && str_comp(pHeader->m_aMap, Client()->GetCurrentMapName()) == 0)
		{
			CGhostEntry Entry;
			str_copy(Entry.m_aFilename, m_lGhostFiles[i].m_aFilename, sizeof(Entry.m_aFilename));
			str_copy(Entry.m_aPlayer, pHeader->m_aOwner, sizeof(Entry.m_aPlayer));
			Entry.m_Time = Time;
			Entry.m_AutoDelete = Config()->m_ClDeleteOldGhosts;
			int Index = m_lGhosts.add_unsorted(Entry);

			if(str_comp(Entry.m_aPlayer, Config()->m_PlayerName) == 0 && (Own == -1 || Entry < m_lGhosts[Own]))
				Own = Index;
		}
	}

	if(Own != -1)
	{
		m_lGhosts[Own].m_Own = true;
		m_lGhosts[Own].m_Slot = m_pClient->m_pGhost->Load(m_lGhosts[Own].m_aFilename);
	}

	if(m_lGhosts.size() > 0)
		m_lGhosts.sort_range();
}

void CMenus::GhostlistPopulate(bool ForceReload)
{
	if(!m_LoadingGhosts)
	{
		m_pClient->m_pGhost->UnloadAll();
		m_lGhosts.clear();
		if(ForceReload || m_lGhostFiles.size() == 0)
		{
			m_LoadingGhosts = true;
			m_pClient->Engine()->AddJob(&m_ScanGhostsJob, CMenus::ScanGhostsThread, this);
		}
		else
		{
			GhostlistUpdate();
		}
	}
}

const CGhostEntry *CMenus::GetOwnGhost() const
{
	for(int i = 0; i < m_lGhosts.size(); i++)
		if(m_lGhosts[i].m_Own)
			return &m_lGhosts[i];
	return 0;
}

void CMenus::UpdateOwnGhost(CGhostEntry Entry)
{
	int Own = -1;
	for(int i = 0; i < m_lGhosts.size(); i++)
		if(m_lGhosts[i].m_Own)
			Own = i;

	if(Own != -1)
	{
		m_lGhosts[Own].m_Slot = -1;
		m_lGhosts[Own].m_Own = false;
		if(m_lGhosts[Own].m_AutoDelete && (Entry.HasFile() || !m_lGhosts[Own].HasFile()))
			DeleteGhostEntry(Own);
	}

	Entry.m_Own = true;
	m_lGhosts.add(Entry);

	// add to file list
	if(!m_LoadingGhosts && Entry.HasFile())
	{
		CGhostFile File;
		str_copy(File.m_aFilename, Entry.m_aFilename, sizeof(File.m_aFilename));
		if(m_pClient->m_pGhost->GhostLoader()->GetGhostInfo(File.m_aFilename, &File.m_Header))
			m_lGhostFiles.add(File);
	}
}

void CMenus::DeleteGhostEntry(int Index)
{
	if(m_lGhosts[Index].HasFile())
		Storage()->RemoveFile(m_lGhosts[Index].m_aFilename, IStorage::TYPE_SAVE);
	// remove from file list
	if(!m_LoadingGhosts && m_lGhosts[Index].HasFile())
	{
		for(int i = 0; i < m_lGhostFiles.size(); i++)
		{
			if(str_comp(m_lGhostFiles[i].m_aFilename, m_lGhosts[Index].m_aFilename) == 0)
			{
				m_lGhostFiles.remove_index_fast(i);
				break;
			}
		}
	}
	m_lGhosts.remove_index(Index);
}

void CMenus::RenderGhost(CUIRect MainView)
{
	bool FastCap = m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_RACE && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_TEAMS;

	const float ButtonHeight = 18.0f;
	const float Spacing = 2.0f;
	const float NameWidth = 250.0f;
	const float TimeWidth = 200.0f;
	CUIRect Label, Row, Footer;
	MainView.HSplitBottom(80.0f, &MainView, 0);
	MainView.HSplitTop(20.0f, 0, &MainView);
	RenderTools()->DrawUIRect(&MainView, vec4(0.0f, 0.0f, 0.0f, Config()->m_ClMenuAlpha/100.0f), CUI::CORNER_ALL, 5.0f);

	MainView.HSplitTop(ButtonHeight, &Label, &MainView);
	Label.y += 2.0f;
	UI()->DoLabel(&Label, Localize("Ghosts"), ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_CENTER);
	RenderTools()->DrawUIRect(&MainView, vec4(0.0, 0.0, 0.0, 0.25f), CUI::CORNER_ALL, 5.0f);

	// prepare headline
	MainView.HSplitTop(ButtonHeight, &Row, &MainView);

	// background
	MainView.HSplitBottom(28.0f, &MainView, &Footer);
	RenderTools()->DrawUIRect(&MainView, vec4(0.0, 0.0, 0.0, 0.25f), CUI::CORNER_ALL, 5.0f);
	MainView.Margin(5.0f, &MainView);

	// prepare scroll
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0, 0);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ClipBgColor = vec4(0,0,0,0);
	ScrollParams.m_ScrollbarBgColor = vec4(0,0,0,0);
	ScrollParams.m_ScrollUnit = ButtonHeight * 3;
	if(s_ScrollRegion.IsScrollbarShown())
		Row.VSplitRight(ScrollParams.m_ScrollbarWidth, &Row, 0);

	// headline
	Row.VSplitLeft(ButtonHeight*2+Spacing*2, 0, &Row);
	Row.VSplitLeft(NameWidth, &Label, &Row);
	Label.y += 2.0f;
	UI()->DoLabel(&Label, Localize("Name"), ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_LEFT);

	Row.VSplitLeft(Spacing, 0, &Row);
	Row.VSplitLeft(TimeWidth, &Label, &Row);
	Label.y += 2.0f;
	UI()->DoLabel(&Label, Localize("Time"), ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_LEFT);

	if(FastCap)
	{
		Row.VSplitRight(Spacing*3+10.0f, &Row, 0);
		Row.VSplitRight(60.0f, &Row, &Label);
		Label.y += 2.0f;
		UI()->DoLabel(&Label, Localize("Mirrored"), ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_CENTER);
	}

	// scroll, ignore margins
	MainView.Margin(-5.0f, &MainView);

	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.Margin(5.0f, &MainView);
	MainView.y += ScrollOffset.y;

	const CGhostEntry *pOwnGhost = GetOwnGhost();
	bool OwnActive = pOwnGhost && pOwnGhost->Active();
	int FreeSlots = m_pClient->m_pGhost->FreeSlots();

	int NumGhosts = m_lGhosts.size();
	for(int i = 0; i < NumGhosts; i++)
	{
		CGhostEntry *pEntry = &m_lGhosts[i];

		vec3 Color = vec3(1.0f, 1.0f, 1.0f);
		if(pEntry->m_Own)
			Color = HslToRgb(vec3(0.33f, 1.0f, 0.75f));

		TextRender()->TextColor(Color.r, Color.g, Color.b, pEntry->HasFile() ? 1.0f : 0.5f);

		MainView.HSplitTop(ButtonHeight, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);

		int Inside = UI()->MouseInside(&Row);

		if(Inside)
			RenderTools()->DrawUIRect(&Row, vec4(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 5.0f);

		Row.VSplitLeft(2*Spacing, 0, &Row);
		Row.VSplitLeft(ButtonHeight, &Label, &Row);

		int ReservedSlots = !pEntry->m_Own && !OwnActive;
		bool BtnActive = pEntry->Active() || FreeSlots > ReservedSlots;
		if(DoButton_Toggle(&pEntry->m_ButtonActiveID, pEntry->Active(), &Label, BtnActive))
		{
			if(pEntry->Active())
			{
				m_pClient->m_pGhost->Unload(pEntry->m_Slot);
				pEntry->m_Slot = -1;
			}
			else
			{
				pEntry->m_Slot = m_pClient->m_pGhost->Load(pEntry->m_aFilename);
			}
		}

		Row.VSplitLeft(ButtonHeight, 0, &Row);
		Row.VSplitLeft(NameWidth, &Label, &Row);
		Label.y += 2.0f;
		UI()->DoLabel(&Label, pEntry->m_aPlayer, ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_LEFT);

		char aTime[32];
		FormatTime(aTime, sizeof(aTime), pEntry->m_Time, 3);
		Row.VSplitLeft(Spacing, 0, &Row);
		Row.VSplitLeft(TimeWidth, &Label, &Row);
		Label.y += 2.0f;
		UI()->DoLabel(&Label, aTime, ButtonHeight*ms_FontmodHeight*0.8f, CUI::ALIGN_LEFT);

		Row.VSplitRight(2*Spacing, &Row, 0);
		Row.VSplitRight(10.0f, &Row, &Label);
		if(Inside)
		{
			Label.HMargin((Label.h - Label.w) / 2, &Label);
			DoIcon(IMAGE_TOOLICONS, UI()->MouseHovered(&Label) ? SPRITE_TOOL_X_A : SPRITE_TOOL_X_B, &Label);
			static int s_DeleteButton = 0;
			if(UI()->DoButtonLogic(&s_DeleteButton, &Label))
			{
				if(pEntry->Active())
					m_pClient->m_pGhost->Unload(pEntry->m_Slot);
				DeleteGhostEntry(i);
			}
		}

		if(FastCap)
		{
			Row.VSplitRight(Spacing+ButtonHeight, &Row, 0);
			Row.VSplitRight(ButtonHeight, &Row, &Label);

			if(pEntry->Active())
			{
				if(DoButton_Toggle(&pEntry->m_ButtonMirroredID, m_pClient->m_pGhost->IsMirrored(pEntry->m_Slot), &Label, true))
					m_pClient->m_pGhost->ToggleMirror(pEntry->m_Slot);
			}
		}
	}

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	s_ScrollRegion.End();

	Footer.VSplitLeft(120.0f, &Label, &Footer);

	static CButtonContainer s_ReloadButton;
	if(DoButton_Menu(&s_ReloadButton, m_LoadingGhosts ? Localize("Loading") : Localize("Reload"), m_LoadingGhosts, &Label))
		GhostlistPopulate(true);

	/*bool Recording = m_pClient->m_pGhost->GhostRecorder()->IsRecording();
	if(!pGhost->HasFile() && !Recording && pGhost->Active())
	{
		static CButtonContainer s_SaveButton;
		Status.VSplitRight(120.0f, &Status, &Button);
		if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button))
			m_pClient->m_pGhost->SaveGhost(pGhost);
	}*/

	if(FastCap && !m_pClient->m_pGhost->IsMapSymmetric())
	{
		Footer.VSplitRight(250.0f, &Footer, &Label);
		UI()->DoLabel(&Label, Localize("Warning: asymmetric map"), 14.0f, CUI::ALIGN_CENTER);
	}
}

