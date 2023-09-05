#ifndef GAME_EDITOR_MAPITEMS_LAYER_H
#define GAME_EDITOR_MAPITEMS_LAYER_H

#include <game/editor/component.h>

#include <game/mapitems.h>

using FIndexModifyFunction = std::function<void(int *pIndex)>;

class CLayerGroup;

class CLayer : public CEditorComponent
{
public:
	explicit CLayer(CEditor *pEditor)
	{
		Init(pEditor);
	}

	CLayer(const CLayer &Other) :
		CEditorComponent(Other)
	{
		str_copy(m_aName, Other.m_aName);
		m_Flags = Other.m_Flags;
		m_Type = Other.m_Type;
	}

	virtual ~CLayer() = default;

	virtual void BrushSelecting(CUIRect Rect) {}
	virtual int BrushGrab(const std::shared_ptr<CLayerGroup> &pBrush, CUIRect Rect) { return 0; }
	virtual void FillSelection(bool Empty, const std::shared_ptr<CLayer> &pBrush, CUIRect Rect) {}
	virtual void BrushDraw(const std::shared_ptr<CLayer> &pBrush, float x, float y) {}
	virtual void BrushPlace(const std::shared_ptr<CLayer> &pBrush, float x, float y) {}
	virtual void BrushFlipX() {}
	virtual void BrushFlipY() {}
	virtual void BrushRotate(float Amount) {}

	virtual bool IsEntitiesLayer() const { return false; }

	virtual void Render(bool Tileset = false) {}
	virtual CUI::EPopupMenuFunctionResult RenderProperties(CUIRect *pToolbox) { return CUI::POPUP_KEEP_OPEN; }

	virtual void ModifyImageIndex(const FIndexModifyFunction &pfnFunc) {}
	virtual void ModifyEnvelopeIndex(const FIndexModifyFunction &pfnFunc) {}
	virtual void ModifySoundIndex(const FIndexModifyFunction &pfnFunc) {}

	virtual std::shared_ptr<CLayer> Duplicate() const = 0;

	virtual void GetSize(float *pWidth, float *pHeight)
	{
		*pWidth = 0;
		*pHeight = 0;
	}

	char m_aName[12] = "(invalid)";
	int m_Type = LAYERTYPE_INVALID;
	int m_Flags = 0;

	bool m_Readonly = false;
	bool m_Visible = true;
};

#endif
