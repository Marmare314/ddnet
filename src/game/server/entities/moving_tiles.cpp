#include "moving_tiles.h"
#include "game/collision.h"
#include "character.h"

CMovingTiles::CMovingTiles(CGameWorld *pGameWorld) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVINGTILES)
{
    GameWorld()->InsertEntity(this);
}

void CMovingTiles::Tick()
{
    CCharacter *pChr = static_cast<CCharacter*>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
    for(; pChr != nullptr; pChr = static_cast<CCharacter*>(pChr->TypeNext()))
	{
        auto coll_ids = Collision()->GetQuadCollisionsBetween(pChr->m_PrevPos, pChr->m_Pos, 1.0f);

        for (auto id: coll_ids) {
            switch (id) {
                case TILE_FREEZE:
                    pChr->Freeze();
                    break;
            }
        }
    }
}
