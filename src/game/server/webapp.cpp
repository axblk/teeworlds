/* Webapp class by Sushi and Redix */
#if defined(CONF_TEERACE)

#include <stdio.h>

#include <base/tl/algorithm.h>
#include <engine/external/json-parser/json-builder.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/http.h>
#include <engine/storage.h>

#include <game/teerace.h>
#include <game/version.h>

#include "gamecontext.h"
#include "webapp.h"
#include "score.h"

IServer *CServerWebapp::Server() { return m_pGameServer->Server(); }
IScore *CServerWebapp::Score() { return m_pGameServer->Score(); }
IStorage *CServerWebapp::Storage() { return m_pGameServer->Storage(); }

CBufferRequest *CServerWebapp::CreateAuthedApiRequest(int Method, const char *pURI)
{
	CBufferRequest *pRequest = ITeerace::CreateApiRequest(Method, pURI);
	RegisterFields(pRequest);
	return pRequest;
}

void CServerWebapp::RegisterFields(IRequest *pRequest)
{
	pRequest->AddField("API-AUTH", g_Config.m_WaApiKey);
	pRequest->AddField("API-GAMESERVER-VERSION", TEERACE_GAMESERVER_VERSION);
}

void CServerWebapp::CheckStatusCode(IConsole *pConsole, class IResponse *pResponse)
{
	if(!pResponse)
		return;
	if(pResponse->StatusCode() == 432)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "webapp",
			"This server is outdated and cannot fully cooperate with Teerace, hence its support is currently disabled. Please notify the server administrator.");
	}
	else if(pResponse->StatusCode() == 403)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "webapp",
			"This server was denied access to Teerace network. Please notify the server administator.");
	}
}

void CServerWebapp::Download(const char *pFilename, const char *pURI, FHttpCallback pfnCallback)
{
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	CBufferRequest *pRequest = new CBufferRequest(IRequest::HTTP_GET, pURI);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host(), File, pFilename);
	pInfo->SetCallback(pfnCallback, this);
	pInfo->SetPriority(HTTP_PRIORITY_LOW);
	Server()->SendHttp(pInfo, pRequest);
}

void CServerWebapp::Upload(const char *pFilename, const char *pURI, const char *pUploadName, FHttpCallback pfnCallback)
{
	CUploadData *pUserData = new CUploadData(this);
	str_copy(pUserData->m_aFilename, pFilename, sizeof(pUserData->m_aFilename));
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	CFileRequest *pRequest = ITeerace::CreateApiUpload(pURI);
	RegisterFields(pRequest);
	pRequest->SetFile(File, pFilename, pUploadName);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(pfnCallback, pUserData);
	pInfo->SetPriority(HTTP_PRIORITY_LOW);
	Server()->SendHttp(pInfo, pRequest);
}

CServerWebapp::CServerWebapp(CGameContext *pGameServer) : m_LastPing(-1), m_pGameServer(pGameServer), m_LastMapListLoad(-1)
{
	// load maps
	Storage()->ListDirectory(IStorage::TYPE_SAVE, "maps/teerace", MaplistFetchCallback, this);
	m_lUploads.clear();
}

void CServerWebapp::OnUserAuth(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUserAuthData *pUser = (CUserAuthData*)pUserData;
	CServerWebapp *pWenapp = pUser->m_pWebapp;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWenapp->GameServer()->Console(), pResponse);

	int ClientID = pUser->m_ClientID;
	if(pWenapp->GameServer()->m_apPlayers[ClientID])
	{
		if(Error)
		{
			pWenapp->GameServer()->SendChatTarget(ClientID, "unknown error");
			delete pUser;
			return;
		}

		int SendRconCmds = pUser->m_SendRconCmds;
		int UserID = 0;

		json_settings JsonSettings;
		mem_zero(&JsonSettings, sizeof(JsonSettings));
		char aError[256];

		const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
		json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
		if(!pJsonData)
			dbg_msg("json", aError);

		if(str_comp(pBody, "false") != 0 && pJsonData)
			UserID = (*pJsonData)["id"].u.integer;

		if(UserID > 0)
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "%s has logged in as %s", pWenapp->Server()->ClientName(ClientID), (const char*)(*pJsonData)["username"]);
			pWenapp->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			pWenapp->Server()->SetUserID(ClientID, UserID);
			pWenapp->Server()->SetUserName(ClientID, (*pJsonData)["username"]);

			// auth staff members
			if((bool)(*pJsonData)["is_staff"])
				pWenapp->Server()->StaffAuth(ClientID, SendRconCmds);

			pWenapp->Score()->LoadScore(ClientID, true);
		}
		else
		{
			pWenapp->GameServer()->SendChatTarget(ClientID, "wrong username and/or password");
		}
		json_value_free(pJsonData);
	}
	delete pUser;
}

void CServerWebapp::OnPingPing(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUserData;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	dbg_msg("webapp", "webapp is%s online", Error ? " not" : "");
	if(Error)
		return;

	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];

	const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
	json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
	if(pJsonData)
	{
		const json_value &Awards = (*pJsonData)["awards"];
		for(unsigned int i = 0; i < Awards.u.array.length; i++)
		{
			const json_value &Award = Awards[i];
			int UserID = Award["user_id"].u.integer;

			if(!UserID)
				continue;

			// show awards to everyone only if the player is there
			for(int j = 0; j < MAX_CLIENTS; j++)
			{
				if(UserID != pWebapp->Server()->GetUserID(j))
					continue;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s achieved award \"%s\".", pWebapp->Server()->ClientName(j), (const char*)Award["name"]);
				pWebapp->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
		}
	}
	else
		dbg_msg("json", aError);
	json_value_free(pJsonData);
}

void CServerWebapp::OnMapList(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUserData;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(Error)
		return;

	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];

	const char *pBody = ((CBufferResponse*)pResponse)->GetBody();
	json_value *pJsonData = json_parse_ex(&JsonSettings, pBody, pResponse->Size(), aError);
	if(pJsonData)
	{
		char aFilename[256];
		const char *pPath = "/maps/teerace/%s.map";
		bool Change = false;

		for(unsigned int i = 0; i < pJsonData->u.array.length; i++)
		{
			const json_value &Map = (*pJsonData)[i];
			if(Map["crc"].type != json_string)
				continue; // skip maps without crc

			CMapInfo Info;
			str_copy(Info.m_aName, Map["name"], sizeof(Info.m_aName));
			sscanf(Map["crc"], "%08x", &Info.m_Crc);
			Info.m_ID = Map["id"].u.integer;
			Info.m_RunCount = Map["run_count"].u.integer;
			str_copy(Info.m_aURL, Map["get_download_url"], sizeof(Info.m_aURL));
			str_copy(Info.m_aAuthor, Map["author"], sizeof(Info.m_aAuthor));

			array<CMapInfo>::range r = find_linear(pWebapp->m_lMapList.all(), Info);
			if(r.empty()) // new entry
			{
				Info.m_State = CMapInfo::MAPSTATE_DOWNLOADING;
				pWebapp->m_lMapList.add(Info);
				dbg_msg("webapp", "added map info: '%s' (%d)", Info.m_aName, Info.m_ID);

				str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
				pWebapp->Download(aFilename, Map["get_download_url"], OnDownloadMap);
			}
			else if(r.front().m_Crc != Info.m_Crc) // we have a wrong version
			{
				Info.m_State = CMapInfo::MAPSTATE_DOWNLOADING;
				r.front() = Info;
				dbg_msg("webapp", "updated map info: '%s' (%d)", Info.m_aName, Info.m_ID);

				str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
				pWebapp->Download(aFilename, Map["get_download_url"], OnDownloadMap);
			}
			else if(r.front().m_Crc == Info.m_Crc) // we already have this 
			{
				if(r.front().m_State == CMapInfo::MAPSTATE_INFO_MISSING)
				{
					Info.m_State = CMapInfo::MAPSTATE_COMPLETE;
					r.front() = Info;
					dbg_msg("webapp", "added map info: '%s' (%d)", r.front().m_aName, r.front().m_ID);
					Change = true;
				}
				else if(r.front().m_State == CMapInfo::MAPSTATE_FILE_MISSING)
				{
					r.front().m_State = CMapInfo::MAPSTATE_DOWNLOADING;
					str_format(aFilename, sizeof(aFilename), pPath, Info.m_aName);
					pWebapp->Download(aFilename, Map["get_download_url"], OnDownloadMap);
				}
			}
		}

		if(Change)
		{
			pWebapp->m_lMapList.sort_range();
			pWebapp->OnInit();
		}
	}
	else
		dbg_msg("json", aError);
	json_value_free(pJsonData);
}

void CServerWebapp::OnDownloadMap(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CFileResponse *pRes = (CFileResponse*)pResponse;
	CServerWebapp *pWebapp = (CServerWebapp*)pUserData;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(!Error)
	{
		CMapInfo *pInfo = pWebapp->GameServer()->Webapp()->AddMap(pRes->GetFilename(), pRes->GetCrc());
		if(pInfo && str_comp(pInfo->m_aName, g_Config.m_SvMap) == 0)
			pWebapp->Server()->ReloadMap();
		if(!pInfo)
			pWebapp->Storage()->RemoveFile(pRes->GetPath(), IStorage::TYPE_SAVE);
	}
	else
	{
		pWebapp->GameServer()->Storage()->RemoveFile(pRes->GetPath(), IStorage::TYPE_SAVE);
		dbg_msg("webapp", "could not download map: '%s'", pRes->GetPath());
	}
}

void CServerWebapp::OnUploadFile(IResponse *pResponse, bool ConnError, void *pUserData)
{
	CUploadData *pUser = (CUploadData*)pUserData;
	CServerWebapp *pWebapp = pUser->m_pWebapp;
	bool Error = ConnError || pResponse->StatusCode() != 200;
	CheckStatusCode(pWebapp->GameServer()->Console(), pResponse);

	if(!Error)
		dbg_msg("webapp", "uploaded file: '%s'", pUser->m_aFilename);
	else
		dbg_msg("webapp", "could not upload file: '%s'", pUser->m_aFilename);
	pWebapp->Storage()->RemoveFile(pUser->m_aFilename, IStorage::TYPE_SAVE);
	delete pUser;
}

int CServerWebapp::MaplistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CServerWebapp *pWebapp = (CServerWebapp*)pUser;
	int Length = str_length(pName);
	if(!IsDir && Length >= 4 && str_comp(pName + Length - 4, ".map") == 0)
	{
		char aFile[256];
		str_format(aFile, sizeof(aFile), "maps/teerace/%s", pName);
		unsigned MapCrc = 0;
		unsigned MapSize = 0;
		if(!CDataFileReader::GetCrcSize(pWebapp->Storage(), aFile, IStorage::TYPE_SAVE, &MapCrc, &MapSize))
			return 0;
		pWebapp->AddMap(pName, MapCrc);
	}
	return 0;
}

CServerWebapp::CMapInfo *CServerWebapp::AddMap(const char *pFilename, unsigned Crc)
{
	CMapInfo Info;
	Info.m_Crc = Crc;
	str_copy(Info.m_aName, pFilename, min((int)sizeof(Info.m_aName),str_length(pFilename)-3));

	array<CMapInfo>::range r = find_linear(m_lMapList.all(), Info);
	if(r.empty()) // new entry
	{
		Info.m_State = CMapInfo::MAPSTATE_INFO_MISSING;
		int Num = m_lMapList.add(Info);
		dbg_msg("", "added map: '%s' (%08x)", Info.m_aName, Info.m_Crc);
		return &m_lMapList[Num];
	}
	else if(r.front().m_State == CMapInfo::MAPSTATE_DOWNLOADING) // entry already exists
	{
		if(r.front().m_Crc == Info.m_Crc)
		{
			r.front().m_State = CMapInfo::MAPSTATE_COMPLETE;
			dbg_msg("", "added map: '%s' (%08x)", Info.m_aName, Info.m_Crc);
			m_lMapList.sort_range();
			AddMapVotes();
			return &r.front();
		}
		// something went wrong
		r.front().m_State = CMapInfo::MAPSTATE_FILE_MISSING;
	}
	return 0;
}

void CServerWebapp::OnInit()
{
	m_CurrentMap.m_ID = -1;
	str_copy(m_CurrentMap.m_aName, g_Config.m_SvMap, sizeof(m_CurrentMap.m_aName));
	array<CMapInfo>::range r = find_linear(m_lMapList.all(), m_CurrentMap);
	if(!r.empty() && r.front().m_State == CMapInfo::MAPSTATE_COMPLETE)
	{
		m_CurrentMap = r.front();
		dbg_msg("webapp", "current map: '%s' (%d)", m_CurrentMap.m_aName, m_CurrentMap.m_ID);

		// add votes
		AddMapVotes();
	}
}

void CServerWebapp::OnAuth(int ClientID, const char *pToken, int SendRconCmds)
{
	json_value *pData = json_object_new(1);
	json_object_push(pData, "api_token", json_string_new(pToken));
	char *pJson = new char[json_measure(pData)];
	json_serialize(pJson, pData);
	json_builder_free(pData);

	CUserAuthData *pUserData = new CUserAuthData(this);
	pUserData->m_ClientID = ClientID;
	pUserData->m_SendRconCmds = SendRconCmds;

	CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/users/auth_token/");
	pRequest->SetBody(pJson, str_length(pJson), "application/json");
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(CServerWebapp::OnUserAuth, pUserData);
	Server()->SendHttp(pInfo, pRequest);
	delete pJson;
}

void CServerWebapp::Tick()
{
	// do uploads
	for(int i = 0; i < m_lUploads.size(); i++)
	{
		if(m_lUploads[i].m_StartTime <= time_get())
		{
			Upload(m_lUploads[i].m_aFilename, m_lUploads[i].m_aURL, m_lUploads[i].m_aUploadname, m_lUploads[i].m_pfnCallback);
			m_lUploads.remove_index_fast(i);
			i--; // since one item was removed
		}
	}

	// load maplist every 20 minutes
	if(m_LastMapListLoad == -1 || m_LastMapListLoad + Server()->TickSpeed() * 60 * 20 < Server()->Tick())
	{
		LoadMapList();
		m_LastMapListLoad = Server()->Tick();
	}

	if(m_LastPing == -1 || m_LastPing+Server()->TickSpeed()*60 < Server()->Tick())
	{
		json_value *pData = json_object_new(3);
		json_value *pUsers = json_object_new(0);
		json_value *pAnonymous = json_array_new(0);
		int Num = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				if(Server()->GetUserID(i) > 0)
				{
					char aBuf[16];
					str_format(aBuf, sizeof(aBuf), "%d", Server()->GetUserID(i));
					char aName[MAX_NAME_LENGTH];
					str_copy(aName, Server()->ClientName(i), sizeof(aName));
					str_sanitize_strong(aName);
					json_object_push(pUsers, aBuf, json_string_new(aName));
				}
				else
				{
					char aName[MAX_NAME_LENGTH];
					str_copy(aName, Server()->ClientName(i), sizeof(aName));
					str_sanitize_strong(aName);
					json_array_push(pAnonymous, json_string_new(aName));
					Num++;
				}
			}
		}
		json_object_push(pData, "users", pUsers);
		json_object_push(pData, "anonymous", pAnonymous);
		json_object_push(pData, "map", json_string_new(g_Config.m_SvMap));
		char *pJson = new char[json_measure(pData)];
		json_serialize(pJson, pData);
		json_builder_free(pData);
			
		CBufferRequest *pRequest = CServerWebapp::CreateAuthedApiRequest(IRequest::HTTP_POST, "/ping/");
		pRequest->SetBody(pJson, str_length(pJson), "application/json");
		CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
		pInfo->SetCallback(CServerWebapp::OnPingPing, this);
		Server()->SendHttp(pInfo, pRequest);
		delete pJson;
			
		m_LastPing = Server()->Tick();
	}
}

void CServerWebapp::LoadMapList()
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "/maps/list/%s/", g_Config.m_WaMapTypes);

	CBufferRequest *pRequest = CreateAuthedApiRequest(IRequest::HTTP_GET, aBuf);
	CRequestInfo *pInfo = new CRequestInfo(ITeerace::Host());
	pInfo->SetCallback(OnMapList, this);
	Server()->SendHttp(pInfo, pRequest);
}

void CServerWebapp::AddUpload(const char *pFilename, const char *pURL, const char *pUploadName, FHttpCallback pfnCallback, int64 StartTime)
{
	m_lUploads.add(CUpload(pFilename, pURL, pUploadName, pfnCallback, StartTime));
}

void CServerWebapp::AddMapVotes()
{
	if(g_Config.m_WaAutoAddMaps && m_CurrentMap.m_ID != -1)
	{
		for(int i = 0; i < m_lMapList.size(); i++)
		{
			CMapInfo *pMapInfo = &m_lMapList[i];
			if(pMapInfo->m_State == CMapInfo::MAPSTATE_COMPLETE)
			{
				char aVoteDescription[128];
				if(str_find(g_Config.m_WaVoteDescription, "%s"))
					str_format(aVoteDescription, sizeof(aVoteDescription), g_Config.m_WaVoteDescription, pMapInfo->m_aName);
				else
					str_format(aVoteDescription, sizeof(aVoteDescription), "change map to %s", pMapInfo->m_aName);

				// check for duplicate entry
				int OptionFound = false;
				CVoteOptionServer *pOption = GameServer()->m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(aVoteDescription, pOption->m_aDescription) == 0)
					{
						OptionFound = true;
						break;
					}
					pOption = pOption->m_pNext;
				}

				if(OptionFound)
					continue;

				// add the option
				++GameServer()->m_NumVoteOptions;
				char aCommand[128];
				str_format(aCommand, sizeof(aCommand), "sv_map %s", pMapInfo->m_aName);
				int Len = str_length(aCommand);

				pOption = (CVoteOptionServer *)GameServer()->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
				pOption->m_pNext = 0;
				pOption->m_pPrev = GameServer()->m_pVoteOptionLast;
				if(pOption->m_pPrev)
					pOption->m_pPrev->m_pNext = pOption;
				GameServer()->m_pVoteOptionLast = pOption;
				if(!GameServer()->m_pVoteOptionFirst)
					GameServer()->m_pVoteOptionFirst = pOption;

				str_copy(pOption->m_aDescription, aVoteDescription, sizeof(pOption->m_aDescription));
				mem_copy(pOption->m_aCommand, aCommand, Len+1);
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

				// inform clients about added option
				CNetMsg_Sv_VoteOptionAdd OptionMsg;
				OptionMsg.m_pDescription = pOption->m_aDescription;
				GameServer()->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
			}
		}
	}
}

CServerWebapp::CUpload::CUpload(const char* pFilename, const char* pURL, const char* pUploadname, FHttpCallback pfnCallback, int64 StartTime)
	: m_pfnCallback(pfnCallback), m_StartTime(StartTime)
{
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	str_copy(m_aURL, pURL, sizeof(m_aURL));
	str_copy(m_aUploadname, pUploadname, sizeof(m_aUploadname));
}

#endif
