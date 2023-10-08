#include "editor_point.h"

#include <game/client/ui.h>

void CEditorPoint::OnHot()
{
    if(UI()->MouseButton(0))
    {
        SetActive();
        m_State = EState::CLICKED;
        m_FirstClickPosition = UI()->MousePos();
        OnInitialClick();
    }
    else if(UI()->MouseButtonClicked(1))
    {
        OnRightClick();
    }
    else
    {
        OnHover();
    }
}

void CEditorPoint::OnActive()
{
    vec2 MouseOffset = m_FirstClickPosition - UI()->MousePos();

    if(m_State == EState::CLICKED)
    {
        if(length(MouseOffset) > 20.0f)
        {
            m_State = EState::DRAGGING;
            OnDragStart();
        }
    }

    if(m_State == EState::DRAGGING)
    {
        OnPositionUpdate(MouseOffset);
    }

    if(m_State == EState::CONTEXT_MENU)
    {
        if(!UI()->MouseButton(1))
        {
            OnContextMenu();
            SetInactive();
        }
    }
    else if(!UI()->MouseButton(0))
    {
        SetInactive();

        if(m_State == EState::CLICKED)
            OnClick();
        else
            OnDragEnd();

        m_State = EState::NONE;
    }
}
