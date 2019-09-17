/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <engine/console.h>
#include <engine/storage.h>

#include "map/map.h"

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	dbg_logger_debugger();
	
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
	if(!pStorage || argc != 3)
		return -1;

	CEditorMap Map;
	Map.m_pConsole = CreateConsole(0);
	if(!Map.Load(pStorage, argv[1], IStorage::TYPE_ALL))
		return -1;

	if(!Map.Save(pStorage, argv[2]))
		return -1;

	return 0;
}
