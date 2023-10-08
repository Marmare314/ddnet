#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_H

#include <game/client/render.h>
#include <game/mapitems.h>

#include <game/editor/envelope_editor_points.h>

class CEnvelope : public CEditorObject
{
public:
	std::vector<CEnvPoint_runtime> m_vPoints;
	char m_aName[32] = "";
	bool m_Synchronized = false;

	enum class EType
	{
		POSITION,
		COLOR,
		SOUND
	};
	CEnvelope(CEditor *pEditor, EType Type);
	CEnvelope(CEditor *pEditor, int NumChannels);
	CEnvelope(CEditor *pEditor, int NumChannels, const std::vector<CEnvPoint_runtime>& vPoints);

	std::pair<float, float> GetValueRange(int ChannelMask);
	int Eval(float Time, ColorRGBA &Color);
	void AddPoint(int Time, int v0, int v1 = 0, int v2 = 0, int v3 = 0);
	float EndTime() const;
	int GetChannels() const;
	std::vector<CEnvelopeEditorPoint> m_vEditorPoints;

private:
	void Resort();

	EType m_Type;

	class CEnvelopePointAccess : public IEnvelopePointAccess
	{
		std::vector<CEnvPoint_runtime> *m_pvPoints;

	public:
		CEnvelopePointAccess(std::vector<CEnvPoint_runtime> *pvPoints);

		int NumPoints() const override;
		const CEnvPoint *GetPoint(int Index) const override;
		const CEnvPointBezier *GetBezier(int Index) const override;
	};
	CEnvelopePointAccess m_PointsAccess;
};

#endif
