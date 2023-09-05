#ifndef GAME_EDITOR_MAPITEMS_LAYER_GAME_H
#define GAME_EDITOR_MAPITEMS_LAYER_GAME_H

#include "layer_tiles.h"

class CLayerGame : public CLayerTiles
{
public:
	CLayerGame(int w, int h);
	~CLayerGame();

	CTile GetTile(int x, int y) override;
	void SetTile(int x, int y, CTile Tile) override;

	CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) override;
};

#endif
