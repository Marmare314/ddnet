#include "envelope_editor_points.h"

#include "editor.h"

CTangentHandleStart::CTangentHandleStart(CEditor *pEditor, const std::shared_ptr<CEnvelope> &pEnvelope) :
	m_pEnvelope(pEnvelope)
{
	Init(pEditor);
}

void CTangentHandleStart::OnRender(CUIRect View)
{
	// vec2 TangentHandleStart = pEnvelope->m_vPoints[i].m_CurveInfo.BezierTangentHandleStart(c);
	// CUIRect Final;
	// Final.x = EnvelopeToScreenX(View, TangentHandleStart.x);
	// Final.y = EnvelopeToScreenY(View, TangentHandleStart.y);
	// Final.x -= 2.0f;
	// Final.y -= 2.0f;
	// Final.w = 4.0f;
	// Final.h = 4.0f;
}

void CTangentHandleStart::OnInitialClick()
{
}

void CTangentHandleStart::OnClick()
{
}

void CTangentHandleStart::OnRightClick()
{
}

void CTangentHandleStart::OnHover()
{
	// Editor()->m_ShowEnvelopePreview = SHOWENV_SELECTED;
	str_copy(Editor()->m_aTooltip, "Bezier out-tangent. Left mouse to drag. Hold ctrl to be more precise. Shift + right-click to reset.");
	CEditor::ms_pUiGotContext = this;
}

void CTangentHandleStart::OnDragStart()
{
}

void CTangentHandleStart::OnDragEnd()
{
}

void CTangentHandleStart::OnPositionUpdate(vec2 PositionOffset)
{
}

void CTangentHandleStart::OnContextMenu()
{
}

CEnvelopeEditorPoint::CEnvelopeEditorPoint(CEditor *pEditor, const std::shared_ptr<CEnvelope> &pEnvelope, int NumChannels)
{
	for(int i = 0; i < NumChannels; i++)
	{
		m_TangentHandlesStart.emplace_back(pEditor, pEnvelope);
	}
}
