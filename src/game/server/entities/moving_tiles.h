#ifndef GAME_SERVER_ENTITIES_MOVING_TILE_H
#define GAME_SERVER_ENTITIES_MOVING_TILE_H

#include <game/server/entity.h>

#include <game/mapitems.h>

#include <memory>
#include <vector>

class CMovingTiles : public CEntity
{

public:
	CMovingTiles(CGameWorld *pGameWorld);

	void Tick() override;
};

#endif // GAME_SERVER_ENTITIES_MOVING_TILE_H