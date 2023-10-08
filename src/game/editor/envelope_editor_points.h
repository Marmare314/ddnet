#ifndef GAME_EDITOR_ENVELOPE_EDITOR_POINTS_H
#define GAME_EDITOR_ENVELOPE_EDITOR_POINTS_H

#include "editor_point.h"
#include <game/mapitems.h>

#include <memory>

class CEnvelope;

class CTangentHandleStart : public CEditorPoint
{
public:
    explicit CTangentHandleStart(CEditor *pEditor, const std::shared_ptr<CEnvelope> &pEnvelope); 
    void OnRender(CUIRect View) override;

    void OnInitialClick() override;
    void OnClick() override;
    void OnRightClick() override;
    void OnHover() override;
    void OnDragStart() override;
    void OnDragEnd() override;
    void OnPositionUpdate(vec2 PositionOffset) override;
    void OnContextMenu() override;

private:
    std::shared_ptr<CEnvelope> m_pEnvelope;
};

class CEnvelopePoint
{
public:
    CEnvelopePoint(CEditor *pEditor, int NumChannels);

    CEnvPoint m_EnvPoint;
    CEnvPointBezier m_Bezier;

    std::vector<CTangentHandleStart> m_TangentHandlesStart;
};

#endif

// Out-Tangent handle
// if(pEnvelope->m_vPoints[i].m_CurveInfo.Type() == CURVETYPE_BEZIER && !pEnvelope->m_vPoints[i].IsLastPoint())
// {
//     vec2 TangentHandleStart = pEnvelope->m_vPoints[i].m_CurveInfo.BezierTangentHandleStart(c);
//     CUIRect Final;
//     Final.x = EnvelopeToScreenX(View, TangentHandleStart.x);
//     Final.y = EnvelopeToScreenY(View, TangentHandleStart.y);
//     Final.x -= 2.0f;
//     Final.y -= 2.0f;
//     Final.w = 4.0f;
//     Final.h = 4.0f;

//     // handle logic
//     const void *pID = &pEnvelope->m_vPoints[i].m_Bezier.m_aOutTangentDeltaX[c];

//     if(IsTangentOutPointSelected(i, c))
//     {
//         Graphics()->SetColor(1, 1, 1, 1);
//         IGraphics::CFreeformItem FreeformItem(
//             Final.x + Final.w / 2.0f,
//             Final.y - 1,
//             Final.x + Final.w / 2.0f,
//             Final.y - 1,
//             Final.x + Final.w + 1,
//             Final.y + Final.h + 1,
//             Final.x - 1,
//             Final.y + Final.h + 1);
//         Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
//     }

//     if(UI()->CheckActiveItem(pID))
//     {
//         m_ShowEnvelopePreview = SHOWENV_SELECTED;

//         if(s_Operation == OP_SELECT)
//         {
//             float dx = s_MouseXStart - UI()->MouseX();
//             float dy = s_MouseYStart - UI()->MouseY();

//             if(dx * dx + dy * dy > 20.0f)
//             {
//                 s_Operation = OP_DRAG_POINT;

//                 s_vAccurateDragValuesX = {TangentHandleStart.x};
//                 s_vAccurateDragValuesY = {TangentHandleStart.y};

//                 if(!IsTangentOutPointSelected(i, c))
//                     SelectTangentOutPoint(i, c);
//             }
//         }

//         if(s_Operation == OP_DRAG_POINT)
//         {
//             float DeltaX = ScreenToEnvelopeDX(View, UI()->MouseDeltaX()) * (Input()->ModifierIsPressed() ? 0.05f : 1.0f);
//             float DeltaY = ScreenToEnvelopeDY(View, UI()->MouseDeltaY()) * (Input()->ModifierIsPressed() ? 0.05f : 1.0f);
//             s_vAccurateDragValuesX[0] += DeltaX;
//             s_vAccurateDragValuesY[0] -= DeltaY;

//             // clamp time value
//             s_vAccurateDragValuesX[0] = maximum(s_vAccurateDragValuesX[0], pEnvelope->m_vPoints[i].Time());

//             pEnvelope->m_vPoints[i].m_CurveInfo.SetBezierTangentHandleStart(c, {s_vAccurateDragValuesX[0], s_vAccurateDragValuesY[0]});
//         }

//         if(s_Operation == OP_CONTEXT_MENU)
//         {
//             if(!UI()->MouseButton(1))
//             {
//                 if(IsTangentOutPointSelected(i, c))
//                 {
//                     m_UpdateEnvPointInfo = true;
//                     static SPopupMenuId s_PopupEnvPointId;
//                     UI()->DoPopupMenu(&s_PopupEnvPointId, UI()->MouseX(), UI()->MouseY(), 150, 56, this, PopupEnvPoint);
//                 }
//                 UI()->SetActiveItem(nullptr);
//             }
//         }
//         else if(!UI()->MouseButton(0))
//         {
//             UI()->SetActiveItem(nullptr);
//             m_SelectedQuadEnvelope = -1;

//             if(s_Operation == OP_SELECT)
//                 SelectTangentOutPoint(i, c);

//             s_Operation = OP_NONE;
//             m_Map.OnModify();
//         }

//         Graphics()->SetColor(1, 1, 1, 1);
//     }
//     else if(UI()->HotItem() == pID)
//     {

//     }
//     else
//         Graphics()->SetColor(aColors[c].r, aColors[c].g, aColors[c].b, 1.0f);

//     // draw triangle
//     IGraphics::CFreeformItem FreeformItem(Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w / 2.0f, Final.y, Final.x + Final.w, Final.y + Final.h, Final.x, Final.y + Final.h);
//     Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
// }
