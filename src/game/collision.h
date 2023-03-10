/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include "game/mapitems.h"
#include <base/vmath.h>
#include <engine/shared/protocol.h>

#include <list>
#include <vector>
#include <array>

enum
{
	CANTMOVE_LEFT = 1 << 0,
	CANTMOVE_RIGHT = 1 << 1,
	CANTMOVE_UP = 1 << 2,
	CANTMOVE_DOWN = 1 << 3,
};

vec2 ClampVel(int MoveRestriction, vec2 Vel);

typedef bool (*CALLBACK_SWITCHACTIVE)(int Number, void *pUser);
struct CAntibotMapData;

struct CMovingTileData {
	std::array<ivec2, 4> m_Pos;
	ivec2 m_Center;
	int m_PosEnvOffset;
	int m_StartEnvPoint;
	int m_NumEnvPoints;
	int m_ParallaxX;
	int m_ParallaxY;
	int m_OffsetX;
	int m_OffsetY;
	CMovingTile m_MovingTile;
	std::array<vec2, 4> m_CurrentPos;
	bool m_TriangulationPattern;

	std::tuple<std::array<vec2, 3>, std::array<vec2, 3>> Triangulate() const;
	bool IsSolid() const;
	bool IsThrough(vec2 Pos0, vec2 Pos1) const;
	int GetTileIndex() const;
	bool IsHookBlocker(vec2 Pos0, vec2 Pos1) const;
	int GetTeleportNumber(int type) const;
	bool IsTimeCheckpoint() const;
};

class CCollision
{
	class CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	std::vector<CEnvPoint> m_pEnvPoints;
	std::vector<CMovingTileData> m_pMovingTiles;

public:
	CCollision();
	~CCollision();
	void Init(class IMap* pMap, class CLayers *pLayers);
	void FillAntibot(CAntibotMapData *pMapData);
	bool CheckPoint(float x, float y) const { return IsSolid(round_to_int(x), round_to_int(y)); }
	bool CheckPoint(vec2 Pos) const { return CheckPoint(Pos.x, Pos.y); }
	int GetCollisionAt(float x, float y) const { return GetTile(round_to_int(x), round_to_int(y)); }
	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	int IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const;
	std::tuple<int, const CMovingTileData*> IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr, vec2 player_pos) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity) const;
	bool TestBox(vec2 Pos, vec2 Size) const;

	// DDRace

	void Dest();
	void SetCollisionAt(float x, float y, int id);
	void SetDTile(float x, float y, bool State);
	void SetDCollisionAt(float x, float y, int Type, int Flags, int Number);
	int GetDTileIndex(int Index) const;
	int GetDTileFlags(int Index) const;
	int GetDTileNumber(int Index) const;
	int GetFCollisionAt(float x, float y) const { return GetFTile(round_to_int(x), round_to_int(y)); }
	int IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	int IntersectNoLaserNW(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	int IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const;
	int GetIndex(int x, int y) const;
	int GetIndex(vec2 PrevPos, vec2 Pos) const;
	int GetFIndex(int x, int y) const;

	int GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 Pos, float Distance = 18.0f, int OverrideCenterTileIndex = -1);
	int GetMoveRestrictions(vec2 Pos, float Distance = 18.0f)
	{
		return GetMoveRestrictions(nullptr, nullptr, Pos, Distance);
	}

	int GetTile(int x, int y) const;
	int GetFTile(int x, int y) const;
	int Entity(int x, int y, int Layer) const;
	int GetPureMapIndex(float x, float y) const;
	int GetPureMapIndex(vec2 Pos) const { return GetPureMapIndex(Pos.x, Pos.y); }
	std::list<int> GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices = 0) const;
	int GetMapIndex(vec2 Pos) const;
	bool TileExists(int Index) const;
	bool TileExistsNext(int Index) const;
	vec2 GetPos(int Index) const;
	int GetTileIndex(int Index) const;
	int GetFTileIndex(int Index) const;
	int GetTileFlags(int Index) const;
	int GetFTileFlags(int Index) const;
	int IsTeleport(int Index, const CMovingTileData* pMovingTile) const;
	int IsEvilTeleport(int Index, const CMovingTileData* pMovingTile) const;
	int IsCheckTeleport(int Index, const CMovingTileData* pMovingTile) const;
	int IsCheckEvilTeleport(int Index, const CMovingTileData* pMovingTile) const;
	int IsTeleportWeapon(int Index) const;
	int IsTeleportHook(int Index) const;
	int IsTeleCheckpoint(int Index, const CMovingTileData* pMovingTile) const;
	int IsSpeedup(int Index) const;
	int IsTune(int Index) const;
	void GetSpeedup(int Index, vec2 *pDir, int *pForce, int *pMaxSpeed) const;
	int GetSwitchType(int Index) const;
	int GetSwitchNumber(int Index) const;
	int GetSwitchDelay(int Index) const;

	int IsSolid(int x, int y) const;
	bool IsThrough(int x, int y, int xoff, int yoff, vec2 pos0, vec2 pos1) const;
	bool IsHookBlocker(int x, int y, vec2 pos0, vec2 pos1) const;
	int IsWallJump(int Index) const;
	int IsNoLaser(int x, int y) const;
	int IsFNoLaser(int x, int y) const;

	int IsTimeCheckpoint(int Index, const CMovingTileData* pMovingTile) const;
	int IsFTimeCheckpoint(int Index) const;

	int IsMover(int x, int y, int *pFlags) const;

	vec2 CpSpeed(int index, int Flags = 0) const;

	class CTeleTile *TeleLayer() { return m_pTele; }
	class CSwitchTile *SwitchLayer() { return m_pSwitch; }
	class CTuneTile *TuneLayer() { return m_pTune; }
	class CLayers *Layers() { return m_pLayers; }
	int m_HighestSwitchNumber;

	void Tick(int pTick, float intraTick = 0);
	const CMovingTileData* GetQuadCollisionAt(vec2 hook_pos, vec2 player_pos) const;
	vec2 UpdateHookPos(vec2 initial_hook_pos, int hook_tick, int pTick, const CMovingTileData* moving_tile, vec2 player_pos) const;
	vec2 ApplyParaToHook(vec2 initial_hook_pos, int tick, const CMovingTileData* moving_tile, vec2 player_pos) const;
	bool TestBoxQuad(vec2 Pos, vec2 Size, bool border = false, const CMovingTileData* movingTile = nullptr) const;
	void MoveBoxOutQuad(vec2 *pInoutPos, vec2 Size) const;
	vec2 MoveGroundedQuad(vec2 player_pos, int initial_tick, int tick, const CMovingTileData* moving_tile, vec2 Size) const;
	std::vector<const CMovingTileData*> GetQuadCollisionsBetween(vec2 initial_pos, vec2 final_pos) const;
	const CMovingTileData* CheckPointQuadRectangular(vec2 Pos, vec2 player_pos, bool border, const CMovingTileData* movingTile) const;
	void MoveBoxQuad(vec2 old_pos, vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size) const;

private:
	class CTeleTile *m_pTele;
	class CSpeedupTile *m_pSpeedup;
	class CTile *m_pFront;
	class CSwitchTile *m_pSwitch;
	class CTuneTile *m_pTune;
	class CDoorTile *m_pDoor;
};

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY);
#endif
