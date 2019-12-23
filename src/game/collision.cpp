/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;

	m_pTilesEx = 0;
	m_pTele = 0;
	m_pSpeedupForce = 0;
	m_pSpeedupAngle = 0;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pTilesEx = 0;
	m_pTele = 0;
	m_pSpeedupForce = 0;
	m_pSpeedupAngle = 0;

	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if(m_pLayers->GameExLayer())
		m_pTilesEx = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameExLayer()->m_Data));

	if(m_pLayers->TeleLayer())
	{
		m_pTele = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Data));
		InitTeleporter();
	}

	if(m_pLayers->SpeedupForceLayer() && m_pLayers->SpeedupAngleLayer() &&
		m_pLayers->SpeedupForceLayer()->m_Width == m_pLayers->SpeedupAngleLayer()->m_Width &&
		m_pLayers->SpeedupForceLayer()->m_Height == m_pLayers->SpeedupAngleLayer()->m_Height)
	{
		m_pSpeedupForce = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupForceLayer()->m_Data));
		m_pSpeedupAngle = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupAngleLayer()->m_Data));
	}

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;

		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK;
			break;
		default:
			m_pTiles[i].m_Index = 0;
		}

		// race tiles
		if(Index >= TILE_TELEIN_STOP && Index <= 59)
			m_pTiles[i].m_Index = Index;
	}
}

void CCollision::InitTeleporter()
{
	mem_zero(m_aTeleporter, sizeof(m_aTeleporter));

	for(int Ny = 0; Ny < m_pLayers->TeleLayer()->m_Height; Ny++)
		for(int Nx = 0; Nx < m_pLayers->TeleLayer()->m_Width; Nx++)
		{
			vec2 Pos = vec2(Nx*32+16, Ny*32+16);
			int TilePosTele = Ny*m_pLayers->TeleLayer()->m_Width+Nx;
			if(m_pTele[TilePosTele].m_Index > 0 && CheckIndexEx(Pos, TILE_TELEOUT))
				m_aTeleporter[m_pTele[TilePosTele].m_Index - 1] = Pos;
		}
}

int CCollision::GetTile(int x, int y) const
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	int Index = m_pTiles[Ny*m_Width+Nx].m_Index;
	if(Index == COLFLAG_SOLID || Index == (COLFLAG_SOLID|COLFLAG_NOHOOK) || Index == COLFLAG_DEATH)
		return Index;
	else
		return 0;
}

bool CCollision::IsTile(int x, int y, int Flag) const
{
	return GetTile(x, y)&Flag;
}

// race
vec2 CCollision::GetPos(int TilePos) const
{
	int x = TilePos%m_Width;
	int y = TilePos/m_Width;
	
	return vec2(x*32+16, y*32+16);
}

int CCollision::GetTilePosLayer(const CMapItemLayerTilemap *pLayer, vec2 Pos) const
{
	int Nx = round_to_int(Pos.x)/32;
	int Ny = round_to_int(Pos.y)/32;

	if(!pLayer)
		return -1;

	if(/*!Ex && */(Nx < 0 || Ny < 0 || Nx >= pLayer->m_Width || Ny >= pLayer->m_Height))
		return -1;

	//Nx = clamp(Nx, 0, pLayer->m_Width-1);
	//Ny = clamp(Ny, 0, pLayer->m_Height-1);
	
	return Ny*pLayer->m_Width+Nx;
}

bool CCollision::CheckIndexEx(vec2 Pos, int Index) const
{
	int TilePos = GetTilePosLayer(m_pLayers->GameLayer(), Pos);
	if(TilePos >= 0 && m_pTiles[TilePos].m_Index == Index)
		return true;
	TilePos = GetTilePosLayer(m_pLayers->GameExLayer(), Pos);
	if(TilePos >= 0 && m_pTilesEx[TilePos].m_Index == Index)
		return true;
	return false;
}

int CCollision::CheckIndexExRange(vec2 Pos, int MinIndex, int MaxIndex) const
{
	int TilePos = GetTilePosLayer(m_pLayers->GameLayer(), Pos);
	if(TilePos >= 0 && m_pTiles[TilePos].m_Index >= MinIndex && m_pTiles[TilePos].m_Index <= MaxIndex)
		return m_pTiles[TilePos].m_Index;
	TilePos = GetTilePosLayer(m_pLayers->GameExLayer(), Pos);
	if(TilePos >= 0 && m_pTilesEx[TilePos].m_Index >= MinIndex && m_pTilesEx[TilePos].m_Index <= MaxIndex)
		return m_pTilesEx[TilePos].m_Index;
	return -1;
}

int CCollision::CheckCheckpoint(vec2 Pos) const
{
	int Cp = CheckIndexExRange(Pos, 35, 59);
	if(Cp >= 0)
		return Cp-35;
	return -1;
}

int CCollision::CheckSpeedup(vec2 Pos) const
{
	int TilePosSpeedup = GetTilePosLayer(m_pLayers->SpeedupForceLayer(), Pos);
	if(TilePosSpeedup < 0 || !m_pSpeedupAngle || !CheckIndexEx(Pos, TILE_BOOST) || m_pSpeedupForce[TilePosSpeedup].m_Index == 0)
		return -1;
	return TilePosSpeedup;
}

vec2 CCollision::GetSpeedupForce(int Speedup) const
{
	int SpeedupAngle = m_pSpeedupAngle[Speedup].m_Index % 90;
	unsigned char Flags = m_pSpeedupAngle[Speedup].m_Flags;
	// TODO: handle all cases
	if(Flags == ROTATION_90)
		SpeedupAngle += 90;
	else if(Flags == ROTATION_180)
		SpeedupAngle += 180;
	else if(Flags == ROTATION_270)
		SpeedupAngle += 270;
	float Angle = SpeedupAngle * pi / 180.0f;
	float Force = m_pSpeedupForce[Speedup].m_Index;
	return vec2(cos(Angle), sin(Angle)) * Force;
}

int CCollision::CheckTeleport(vec2 Pos, bool *pStop) const
{
	int TilePosTele = GetTilePosLayer(m_pLayers->TeleLayer(), Pos);
	if(TilePosTele >= 0)
	{
		int Index = CheckIndexExRange(Pos, TILE_TELEIN_STOP, TILE_TELEIN);
		*pStop = Index == TILE_TELEIN_STOP;
		if(Index != -1)
			return m_pTele[TilePosTele].m_Index;
	}
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i <= End; i++)
	{
		float a = i/float(End);
		vec2 Pos = mix(Pos0, Pos1, a);
		if(CheckPoint(Pos.x, Pos.y))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size, int Flag) const
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y, Flag))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, CCollisionData *pCollisionData) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(pDeath)
		*pDeath = false;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			//You hit a deathtile, congrats to that :)
			//Deathtiles are a bit smaller
			if(pDeath && TestBox(vec2(NewPos.x, NewPos.y), Size*(2.0f/3.0f), COLFLAG_DEATH))
			{
				*pDeath = true;
			}

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			if(pCollisionData)
			{
				if(pCollisionData->m_PhysicsFlags&PHYSICSFLAG_STOPPER)
				{
					int Index =  CheckIndexExRange(NewPos, TILE_STOPL, TILE_STOPT);
					if((Index == TILE_STOPL && Vel.x > 0) || (Index == TILE_STOPR && Vel.x < 0))
					{
						NewPos.x = Pos.x;
						Vel.x = 0;
					}
					if((Index == TILE_STOPB && Vel.y < 0) || (Index == TILE_STOPT && Vel.y > 0))
					{
						NewPos.y = Pos.y;
						Vel.y = 0;
					}
				}

				if(pCollisionData->m_PhysicsFlags&PHYSICSFLAG_TELEPORT)
				{
					bool Stop;
					int Tele = CheckTeleport(NewPos, &Stop);
					if(Tele)
					{
						pCollisionData->m_Teleported = true;
						NewPos = GetTeleportDestination(Tele);
						if(Stop)
							Vel = vec2(0,0);
					}
				}

				if(pCollisionData->m_PhysicsFlags&PHYSICSFLAG_SPEEDUP)
				{
					ivec2 SpeedupTilePos = ivec2(round_to_int(Pos.x)/32, round_to_int(Pos.y)/32);
					int Speedup = CheckSpeedup(NewPos);
					if(pCollisionData->m_LastSpeedupTilePos != SpeedupTilePos && Speedup > -1)
					{
						Vel += GetSpeedupForce(Speedup);
						pCollisionData->m_LastSpeedupTilePos = SpeedupTilePos;
					}
				}

				if(pCollisionData->m_pfnPhysicsStepCallback)
					pCollisionData->m_pfnPhysicsStepCallback(NewPos, (i+1)*Fraction, pCollisionData->m_pPhysicsStepUserData);
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}
