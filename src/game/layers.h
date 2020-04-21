/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H

#include <engine/map.h>
#include <game/mapitems.h>

class CLayers
{
	int m_GroupsNum;
	int m_GroupsStart;
	int m_LayersNum;
	int m_LayersStart;
	CMapItemGroup *m_pGameGroup;
	CMapItemLayerTilemap *m_pGameLayer;
	CMapItemLayerTilemap *m_pGameExLayer;
	CMapItemLayerTilemap *m_pSettingsLayer;
	CMapItemLayerTilemap *m_pTeleLayer;
	CMapItemLayerTilemap *m_pSpeedupForceLayer;
	CMapItemLayerTilemap *m_pSpeedupAngleLayer;
	class IMap *m_pMap;
	void InitTilemapSkip();

public:
	CLayers();
	void Init(class IKernel *pKernel, class IMap *pMap=0);
	int NumGroups() const { return m_GroupsNum; };
	int NumLayers() const { return m_LayersNum; };
	class IMap *Map() const { return m_pMap; };
	CMapItemGroup *GameGroup() const { return m_pGameGroup; };
	CMapItemLayerTilemap *GameLayer() const { return m_pGameLayer; };
	CMapItemLayerTilemap *GameExLayer() const { return m_pGameExLayer; }
	CMapItemLayerTilemap *SettingsLayer() const { return m_pSettingsLayer; }
	CMapItemLayerTilemap *TeleLayer() const { return m_pTeleLayer; }
	CMapItemLayerTilemap *SpeedupForceLayer() const { return m_pSpeedupForceLayer; }
	CMapItemLayerTilemap *SpeedupAngleLayer() const { return m_pSpeedupAngleLayer; }
	CMapItemGroup *GetGroup(int Index) const;
	CMapItemLayer *GetLayer(int Index) const;
};

#endif
