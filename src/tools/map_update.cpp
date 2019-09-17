/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <engine/console.h>
#include <engine/storage.h>

#include "map/map.h"

#include "map_update.h"

typedef void (*LAYER_MODIFY_FUNC)(CLayerGroup *pGroup, CLayerTiles *pLayer, const void *pUserData);

static IStorage *s_pStorage = 0;

int AddImage(CEditorMap *pMap, const char *pImageName)
{
	CEditorImage *pImg = new CEditorImage(pMap->m_pConsole);
	pImg->m_External = 1;

	LoadExternalImage(pMap->m_pConsole, s_pStorage, pImg, pImageName);
	str_copy(pImg->m_aName, pImageName, sizeof(pImg->m_aName));

	dbg_msg("mapupdate", "added '%s' image", pImageName);

	return pMap->m_lImages.add(pImg);
}

CLayerTiles *CreateLayer(CLayerTiles *pLayer, const char *pImageName)
{
	int NewImage = AddImage(pLayer->m_pMap, pImageName);
	if(NewImage == -1)
		return 0;

	CLayerTiles *pTiles = new CLayerTiles(pLayer->m_Width, pLayer->m_Height);
	pTiles->m_pMap = pLayer->m_pMap;
	pTiles->m_Color = pLayer->m_Color;
	pTiles->m_ColorEnv = pLayer->m_ColorEnv;
	pTiles->m_ColorEnvOffset = pLayer->m_ColorEnvOffset;

	pTiles->m_Image = NewImage;
	pTiles->m_Game = 0;
	str_copy(pTiles->m_aName, pLayer->m_aName, sizeof(pTiles->m_aName));

	return pTiles;
}

static void UpdateLayer(CLayerGroup *pGroup, CLayerTiles *pLayer, const CTilesetUpdateData *pUpdateData)
{
	CLayerTiles **ppNewLayers = new CLayerTiles*[pUpdateData->m_NumMoves];
	for(int i = 0; i < pUpdateData->m_NumMoves; i++)
		ppNewLayers[i] = 0;

	int ReplacedTiles = 0;
	int RemovedTiles = 0;
	int MovedTiles = 0;

	bool Empty = true;

	for(int Count = 0; Count < pLayer->m_Height*pLayer->m_Width; Count++)
	{
		bool Modified = false;
		for(int TileIndex = 0; !Modified && TileIndex < pUpdateData->m_NumReplacedTiles; TileIndex++)
		{
			if(pLayer->m_pTiles[Count].m_Index == pUpdateData->m_pOldData[TileIndex])
			{
				pLayer->m_pTiles[Count].m_Index = pUpdateData->m_pNewData[TileIndex];
				ReplacedTiles++;
				Modified = true;
			}
		}

		for(int TileIndex = 0; !Modified && TileIndex < pUpdateData->m_NumRemovedTiles; TileIndex++)
		{
			if(pLayer->m_pTiles[Count].m_Index == pUpdateData->m_pRemoveData[TileIndex])
			{
				pLayer->m_pTiles[Count].m_Index = 0;
				RemovedTiles++;
				Modified = true;
			}
		}

		for(int i = 0; !Modified && i < pUpdateData->m_NumMoves; i++)
		{
			const CTilesetMoveData *pMoveData = pUpdateData->m_pMoveData;
			for(int TileIndex = 0; !Modified && TileIndex < pMoveData->m_NumMovedTiles; TileIndex++)
			{
				if(pLayer->m_pTiles[Count].m_Index == pMoveData->m_pOldData[TileIndex])
				{
					if(!ppNewLayers[i])
						ppNewLayers[i] = CreateLayer(pLayer, pMoveData->m_pImageName);
					if(ppNewLayers[i])
					{
						ppNewLayers[i]->m_pTiles[Count].m_Index = pMoveData->m_pNewData[TileIndex];
						ppNewLayers[i]->m_pTiles[Count].m_Flags = pLayer->m_pTiles[Count].m_Flags;
						pLayer->m_pTiles[Count].m_Index = 0;
						MovedTiles++;
						Modified = true;
					}
				}
			}
		}

		if(pLayer->m_pTiles[Count].m_Index != 0)
			Empty = false;
	}

	if(ReplacedTiles > 0 || RemovedTiles > 0)
		dbg_msg("mapupdate", "replaced %d and removed %d tiles in '%s' layer", ReplacedTiles, RemovedTiles, pUpdateData->m_pImageName);

	if(MovedTiles > 0)
	{
		dbg_msg("mapupdate", "moved %d tiles from '%s' to new layers", MovedTiles, pUpdateData->m_pImageName);

		array<CLayer*>::range r = find_linear(pGroup->m_lLayers.all(), pLayer);
		for(int i = 0; i < pUpdateData->m_NumMoves; i++)
			if(ppNewLayers[i])
			{
				const CTilesetMoveData *pMoveData = pUpdateData->m_pMoveData;
				dbg_msg("mapupdate", "moved tiles from '%s' to '%s' layer", pUpdateData->m_pImageName, pMoveData->m_pImageName);
				pGroup->m_lLayers.insert(ppNewLayers[i], find_linear(r, pLayer));
			}

		if(Empty)
		{
			pGroup->DeleteLayer(pLayer);
			dbg_msg("mapupdate", "removed empty layer (all tiles moved to new layer)");
		}
	}

	delete[] ppNewLayers;
}

void UpdateLayers(CEditorMap *pMap, int ImageID, const CTilesetUpdateData *pUpdateData)
{
	for(int g = 0; g < pMap->m_lGroups.size(); g++)
	{
		CLayerGroup *pGroup = pMap->m_lGroups[g];
		for(int i = 0; i < pGroup->m_lLayers.size(); i++)
		{
			if(pGroup->m_lLayers[i]->m_Type == LAYERTYPE_TILES)
			{
				CLayerTiles *pLayer = static_cast<CLayerTiles *>(pGroup->m_lLayers[i]);
				if(pLayer->m_Image == ImageID)
					UpdateLayer(pGroup, pLayer, pUpdateData);
			}
		}
	}
}

void UpdateMap(CEditorMap *pMap)
{
	// find image
	for(int i = 0; i < pMap->m_lImages.size(); ++i)
		for(int Index = 0; Index < ARRAY_SIZE(s_TilesetUpdateData); Index++)
			if(!str_comp(pMap->m_lImages[i]->m_aName, s_TilesetUpdateData[Index].m_pImageName))
				UpdateLayers(pMap, i, &s_TilesetUpdateData[Index]);
}

void RemoveUnusedImages(CEditorMap *pMap)
{
	int Removed = 0;

	for(int i = 0; i < pMap->m_lImages.size(); ++i)
	{
		// check if images is used
		bool Used = false;
		for(int g = 0; !Used && (g < pMap->m_lGroups.size()); g++)
		{
			CLayerGroup *pGroup = pMap->m_lGroups[g];
			for(int l = 0; !Used && (l < pGroup->m_lLayers.size()); l++)
			{
				if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
				{
					CLayerTiles *pLayer = static_cast<CLayerTiles *>(pGroup->m_lLayers[l]);
					if(pLayer->m_Image == i)
						Used = true;
				}
				else if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_QUADS)
				{
					CLayerQuads *pLayer = static_cast<CLayerQuads *>(pGroup->m_lLayers[l]);
					if(pLayer->m_Image == i)
						Used = true;
				}
			}
		}

		if(!Used)
		{
			delete pMap->m_lImages[i];
		    pMap->m_lImages.remove_index(i);
		    gs_ModifyIndexDeletedIndex = i;
		    pMap->ModifyImageIndex(ModifyIndexDeleted);
			Removed++;
			i--;
		}
	}

	if(Removed > 0)
		dbg_msg("mapupdate", "removed %d unused images", Removed);
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	dbg_logger_debugger();

	s_pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
	if(!s_pStorage || argc != 3)
		return -1;

	CEditorMap Map;
	Map.m_pConsole = CreateConsole(0);
	if(!Map.Load(s_pStorage, argv[1], IStorage::TYPE_ALL, MOD_RACE))
		return -1;

	UpdateMap(&Map);
	RemoveUnusedImages(&Map);

	if(!Map.Save(s_pStorage, argv[2]))
		return -1;

	return 0;
}
