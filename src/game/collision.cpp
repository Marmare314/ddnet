/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "engine/shared/protocol.h"
#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <antibot/antibot_data.h>

#include <cmath>
#include <engine/map.h>

#include <game/collision.h>
#include <game/layers.h>
#include <game/mapitems.h>
#include <game/gamecore.h>

#include <engine/shared/config.h>

#include <chrono>
#include <iterator>

using namespace std::chrono_literals;

std::tuple<std::array<vec2, 3>, std::array<vec2, 3>> CMovingTileData::Triangulate() const {
	if (m_TriangulationPattern) {
		return {{m_CurrentPos[3], m_CurrentPos[2], m_CurrentPos[1]},
				{m_CurrentPos[1], m_CurrentPos[2], m_CurrentPos[0]}};
	} else {
		return {{m_CurrentPos[1], m_CurrentPos[3], m_CurrentPos[0]},
				{m_CurrentPos[0], m_CurrentPos[3], m_CurrentPos[2]}};
	}
}

bool CMovingTileData::IsSolid() const {
	if (m_MovingTile.m_Skip == 0)
	{
		return m_MovingTile.m_Index == TILE_SOLID 
			|| m_MovingTile.m_Index == TILE_NOHOOK 
			|| m_MovingTile.m_Index == TILE_THROUGH_CUT 
			|| m_MovingTile.m_Index == TILE_THROUGH_ALL 
			|| m_MovingTile.m_Index == TILE_THROUGH_DIR 
			|| m_MovingTile.m_Index == TILE_THROUGH;
	}
	return false;
}

bool CMovingTileData::IsThrough(vec2 Pos0, vec2 Pos1) const {
	if (m_MovingTile.m_Skip == 0) {
		if (m_MovingTile.m_Index == TILE_THROUGH_ALL) {
			return true;
		}
		if (m_MovingTile.m_Index == TILE_THROUGH_DIR) {
			if (m_MovingTile.m_Reserved == 0 && Pos0.y > Pos1.y) {
				return true;
			}
			if (m_MovingTile.m_Reserved == 1 && Pos0.x < Pos1.x) {
				return true;
			}
			if (m_MovingTile.m_Reserved == 2 && Pos0.y < Pos1.y) {
				return true;
			}
			if (m_MovingTile.m_Reserved == 3 && Pos0.x > Pos1.x) {
				return true;
			}
		}
	}
	return false;
}

int CMovingTileData::GetTileIndex() const {
	if (m_MovingTile.m_Skip == 0) {
		return m_MovingTile.m_Index;
	} else if (m_MovingTile.m_Skip == 1) {
		return m_MovingTile.m_Flags;
	} else {
		dbg_assert(false, "failed to get tile index");
		return -1;
	}
}

bool CMovingTileData::IsHookBlocker(vec2 Pos0, vec2 Pos1) const {
	return false; // TODO Marmare: implement
}

int CMovingTileData::GetTeleportNumber(int type) const {
	if (m_MovingTile.m_Skip == 1) {
		if (m_MovingTile.m_Flags == type) {
			return m_MovingTile.m_Index;
		}
	}
	return 0;
}

bool CMovingTileData::IsTimeCheckpoint() const {
	return false; // TODO Marmare: implement
}

vec2 ClampVel(int MoveRestriction, vec2 Vel)
{
	if(Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT))
	{
		Vel.x = 0;
	}
	if(Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT))
	{
		Vel.x = 0;
	}
	if(Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN))
	{
		Vel.y = 0;
	}
	if(Vel.y < 0 && (MoveRestriction & CANTMOVE_UP))
	{
		Vel.y = 0;
	}
	return Vel;
}

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;

	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pDoor = 0;
	m_pTune = 0;
}

CCollision::~CCollision()
{
	Dest();
}

// TODO: this is not nice
vec2 normalize_ivec2(ivec2 v)
{
	return vec2(v.x, v.y) / length(vec2(v.x, v.y));
}

float dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

bool calculate_triangulation(std::array<ivec2, 4> pPos)
{
	// calculate delaunay triangulation of pPos
	float dot0 = dot(normalize_ivec2(pPos[2] - pPos[3]), normalize_ivec2(pPos[1] - pPos[3]));
	float dot1 = dot(normalize_ivec2(pPos[2] - pPos[0]), normalize_ivec2(pPos[1] - pPos[0]));
	float angle0 = acos(fmax(-1, fmin(1, dot0))) + acos(fmax(-1, fmin(1, dot1)));

	dot0 = dot(normalize_ivec2(pPos[0] - pPos[2]), normalize_ivec2(pPos[3] - pPos[2]));
	dot1 = dot(normalize_ivec2(pPos[0] - pPos[1]), normalize_ivec2(pPos[3] - pPos[1]));
	float angle1 = acos(fmax(-1, fmin(1, dot0))) + acos(fmax(-1, fmin(1, dot1)));

	return angle0 < angle1;
}

void CCollision::Init(class IMap *pMap, class CLayers *pLayers)
{
	Dest();
	m_HighestSwitchNumber = 0;
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if(m_pLayers->TeleLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TeleLayer()->m_Tele);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTeleTile))
			m_pTele = static_cast<CTeleTile *>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Tele));
	}

	if(m_pLayers->SpeedupLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SpeedupLayer()->m_Speedup);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSpeedupTile))
			m_pSpeedup = static_cast<CSpeedupTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupLayer()->m_Speedup));
	}

	if(m_pLayers->SwitchLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->SwitchLayer()->m_Switch);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CSwitchTile))
			m_pSwitch = static_cast<CSwitchTile *>(m_pLayers->Map()->GetData(m_pLayers->SwitchLayer()->m_Switch));

		m_pDoor = new CDoorTile[m_Width * m_Height];
		mem_zero(m_pDoor, (size_t)m_Width * m_Height * sizeof(CDoorTile));
	}
	else
	{
		m_pDoor = 0;
	}

	if(m_pLayers->TuneLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->TuneLayer()->m_Tune);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTuneTile))
			m_pTune = static_cast<CTuneTile *>(m_pLayers->Map()->GetData(m_pLayers->TuneLayer()->m_Tune));
	}

	if(m_pLayers->FrontLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetDataSize(m_pLayers->FrontLayer()->m_Front);
		if(Size >= (size_t)m_Width * m_Height * sizeof(CTile))
			m_pFront = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->FrontLayer()->m_Front));
	}

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		int Index;
		if(m_pSwitch)
		{
			if(m_pSwitch[i].m_Number > m_HighestSwitchNumber)
				m_HighestSwitchNumber = m_pSwitch[i].m_Number;

			if(m_pSwitch[i].m_Number)
				m_pDoor[i].m_Number = m_pSwitch[i].m_Number;
			else
				m_pDoor[i].m_Number = 0;

			Index = m_pSwitch[i].m_Type;

			if(Index <= TILE_NPH_ENABLE)
			{
				if((Index >= TILE_JUMP && Index <= TILE_SUBTRACT_TIME) || Index == TILE_ALLOW_TELE_GUN || Index == TILE_ALLOW_BLUE_TELE_GUN)
					m_pSwitch[i].m_Type = Index;
				else
					m_pSwitch[i].m_Type = 0;
			}
		}
	}

	// Moving tiles
	m_pMovingTiles.clear();

	int StartGroups, NumGroups;
	pMap->GetType(MAPITEMTYPE_GROUP, &StartGroups, &NumGroups);

	int StartEnvelopes, NumEnvelopes;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &StartEnvelopes, &NumEnvelopes);

	// TODO: check for kog_qquads 1

	int last_envpoint_used = 0;
	for (int index_group = 0; index_group < NumGroups; index_group++) {
		CMapItemGroup* group = static_cast<CMapItemGroup*>(pMap->GetItem(StartGroups + index_group, 0, 0));
		if (group->m_Version < 2) {
			continue;
		}

		for (int index_layer = 0; index_layer < group->m_NumLayers; index_layer++) {
			CMapItemLayer* layer = pLayers->GetLayer(group->m_StartLayer + index_layer);
			if (layer->m_Type == LAYERTYPE_QUADS) {
				
				// TODO: assert that parazoom is off

				auto* layer_quads = reinterpret_cast<CMapItemLayerQuads*>(layer);
				CQuad* quad_array = nullptr;
				// CMovingTile* moving_tile_info = nullptr;
				if (layer_quads->m_Data >= 0) {
					quad_array = static_cast<CQuad*>(pMap->GetDataSwapped(layer_quads->m_Data));
					// moving_tile_info = static_cast<CMovingTile*>(pMap->GetDataSwapped(layer_quads->m_Data + 1));
				} else {
					continue;
				}

				CMovingTile info;
				char Name[128];
				IntsToStr(layer_quads->m_aName, 3, Name);
				if(str_comp(Name, "QFr") == 0) {
					info = {
						TILE_FREEZE,
						0,
						0,
						0
					};
				}
				else if(str_comp(Name, "QUnFr") == 0) {
					info = {
						TILE_UNFREEZE,
						0,
						0,
						0
					};
				}
				else if(str_comp(Name, "QHook") == 0) {
					info = {
						TILE_SOLID,
						0,
						0,
						0
					};
				}
				else if(str_comp(Name, "QUnHook") == 0) {
					info = {
						TILE_NOHOOK,
						0,
						0,
						0
					};
				}
				else if(str_comp(Name, "QDeath") == 0) {
					info = {
						TILE_DEATH,
						0,
						0,
						0
					};
				}
				// else if(str_comp(Name, "QCfrm") == 0) {
				// 	info = {
				// 		0,
				// 		TILE_TELECHECKIN,
				// 		1,
				// 		0
				// 	};
				// }

				for (int index_quad = 0; index_quad < layer_quads->m_NumQuads; index_quad++) {
					// TODO: assert that envelope is synchronized
					// TODO: assert solid tiles are rectangular and have no rotation

					auto* env = static_cast<CMapItemEnvelope*>(pMap->GetItem(StartEnvelopes + quad_array[index_quad].m_PosEnv, 0, 0));

					int num_env_points = env->m_NumPoints < 0 ? 0 : env->m_NumPoints;
					if (num_env_points > 0 && env->m_StartPoint + num_env_points > last_envpoint_used) {
						last_envpoint_used = env->m_StartPoint + num_env_points;
					}

					m_pMovingTiles.push_back(CMovingTileData{
						{quad_array[index_quad].m_aPoints[0], quad_array[index_quad].m_aPoints[1], quad_array[index_quad].m_aPoints[2], quad_array[index_quad].m_aPoints[3]},
						quad_array[index_quad].m_aPoints[4],
						quad_array[index_quad].m_PosEnvOffset,
						env->m_StartPoint,
						num_env_points,
						group->m_ParallaxX,
						group->m_ParallaxY,
						group->m_OffsetX,
						group->m_OffsetY,
						info
					});

					m_pMovingTiles.back().m_TriangulationPattern = calculate_triangulation(m_pMovingTiles.back().m_Pos);
				}
			}
		}
	}

	int Start, Num;
	pMap->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);

	m_pEnvPoints.resize(last_envpoint_used);
	if(Num > 0 && last_envpoint_used > 0) {
		auto* pPoints = static_cast<CEnvPoint *>(pMap->GetItem(Start, 0, 0));
		m_pEnvPoints.assign(pPoints, pPoints + last_envpoint_used);
	}
}

// copied from render_map.cpp
ivec2 Rotate(ivec2 pCenter, ivec2 pPoint, float Rotation)
{
	int x = pPoint.x - pCenter.x;
	int y = pPoint.y - pCenter.y;

	ivec2 res;
	res.x = (int)(x * cosf(Rotation) - y * sinf(Rotation) + pCenter.x);
	res.y = (int)(x * sinf(Rotation) + y * cosf(Rotation) + pCenter.y);

	return res;
}

vec3 evaluate_envelope(int pTick, float intraTick, const CMovingTileData& tile, const std::vector<CEnvPoint>& pEnvPoints) {
	const auto TickToNanoSeconds = std::chrono::nanoseconds(1s) / (int64_t)SERVER_TICK_SPEED;
	auto TimeNanos = std::chrono::nanoseconds(std::chrono::milliseconds(tile.m_PosEnvOffset)) + TickToNanoSeconds * (int64_t)pTick + std::chrono::nanoseconds(static_cast<int64_t>(TickToNanoSeconds.count() * intraTick));

	vec3 pos_offset;
	if(tile.m_NumEnvPoints == 0)
	{
		pos_offset.x = 0;
		pos_offset.y = 0;
		pos_offset.z = 0;
	}
	else if(tile.m_NumEnvPoints == 1)
	{
		pos_offset.x = fx2f(pEnvPoints.at(tile.m_StartEnvPoint).m_aValues[0]);
		pos_offset.y = fx2f(pEnvPoints.at(tile.m_StartEnvPoint).m_aValues[1]);
		pos_offset.z = fx2f(pEnvPoints.at(tile.m_StartEnvPoint).m_aValues[2]);
	}
	else
	{
		int64_t MaxPointTime = (int64_t)pEnvPoints.at(tile.m_StartEnvPoint + tile.m_NumEnvPoints - 1).m_Time * std::chrono::nanoseconds(1ms).count();
		if(MaxPointTime > 0)
		{
			TimeNanos = std::chrono::nanoseconds(TimeNanos.count() % MaxPointTime);
		}
		else
		{
			TimeNanos = decltype(TimeNanos)::zero();
		}

		int TimeMillis = (int)(TimeNanos / std::chrono::nanoseconds(1ms).count()).count();
		bool in_any_range = false;
		for(int i = 0; i < tile.m_NumEnvPoints - 1; i++)
		{
			if(TimeMillis >= pEnvPoints.at(tile.m_StartEnvPoint + i).m_Time && TimeMillis <= pEnvPoints.at(tile.m_StartEnvPoint + i + 1).m_Time)
			{
				in_any_range = true;

				float Delta = pEnvPoints.at(tile.m_StartEnvPoint + i + 1).m_Time - pEnvPoints.at(tile.m_StartEnvPoint + i).m_Time;
				float a = (float)(((double)TimeNanos.count() / (double)std::chrono::nanoseconds(1ms).count()) - pEnvPoints.at(tile.m_StartEnvPoint + i).m_Time) / Delta;

				if(pEnvPoints.at(tile.m_StartEnvPoint + i).m_Curvetype == CURVETYPE_SMOOTH)
					a = -2 * a * a * a + 3 * a * a; // second hermite basis
				else if(pEnvPoints.at(tile.m_StartEnvPoint + i).m_Curvetype == CURVETYPE_SLOW)
					a = a * a * a;
				else if(pEnvPoints.at(tile.m_StartEnvPoint + i).m_Curvetype == CURVETYPE_FAST)
				{
					a = 1 - a;
					a = 1 - a * a * a;
				}
				else if(pEnvPoints.at(tile.m_StartEnvPoint + i).m_Curvetype == CURVETYPE_STEP)
					a = 0;
				else
				{
					// linear
				}

				float v0 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i).m_aValues[0]);
				float v1 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i + 1).m_aValues[0]);
				pos_offset.x = v0 + (v1 - v0) * a;

				v0 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i).m_aValues[1]);
				v1 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i + 1).m_aValues[1]);
				pos_offset.y = v0 + (v1 - v0) * a;

				v0 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i).m_aValues[2]);
				v1 = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + i + 1).m_aValues[2]);
				pos_offset.z = v0 + (v1 - v0) * a;
			}
		}

		if (!in_any_range) {
			pos_offset.x = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + tile.m_NumEnvPoints - 1).m_aValues[0]);
			pos_offset.y = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + tile.m_NumEnvPoints - 1).m_aValues[1]);
			pos_offset.z = fx2f(pEnvPoints.at(tile.m_StartEnvPoint + tile.m_NumEnvPoints - 1).m_aValues[2]);
		}
	}

	pos_offset.z = pos_offset.z / 180.0f * pi;
	return pos_offset;
}

std::array<vec2, 4> update_envelope(int pTick, float intraTick, const CMovingTileData& tile, const std::vector<CEnvPoint>& pEnvPoints) {
	auto pos_offset = evaluate_envelope(pTick, intraTick, tile, pEnvPoints);

	std::array<vec2, 4> eval_pos;
	for(int i = 0; i < 4; i++)
	{
		auto rotated_point = Rotate(tile.m_Center, tile.m_Pos[i], pos_offset.z);
		eval_pos[i].x = fx2f(rotated_point.x) + pos_offset.x;
		eval_pos[i].y = fx2f(rotated_point.y) + pos_offset.y;
	}

	return eval_pos;
}

// TODO Marmare: use fixed point for as long as possible
void CCollision::Tick(int pTick, float intraTick) {
	for (auto& movingTile: m_pMovingTiles) {
		movingTile.m_CurrentPos = update_envelope(pTick, intraTick, movingTile, m_pEnvPoints);
	}
}

bool point_in_triangle(std::array<vec2, 3> triangle, vec2 point) {
	vec2 v0 = triangle[2] - triangle[0];
	vec2 v1 = triangle[1] - triangle[0];
	vec2 v2 = point - triangle[0];

	float dot00 = v0[0] * v0[0] + v0[1] * v0[1];
	float dot01 = v0[0] * v1[0] + v0[1] * v1[1];
	float dot02 = v0[0] * v2[0] + v0[1] * v2[1];
	float dot11 = v1[0] * v1[0] + v1[1] * v1[1];
	float dot12 = v1[0] * v2[0] + v1[1] * v2[1];

	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	return (u >= 0) && (v >= 0) && (u + v < 1);
}

vec2 apply_para_to_point(vec2 player_pos, vec2 pos, int parax, int paray, int offsetx, int offsety) {
	return vec2(pos.x + player_pos.x - player_pos.x * parax / 100.0f - offsetx, pos.y + player_pos.y - player_pos.y * paray / 100.0f - offsety);
}

vec2 remove_para_from_point(vec2 player_pos, vec2 pos, int parax, int paray, int offsetx, int offsety) {
	return vec2(pos.x - player_pos.x + player_pos.x * parax / 100.0f + offsetx, pos.y - player_pos.y + player_pos.y * paray / 100.0f + offsety);
}

std::array<vec2, 3> apply_para_to_triangle(std::array<vec2, 3> triangle, vec2 player_pos, int parax, int paray, int offsetx, int offsety) {
	std::array<vec2, 3> new_triangle;
	for (int i = 0; i < 3; i++) {
		new_triangle[i] = apply_para_to_point(player_pos, triangle[i], parax, paray, offsetx, offsety);
	}
	return new_triangle;
}

const CMovingTileData* CCollision::CheckPointQuadRectangular(vec2 Pos, vec2 player_pos, bool border, const CMovingTileData* movingTile) const {
	if (movingTile != nullptr) {
		if (movingTile->IsSolid()) {
			// TODO Marmare: apply parallax
			if (round_to_int(movingTile->m_CurrentPos[0].x) <= round_to_int(Pos.x) && round_to_int(Pos.x) <= round_to_int(movingTile->m_CurrentPos[1].x) && round_to_int(movingTile->m_CurrentPos[0].y) <= round_to_int(Pos.y) && round_to_int(Pos.y) <= round_to_int(movingTile->m_CurrentPos[2].y)) {
				return movingTile;
			}
		}
	} else {
		for (const auto& tile: m_pMovingTiles) {
			if (const auto* ptr = CheckPointQuadRectangular(Pos, player_pos, border, &tile); ptr != nullptr) {
				return ptr;
			}
		}
	}
	return nullptr;
}

const CMovingTileData* CCollision::GetQuadCollisionAt(vec2 Pos, vec2 player_pos) const {
	for (const auto& movingTile : m_pMovingTiles) {
		// TODO Marmare: could be made more efficient for solid tiles
		auto [triangle_1, triangle_2] = movingTile.Triangulate();
		triangle_1 = apply_para_to_triangle(triangle_1, player_pos, movingTile.m_ParallaxX, movingTile.m_ParallaxY, movingTile.m_OffsetX, movingTile.m_OffsetY);
		triangle_2 = apply_para_to_triangle(triangle_2, player_pos, movingTile.m_ParallaxX, movingTile.m_ParallaxY, movingTile.m_OffsetX, movingTile.m_OffsetY);
		if (point_in_triangle(triangle_1, Pos) || point_in_triangle(triangle_2, Pos)) {
			Pos = remove_para_from_point(player_pos, Pos, movingTile.m_ParallaxX, movingTile.m_ParallaxY, movingTile.m_OffsetX, movingTile.m_OffsetY);
			return &movingTile;
		}
	}
	return nullptr;
}

vec2 CCollision::UpdateHookPos(vec2 initial_hook_pos, int hook_tick, int pTick, const CMovingTileData* moving_tile, vec2 player_pos) const {
	if (moving_tile->m_NumEnvPoints == 0) {
		apply_para_to_point(player_pos, initial_hook_pos, moving_tile->m_ParallaxX, moving_tile->m_ParallaxY, moving_tile->m_OffsetX, moving_tile->m_OffsetY);
	}

	auto pos_offset0 = evaluate_envelope(hook_tick, 0, *moving_tile, m_pEnvPoints);
	auto pos_offset1 = evaluate_envelope(pTick, 0, *moving_tile, m_pEnvPoints);

	vec3 pos_offset = pos_offset1 - pos_offset0;

	auto rotated_point = Rotate(moving_tile->m_Center, {f2fx(initial_hook_pos.x), f2fx(initial_hook_pos.y)}, pos_offset.z);
	
	vec2 new_hook_pos;
	new_hook_pos.x = fx2f(rotated_point.x) + pos_offset.x;
	new_hook_pos.y = fx2f(rotated_point.y) + pos_offset.y;

	return apply_para_to_point(player_pos, new_hook_pos, moving_tile->m_ParallaxX, moving_tile->m_ParallaxY, moving_tile->m_OffsetX, moving_tile->m_OffsetY);
}

vec2 CCollision::ApplyParaToHook(vec2 initial_hook_pos, int tick, const CMovingTileData* moving_tile, vec2 player_pos) const {
	return apply_para_to_point(player_pos, initial_hook_pos, moving_tile->m_ParallaxX, moving_tile->m_ParallaxY, moving_tile->m_OffsetX, moving_tile->m_OffsetY);
}

void CCollision::FillAntibot(CAntibotMapData *pMapData)
{
	pMapData->m_Width = m_Width;
	pMapData->m_Height = m_Height;
	pMapData->m_pTiles = (unsigned char *)malloc((size_t)m_Width * m_Height);
	for(int i = 0; i < m_Width * m_Height; i++)
	{
		pMapData->m_pTiles[i] = 0;
		if(m_pTiles[i].m_Index >= TILE_SOLID && m_pTiles[i].m_Index <= TILE_NOLASER)
		{
			pMapData->m_pTiles[i] = m_pTiles[i].m_Index;
		}
	}
}

enum
{
	MR_DIR_HERE = 0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
	switch(Tile)
	{
	case TILE_STOP:
		switch(Flags)
		{
		case ROTATION_0: return CANTMOVE_DOWN;
		case ROTATION_90: return CANTMOVE_LEFT;
		case ROTATION_180: return CANTMOVE_UP;
		case ROTATION_270: return CANTMOVE_RIGHT;

		case TILEFLAG_YFLIP ^ ROTATION_0: return CANTMOVE_UP;
		case TILEFLAG_YFLIP ^ ROTATION_90: return CANTMOVE_RIGHT;
		case TILEFLAG_YFLIP ^ ROTATION_180: return CANTMOVE_DOWN;
		case TILEFLAG_YFLIP ^ ROTATION_270: return CANTMOVE_LEFT;
		}
		break;
	case TILE_STOPS:
		switch(Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
		case TILEFLAG_YFLIP ^ ROTATION_0:
		case TILEFLAG_YFLIP ^ ROTATION_180:
			return CANTMOVE_DOWN | CANTMOVE_UP;
		case ROTATION_90:
		case ROTATION_270:
		case TILEFLAG_YFLIP ^ ROTATION_90:
		case TILEFLAG_YFLIP ^ ROTATION_270:
			return CANTMOVE_LEFT | CANTMOVE_RIGHT;
		}
		break;
	case TILE_STOPA:
		return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch(Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT;
	case MR_DIR_DOWN: return CANTMOVE_DOWN;
	case MR_DIR_LEFT: return CANTMOVE_LEFT;
	case MR_DIR_UP: return CANTMOVE_UP;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int GetMoveRestrictions(int Direction, int Tile, int Flags)
{
	int Result = GetMoveRestrictionsRaw(Direction, Tile, Flags);
	// Generally, stoppers only have an effect if they block us from moving
	// *onto* them. The one exception is one-way blockers, they can also
	// block us from moving if we're on top of them.
	if(Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}
	return Result & GetMoveRestrictionsMask(Direction);
}

int CCollision::GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex)
{
	static const vec2 DIRECTIONS[NUM_MR_DIRS] =
		{
			vec2(0, 0),
			vec2(1, 0),
			vec2(0, 1),
			vec2(-1, 0),
			vec2(0, -1)};
	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "invalid distance");
	int Restrictions = 0;
	for(int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DIRECTIONS[d] * Distance;
		int ModMapIndex = GetPureMapIndex(ModPos);
		if(d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
		{
			ModMapIndex = OverrideCenterTileIndex;
		}
		for(int Front = 0; Front < 2; Front++)
		{
			int Tile;
			int Flags;
			if(!Front)
			{
				Tile = GetTileIndex(ModMapIndex);
				Flags = GetTileFlags(ModMapIndex);
			}
			else
			{
				Tile = GetFTileIndex(ModMapIndex);
				Flags = GetFTileFlags(ModMapIndex);
			}
			Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);
		}
		if(pfnSwitchActive)
		{
			int TeleNumber = GetDTileNumber(ModMapIndex);
			if(pfnSwitchActive(TeleNumber, pUser))
			{
				int Tile = GetDTileIndex(ModMapIndex);
				int Flags = GetDTileFlags(ModMapIndex);
				Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);
			}
		}
	}
	return Restrictions;
}

int CCollision::GetTile(int x, int y) const
{
	if(!m_pTiles)
		return 0;

	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int pos = Ny * m_Width + Nx;

	if(m_pTiles[pos].m_Index >= TILE_SOLID && m_pTiles[pos].m_Index <= TILE_NOLASER)
		return m_pTiles[pos].m_Index;
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

std::tuple<int, const CMovingTileData*> CCollision::IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr, vec2 player_pos) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	int dx = 0, dy = 0; // Offset for checking the "through" tile
	ThroughOffset(Pos0, Pos1, &dx, &dy);
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		// TODO Marmare: implement moving tiles telehook
		int Index = GetPureMapIndex(Pos);
		if(g_Config.m_SvOldTeleportHook)
			*pTeleNr = IsTeleport(Index, nullptr);
		else
			*pTeleNr = IsTeleportHook(Index);
		if(*pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return {TILE_TELEINHOOK, nullptr};
		}

		int hit = 0;
		const CMovingTileData* moving_tile = nullptr;
		if(CheckPoint(ix, iy))
		{
			if(!IsThrough(ix, iy, dx, dy, Pos0, Pos1))
				hit = GetCollisionAt(ix, iy);
		}
		else if(IsHookBlocker(ix, iy, Pos0, Pos1))
		{
			hit = TILE_NOHOOK;
		}
		else if(moving_tile = GetQuadCollisionAt(Pos, player_pos); moving_tile != nullptr) {
			if (moving_tile->IsSolid()) {
				if (!moving_tile->IsThrough(Pos0, Pos1)) {
					hit = moving_tile->GetTileIndex();
				}
				else if (moving_tile->IsHookBlocker(Pos0, Pos1)) {
					hit = TILE_NOHOOK;
				}
			}
		}
		if(hit)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return {hit, moving_tile};
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return {0, nullptr};
}

int CCollision::IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr) const
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance + 1);
	vec2 Last = Pos0;
	for(int i = 0; i <= End; i++)
	{
		float a = i / (float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		// Temporary position for checking collision
		int ix = round_to_int(Pos.x);
		int iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if(g_Config.m_SvOldTeleportWeapons)
			*pTeleNr = IsTeleport(Index, nullptr);
		else
			*pTeleNr = IsTeleportWeapon(Index);
		if(*pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINWEAPON;
		}

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
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

// bool line_collide(vec2 p1, vec2 p2, vec2 center, float radius) {
// 	vec2 intersect_pos;
// 	if(closest_point_on_line(p1, p2, center, intersect_pos)) {
// 		float len = distance(center, intersect_pos);
// 		return len < radius;
// 	}
// 	return false;
// }

// bool circle_intersects_triangle(std::array<vec2, 3> corners, vec2 center, float radius) {
// 	return point_in_triangle(corners, center)
// 		|| line_collide(corners[0], corners[1], center, radius)
// 		|| line_collide(corners[1], corners[2], center, radius)
// 		|| line_collide(corners[2], corners[0], center, radius);
// }

bool CCollision::TestBoxQuad(vec2 Pos, vec2 Size, bool border, const CMovingTileData* movingTile) const {
	Size *= 0.5f;
	if(CheckPointQuadRectangular(vec2(Pos.x - Size.x, Pos.y - Size.y), Pos, border, movingTile))
		return true;
	if(CheckPointQuadRectangular(vec2(Pos.x + Size.x, Pos.y - Size.y), Pos, border, movingTile))
		return true;
	if(CheckPointQuadRectangular(vec2(Pos.x - Size.x, Pos.y + Size.y), Pos, border, movingTile))
		return true;
	if(CheckPointQuadRectangular(vec2(Pos.x + Size.x, Pos.y + Size.y), Pos, border, movingTile))
		return true;
	return false;
}

void CCollision::MoveBoxOutQuad(vec2* pInoutPos, vec2 Size) const {
	for (const auto& movingTile: m_pMovingTiles) {
		if (movingTile.IsSolid()) {
			// TODO Marmare: die if the player is inside a tile
			if (TestBoxQuad(*pInoutPos, Size, false, &movingTile)) {
				float dx0 = pInoutPos->x - movingTile.m_CurrentPos.at(0).x;
				float dx1 = movingTile.m_CurrentPos.at(1).x - pInoutPos->x;
				float dy0 = pInoutPos->y - movingTile.m_CurrentPos.at(0).y;
				float dy2 = movingTile.m_CurrentPos.at(2).y - pInoutPos->y;
				if (std::min(dx0, dx1) < std::min(dy0, dy2)) {
					if (dx0 < dx1) {
						pInoutPos->x = floor(movingTile.m_CurrentPos.at(0).x - Size.x / 2) - 1;
					} else {
						pInoutPos->x = ceil(movingTile.m_CurrentPos.at(1).x + Size.x / 2) + 1;
					}

					if (TestBoxQuad(*pInoutPos, Size, false, &movingTile)) {
						if (dy0 < dy2) {
							pInoutPos->y = floor(movingTile.m_CurrentPos.at(0).y - Size.y / 2) - 1;
						} else {
							pInoutPos->y = ceil(movingTile.m_CurrentPos.at(2).y + Size.y / 2) + 1;
						}
					}
				} else {
					if (dy0 < dy2) {
						pInoutPos->y = floor(movingTile.m_CurrentPos.at(0).y - Size.y / 2) - 1;
					} else {
						pInoutPos->y = ceil(movingTile.m_CurrentPos.at(2).y + Size.y / 2) + 1;
					}

					if (TestBoxQuad(*pInoutPos, Size, false, &movingTile)) {
						if (dx0 < dx1) {
							pInoutPos->x = floor(movingTile.m_CurrentPos.at(0).x - Size.x / 2) - 1;
						} else {
							pInoutPos->x = ceil(movingTile.m_CurrentPos.at(1).x + Size.x / 2) + 1;
						}
					}
				}
			}
		}
	}
}

vec2 CCollision::MoveGroundedQuad(vec2 player_pos, int initial_tick, int tick, const CMovingTileData* moving_tile, vec2 Size) const {
	vec2 offset(0, 0);
	if (moving_tile->m_NumEnvPoints == 0) {
		return offset;
	}

	auto pos_offset0 = evaluate_envelope(initial_tick, 0, *moving_tile, m_pEnvPoints);
	auto pos_offset1 = evaluate_envelope(tick, 0, *moving_tile, m_pEnvPoints);

	vec3 pos_offset = pos_offset1 - pos_offset0;

	return vec2(pos_offset.x, pos_offset.y);
}

float cross(vec2 a, vec2 b) {
	return a.x * b.y - a.y * b.x;
}

bool line_intersects_line(vec2 p0, vec2 p1, vec2 q0, vec2 q1) {
	vec2 r = p1 - p0;
	vec2 s = q1 - q0;
	float rxs = cross(r, s);
	if (rxs == 0) {
		return false;
	}
	float t = cross(q0 - p0, s) / rxs;
	float u = cross(q0 - p0, r) / rxs;
	return 0 <= t && t <= 1 && 0 <= u && u <= 1;
}

bool line_intersects_triangle(vec2 p0, vec2 p1, std::array<vec2, 3> corners) {
	if (point_in_triangle(corners, p0) || point_in_triangle(corners, p1)) {
		return true;
	}
	for (int i = 0; i < 3; i++) {
		if (line_intersects_line(p0, p1, corners[i], corners[(i + 1) % 3])) {
			return true;
		}
	}
	return false;
}

std::vector<const CMovingTileData*> CCollision::GetQuadCollisionsBetween(vec2 initial_pos, vec2 final_pos) const {
	std::vector<const CMovingTileData*> collisions;
	for (const auto& movingTile: m_pMovingTiles) {
		// TODO: consider parallax
		auto [triangle_1, triangle_2] = movingTile.Triangulate();
		if (line_intersects_triangle(initial_pos, final_pos, triangle_1) || line_intersects_triangle(initial_pos, final_pos, triangle_2)) {
			collisions.push_back(&movingTile);
		}
	}
	return collisions;
}

bool CCollision::TestBox(vec2 Pos, vec2 Size) const
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x - Size.x, Pos.y - Size.y))
		return true;
	if(CheckPoint(Pos.x + Size.x, Pos.y - Size.y))
		return true;
	if(CheckPoint(Pos.x - Size.x, Pos.y + Size.y))
		return true;
	if(CheckPoint(Pos.x + Size.x, Pos.y + Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance + 1; // TODO Marmare: is the * 32 needed?

	if(Distance > 0.00001f)
	{
		float Fraction = 1.0f / (float)(Max + 1);
		for(int i = 0; i <= Max; i++)
		{
			// Early break as optimization to stop checking for collisions for
			// large distances after the obstacles we have already hit reduced
			// our speed to exactly 0.
			if(Vel == vec2(0, 0))
			{
				break;
			}

			vec2 NewPos = Pos + Vel * Fraction; // TODO: this row is not nice

			// Fraction can be very small and thus the calculation has no effect, no
			// reason to continue calculating.
			if(NewPos == Pos)
			{
				break;
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

			if(TestBoxQuad(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBoxQuad(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBoxQuad(vec2(NewPos.x, Pos.y), Size))
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

			// MoveBoxQuad(Pos, &NewPos, &Vel, Size);

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

// DDRace

void CCollision::Dest()
{
	delete[] m_pDoor;
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pTune = 0;
	m_pDoor = 0;
}

int CCollision::IsSolid(int x, int y) const
{
	int index = GetTile(x, y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

bool CCollision::IsThrough(int x, int y, int xoff, int yoff, vec2 pos0, vec2 pos1) const
{
	int pos = GetPureMapIndex(x, y);
	if(m_pFront && (m_pFront[pos].m_Index == TILE_THROUGH_ALL || m_pFront[pos].m_Index == TILE_THROUGH_CUT))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && ((m_pFront[pos].m_Flags == ROTATION_0 && pos0.y > pos1.y) || (m_pFront[pos].m_Flags == ROTATION_90 && pos0.x < pos1.x) || (m_pFront[pos].m_Flags == ROTATION_180 && pos0.y < pos1.y) || (m_pFront[pos].m_Flags == ROTATION_270 && pos0.x > pos1.x)))
		return true;
	int offpos = GetPureMapIndex(x + xoff, y + yoff);
	return m_pTiles[offpos].m_Index == TILE_THROUGH || (m_pFront && m_pFront[offpos].m_Index == TILE_THROUGH);
}

bool CCollision::IsHookBlocker(int x, int y, vec2 pos0, vec2 pos1) const
{
	int pos = GetPureMapIndex(x, y);
	if(m_pTiles[pos].m_Index == TILE_THROUGH_ALL || (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_ALL))
		return true;
	if(m_pTiles[pos].m_Index == TILE_THROUGH_DIR && ((m_pTiles[pos].m_Flags == ROTATION_0 && pos0.y < pos1.y) ||
								(m_pTiles[pos].m_Flags == ROTATION_90 && pos0.x > pos1.x) ||
								(m_pTiles[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) ||
								(m_pTiles[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x)))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && ((m_pFront[pos].m_Flags == ROTATION_0 && pos0.y < pos1.y) || (m_pFront[pos].m_Flags == ROTATION_90 && pos0.x > pos1.x) || (m_pFront[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) || (m_pFront[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x)))
		return true;
	return false;
}

int CCollision::IsWallJump(int Index) const
{
	if(Index < 0)
		return 0;

	return m_pTiles[Index].m_Index == TILE_WALLJUMP;
}

int CCollision::IsNoLaser(int x, int y) const
{
	return (CCollision::GetTile(x, y) == TILE_NOLASER);
}

int CCollision::IsFNoLaser(int x, int y) const
{
	return (CCollision::GetFTile(x, y) == TILE_NOLASER);
}

int CCollision::IsTeleport(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return 0;
		if(!m_pTele)
			return 0;
		if(m_pTele[Index].m_Type == TILE_TELEIN)
			return m_pTele[Index].m_Number;
		return 0;
	} else {
		return pMovingTile->GetTeleportNumber(TILE_TELEIN);
	}
}

int CCollision::IsEvilTeleport(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return 0;
		if(!m_pTele)
			return 0;
		if(m_pTele[Index].m_Type == TILE_TELEINEVIL)
			return m_pTele[Index].m_Number;
		return 0;
	} else {
		return pMovingTile->GetTeleportNumber(TILE_TELEINEVIL);
	}
}

int CCollision::IsCheckTeleport(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return 0;
		if(!m_pTele)
			return 0;
		if(m_pTele[Index].m_Type == TILE_TELECHECKIN)
			return m_pTele[Index].m_Number;
		return 0;
	} else {
		return pMovingTile->GetTeleportNumber(TILE_TELECHECKIN);
	}
}

int CCollision::IsCheckEvilTeleport(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return 0;
		if(!m_pTele)
			return 0;
		if(m_pTele[Index].m_Type == TILE_TELECHECKINEVIL)
			return m_pTele[Index].m_Number;
		return 0;
	} else {
		return pMovingTile->GetTeleportNumber(TILE_TELECHECKINEVIL);
	}
}

int CCollision::IsTeleCheckpoint(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return 0;
		if(!m_pTele)
			return 0;
		if(m_pTele[Index].m_Type == TILE_TELECHECK)
			return m_pTele[Index].m_Number;
		return 0;
	} else {
		return pMovingTile->GetTeleportNumber(TILE_TELECHECK);
	}
}

int CCollision::IsTeleportWeapon(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINWEAPON)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportHook(int Index) const
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINHOOK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsSpeedup(int Index) const
{
	if(Index < 0 || !m_pSpeedup)
		return 0;

	if(m_pSpeedup[Index].m_Force > 0)
		return Index;

	return 0;
}

int CCollision::IsTune(int Index) const
{
	if(Index < 0 || !m_pTune)
		return 0;

	if(m_pTune[Index].m_Type)
		return m_pTune[Index].m_Number;

	return 0;
}

void CCollision::GetSpeedup(int Index, vec2 *pDir, int *pForce, int *pMaxSpeed) const
{
	if(Index < 0 || !m_pSpeedup)
		return;
	float Angle = m_pSpeedup[Index].m_Angle * (pi / 180.0f);
	*pForce = m_pSpeedup[Index].m_Force;
	*pDir = direction(Angle);
	if(pMaxSpeed)
		*pMaxSpeed = m_pSpeedup[Index].m_MaxSpeed;
}

int CCollision::GetSwitchType(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Type;

	return 0;
}

int CCollision::GetSwitchNumber(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0 && m_pSwitch[Index].m_Number > 0)
		return m_pSwitch[Index].m_Number;

	return 0;
}

int CCollision::GetSwitchDelay(int Index) const
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Delay;

	return 0;
}

int CCollision::IsMover(int x, int y, int *pFlags) const
{
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	int Index = m_pTiles[Ny * m_Width + Nx].m_Index;
	*pFlags = m_pTiles[Ny * m_Width + Nx].m_Flags;
	if(Index < 0)
		return 0;
	if(Index == TILE_CP || Index == TILE_CP_F)
		return Index;
	else
		return 0;
}

vec2 CCollision::CpSpeed(int Index, int Flags) const
{
	if(Index < 0)
		return vec2(0, 0);
	vec2 target;
	if(Index == TILE_CP || Index == TILE_CP_F)
		switch(Flags)
		{
		case ROTATION_0:
			target.x = 0;
			target.y = -4;
			break;
		case ROTATION_90:
			target.x = 4;
			target.y = 0;
			break;
		case ROTATION_180:
			target.x = 0;
			target.y = 4;
			break;
		case ROTATION_270:
			target.x = -4;
			target.y = 0;
			break;
		default:
			target = vec2(0, 0);
			break;
		}
	if(Index == TILE_CP_F)
		target *= 4;
	return target;
}

int CCollision::GetPureMapIndex(float x, float y) const
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return Ny * m_Width + Nx;
}

bool CCollision::TileExists(int Index) const
{
	if(Index < 0)
		return false;

	if((m_pTiles[Index].m_Index >= TILE_FREEZE && m_pTiles[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pTiles[Index].m_Index >= TILE_LFREEZE && m_pTiles[Index].m_Index <= TILE_LUNFREEZE))
		return true;
	if(m_pFront && ((m_pFront[Index].m_Index >= TILE_FREEZE && m_pFront[Index].m_Index <= TILE_TELE_LASER_DISABLE) || (m_pFront[Index].m_Index >= TILE_LFREEZE && m_pFront[Index].m_Index <= TILE_LUNFREEZE)))
		return true;
	if(m_pTele && (m_pTele[Index].m_Type == TILE_TELEIN || m_pTele[Index].m_Type == TILE_TELEINEVIL || m_pTele[Index].m_Type == TILE_TELECHECKINEVIL || m_pTele[Index].m_Type == TILE_TELECHECK || m_pTele[Index].m_Type == TILE_TELECHECKIN))
		return true;
	if(m_pSpeedup && m_pSpeedup[Index].m_Force > 0)
		return true;
	if(m_pDoor && m_pDoor[Index].m_Index)
		return true;
	if(m_pSwitch && m_pSwitch[Index].m_Type)
		return true;
	if(m_pTune && m_pTune[Index].m_Type)
		return true;
	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index) const
{
	if(Index < 0)
		return false;
	int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRight = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelow = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAbove = (Index - m_Width > 0) ? Index - m_Width : Index;

	if((m_pTiles[TileOnTheRight].m_Index == TILE_STOP && m_pTiles[TileOnTheRight].m_Flags == ROTATION_270) || (m_pTiles[TileOnTheLeft].m_Index == TILE_STOP && m_pTiles[TileOnTheLeft].m_Flags == ROTATION_90))
		return true;
	if((m_pTiles[TileBelow].m_Index == TILE_STOP && m_pTiles[TileBelow].m_Flags == ROTATION_0) || (m_pTiles[TileAbove].m_Index == TILE_STOP && m_pTiles[TileAbove].m_Flags == ROTATION_180))
		return true;
	if(m_pTiles[TileOnTheRight].m_Index == TILE_STOPA || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pTiles[TileOnTheRight].m_Index == TILE_STOPS || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPS)))
		return true;
	if(m_pTiles[TileBelow].m_Index == TILE_STOPA || m_pTiles[TileAbove].m_Index == TILE_STOPA || ((m_pTiles[TileBelow].m_Index == TILE_STOPS || m_pTiles[TileAbove].m_Index == TILE_STOPS) && m_pTiles[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
		return true;
	if(m_pFront)
	{
		if(m_pFront[TileOnTheRight].m_Index == TILE_STOPA || m_pFront[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pFront[TileOnTheRight].m_Index == TILE_STOPS || m_pFront[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pFront[TileBelow].m_Index == TILE_STOPA || m_pFront[TileAbove].m_Index == TILE_STOPA || ((m_pFront[TileBelow].m_Index == TILE_STOPS || m_pFront[TileAbove].m_Index == TILE_STOPS) && m_pFront[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pFront[TileOnTheRight].m_Index == TILE_STOP && m_pFront[TileOnTheRight].m_Flags == ROTATION_270) || (m_pFront[TileOnTheLeft].m_Index == TILE_STOP && m_pFront[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pFront[TileBelow].m_Index == TILE_STOP && m_pFront[TileBelow].m_Flags == ROTATION_0) || (m_pFront[TileAbove].m_Index == TILE_STOP && m_pFront[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	if(m_pDoor)
	{
		if(m_pDoor[TileOnTheRight].m_Index == TILE_STOPA || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pDoor[TileOnTheRight].m_Index == TILE_STOPS || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPS)))
			return true;
		if(m_pDoor[TileBelow].m_Index == TILE_STOPA || m_pDoor[TileAbove].m_Index == TILE_STOPA || ((m_pDoor[TileBelow].m_Index == TILE_STOPS || m_pDoor[TileAbove].m_Index == TILE_STOPS) && m_pDoor[TileBelow].m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((m_pDoor[TileOnTheRight].m_Index == TILE_STOP && m_pDoor[TileOnTheRight].m_Flags == ROTATION_270) || (m_pDoor[TileOnTheLeft].m_Index == TILE_STOP && m_pDoor[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pDoor[TileBelow].m_Index == TILE_STOP && m_pDoor[TileBelow].m_Flags == ROTATION_0) || (m_pDoor[TileAbove].m_Index == TILE_STOP && m_pDoor[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos) const
{
	int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;

	if(TileExists(Index))
		return Index;
	else
		return -1;
}

std::list<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices) const
{
	std::list<int> Indices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if(!d)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if(TileExists(Index))
		{
			Indices.push_back(Index);
			return Indices;
		}
		else
			return Indices;
	}
	else
	{
		int LastIndex = 0;
		for(int i = 0; i < End; i++)
		{
			float a = i / d;
			vec2 Tmp = mix(PrevPos, Pos, a);
			int Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
			int Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
			int Index = Ny * m_Width + Nx;
			if(TileExists(Index) && LastIndex != Index)
			{
				if(MaxIndices && Indices.size() > MaxIndices)
					return Indices;
				Indices.push_back(Index);
				LastIndex = Index;
			}
		}

		return Indices;
	}
}

vec2 CCollision::GetPos(int Index) const
{
	if(Index < 0)
		return vec2(0, 0);

	int x = Index % m_Width;
	int y = Index / m_Width;
	return vec2(x * 32 + 16, y * 32 + 16);
}

int CCollision::GetTileIndex(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetFTileIndex(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Index;
}

int CCollision::GetTileFlags(int Index) const
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

int CCollision::GetFTileFlags(int Index) const
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Flags;
}

int CCollision::GetIndex(int Nx, int Ny) const
{
	return m_pTiles[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetIndex(vec2 PrevPos, vec2 Pos) const
{
	float Distance = distance(PrevPos, Pos);

	if(!Distance)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);

		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	for(int i = 0, id = std::ceil(Distance); i < id; i++)
	{
		float a = (float)i / Distance;
		vec2 Tmp = mix(PrevPos, Pos, a);
		int Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
		if((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny * m_Width + Nx].m_Force > 0))
		{
			return Ny * m_Width + Nx;
		}
	}

	return -1;
}

int CCollision::GetFIndex(int Nx, int Ny) const
{
	if(!m_pFront)
		return 0;
	return m_pFront[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetFTile(int x, int y) const
{
	if(!m_pFront)
		return 0;
	int Nx = clamp(x / 32, 0, m_Width - 1);
	int Ny = clamp(y / 32, 0, m_Height - 1);
	if(m_pFront[Ny * m_Width + Nx].m_Index == TILE_DEATH || m_pFront[Ny * m_Width + Nx].m_Index == TILE_NOLASER)
		return m_pFront[Ny * m_Width + Nx].m_Index;
	else
		return 0;
}

int CCollision::Entity(int x, int y, int Layer) const
{
	if((0 > x || x >= m_Width) || (0 > y || y >= m_Height))
	{
		const char *pName;
		switch(Layer)
		{
		case LAYER_GAME:
			pName = "Game";
			break;
		case LAYER_FRONT:
			pName = "Front";
			break;
		case LAYER_SWITCH:
			pName = "Switch";
			break;
		case LAYER_TELE:
			pName = "Tele";
			break;
		case LAYER_SPEEDUP:
			pName = "Speedup";
			break;
		case LAYER_TUNE:
			pName = "Tune";
			break;
		default:
			pName = "Unknown";
		}
		dbg_msg("collision", "something is VERY wrong with the %s layer please report this at https://github.com/ddnet/ddnet, you will need to post the map as well and any steps that u think may have led to this", pName);
		return 0;
	}
	switch(Layer)
	{
	case LAYER_GAME:
		return m_pTiles[y * m_Width + x].m_Index - ENTITY_OFFSET;
	case LAYER_FRONT:
		return m_pFront[y * m_Width + x].m_Index - ENTITY_OFFSET;
	case LAYER_SWITCH:
		return m_pSwitch[y * m_Width + x].m_Type - ENTITY_OFFSET;
	case LAYER_TELE:
		return m_pTele[y * m_Width + x].m_Type - ENTITY_OFFSET;
	case LAYER_SPEEDUP:
		return m_pSpeedup[y * m_Width + x].m_Type - ENTITY_OFFSET;
	case LAYER_TUNE:
		return m_pTune[y * m_Width + x].m_Type - ENTITY_OFFSET;
	default:
		return 0;
		break;
	}
}

void CCollision::SetCollisionAt(float x, float y, int id)
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pTiles[Ny * m_Width + Nx].m_Index = id;
}

void CCollision::SetDCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if(!m_pDoor)
		return;
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pDoor[Ny * m_Width + Nx].m_Index = Type;
	m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
	m_pDoor[Ny * m_Width + Nx].m_Number = Number;
}

int CCollision::GetDTileIndex(int Index) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Index;
}

int CCollision::GetDTileNumber(int Index) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	if(m_pDoor[Index].m_Number)
		return m_pDoor[Index].m_Number;
	return 0;
}

int CCollision::GetDTileFlags(int Index) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Flags;
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY)
{
	float x = Pos0.x - Pos1.x;
	float y = Pos0.y - Pos1.y;
	if(absolute(x) > absolute(y))
	{
		if(x < 0)
		{
			*pOffsetX = -32;
			*pOffsetY = 0;
		}
		else
		{
			*pOffsetX = 32;
			*pOffsetY = 0;
		}
	}
	else
	{
		if(y < 0)
		{
			*pOffsetX = 0;
			*pOffsetY = -32;
		}
		else
		{
			*pOffsetX = 0;
			*pOffsetY = 32;
		}
	}
}

int CCollision::IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = (int)i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
		int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
		if(GetIndex(Nx, Ny) == TILE_SOLID || GetIndex(Nx, Ny) == TILE_NOHOOK || GetIndex(Nx, Ny) == TILE_NOLASER || GetFIndex(Nx, Ny) == TILE_NOLASER)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(GetFIndex(Nx, Ny) == TILE_NOLASER)
				return GetFCollisionAt(Pos.x, Pos.y);
			else
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

int CCollision::IntersectNoLaserNW(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = (float)i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)) || IsFNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetCollisionAt(Pos.x, Pos.y);
			else
				return GetFCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(int i = 0, id = std::ceil(d); i < id; i++)
	{
		float a = (float)i / d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsSolid(round_to_int(Pos.x), round_to_int(Pos.y)) || (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y))))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return -1;
			else if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return GetTile(round_to_int(Pos.x), round_to_int(Pos.y));
			else
				return GetFTile(round_to_int(Pos.x), round_to_int(Pos.y));
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IsTimeCheckpoint(int Index, const CMovingTileData* pMovingTile) const
{
	if (pMovingTile == nullptr) {
		if(Index < 0)
			return -1;

		int z = m_pTiles[Index].m_Index;
		if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
			return z - TILE_TIME_CHECKPOINT_FIRST;
		return -1;
	} else {
		return pMovingTile->IsTimeCheckpoint();
	}
}

int CCollision::IsFTimeCheckpoint(int Index) const
{
	if(Index < 0 || !m_pFront)
		return -1;

	int z = m_pFront[Index].m_Index;
	if(z >= TILE_TIME_CHECKPOINT_FIRST && z <= TILE_TIME_CHECKPOINT_LAST)
		return z - TILE_TIME_CHECKPOINT_FIRST;
	return -1;
}
