/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

enum
{
	PHYSICSFLAG_STOPPER=1,
	PHYSICSFLAG_TELEPORT=2,
	PHYSICSFLAG_SPEEDUP=4,

	PHYSICSFLAG_RACE_ALL=PHYSICSFLAG_STOPPER|PHYSICSFLAG_TELEPORT|PHYSICSFLAG_SPEEDUP,
};

typedef void (*FPhysicsStepCallback)(vec2 Pos, float IntraTick, void *pUserData);

struct CCollisionData
{
	FPhysicsStepCallback m_pfnPhysicsStepCallback;
	void *m_pPhysicsStepUserData;
	int m_PhysicsFlags;
	ivec2 m_LastSpeedupTilePos;
	bool m_Teleported;
};

class CCollision
{
	class CTile *m_pTiles;
	class CTile *m_pTilesEx;
	class CTile *m_pTele;
	class CTile *m_pSpeedupForce;
	class CTile *m_pSpeedupAngle;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;

	vec2 m_aTeleporter[1<<8];

	void InitTeleporter();

	bool IsTile(int x, int y, int Flag=COLFLAG_SOLID) const;
	int GetTile(int x, int y) const;

	bool IsRaceTile(int TilePos, int Mask);

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,
	};

	CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y, int Flag=COLFLAG_SOLID) const { return IsTile(round_to_int(x), round_to_int(y), Flag); }
	bool CheckPoint(vec2 Pos, int Flag=COLFLAG_SOLID) const { return CheckPoint(Pos.x, Pos.y, Flag); }
	int GetCollisionAt(float x, float y) const { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWidth() const { return m_Width; };
	int GetHeight() const { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, CCollisionData *pCollisionData = 0) const;
	bool TestBox(vec2 Pos, vec2 Size, int Flag=COLFLAG_SOLID) const;

	// race
	vec2 GetPos(int TilePos) const;
	int GetTilePosLayer(const struct CMapItemLayerTilemap *pLayer, vec2 Pos) const;

	bool CheckIndexEx(vec2 Pos, int Index) const;
	int CheckIndexExRange(vec2 Pos, int MinIndex, int MaxIndex) const;

	int CheckCheckpoint(vec2 Pos) const;
	int CheckSpeedup(vec2 Pos) const;
	vec2 GetSpeedupForce(int Speedup) const;
	int CheckTeleport(vec2 Pos, bool *pStop) const;
	vec2 GetTeleportDestination(int Tele) const { return m_aTeleporter[Tele - 1]; };
};

#endif
