#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_H

#include <game/client/render.h>

#include "envelope_point.h"

class CEnvelope
{
public:
	std::vector<CEnvelopePoint> m_vPoints;
	char m_aName[32] = "";
	bool m_Synchronized = false;

	enum class EType
	{
		POSITION,
		COLOR,
		SOUND
	};
	explicit CEnvelope(EType Type);
	explicit CEnvelope(int NumChannels);

	std::pair<float, float> GetValueRange(int ChannelMask);
	int Eval(float Time, ColorRGBA &Color);
	void AddPoint(int Time, int v0, int v1 = 0, int v2 = 0, int v3 = 0);
	float EndTime() const;
	int GetChannels() const;

private:
	void Resort();

	EType m_Type;

	class CEnvelopePointAccess : public IEnvelopePointAccess
	{
		std::vector<CEnvelopePoint> *m_pvPoints;

	public:
		CEnvelopePointAccess(std::vector<CEnvelopePoint> *pvPoints);

		int NumPoints() const override;
		const CEnvPoint *GetPoint(int Index) const override;
		const CEnvPointBezier *GetBezier(int Index) const override;
	};
	CEnvelopePointAccess m_PointsAccess;
};

#endif
