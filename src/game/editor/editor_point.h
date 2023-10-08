#ifndef GAME_EDITOR_EDITOR_POINT_H
#define GAME_EDITOR_EDITOR_POINT_H

#include "editor_object.h"

class CEditorPoint : public CEditorObject
{
public:
    void OnHot() override;
    void OnActive() override;

    // 
    virtual void OnInitialClick() = 0;
    virtual void OnClick() = 0;
    virtual void OnRightClick() = 0;
    virtual void OnHover() = 0;
    virtual void OnDragStart() = 0;
    virtual void OnDragEnd() = 0;
    virtual void OnPositionUpdate(vec2 PositionOffset) = 0;
    virtual void OnContextMenu() = 0;

private:
    enum class EState
    {
        NONE,
        DRAGGING,
        CLICKED,
        CONTEXT_MENU
    } m_State;

    vec2 m_FirstClickPosition;
};

#endif
