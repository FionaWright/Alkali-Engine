#pragma once

#include "pch.h"
#include "DebugLine.h"

using std::vector;

class Frustum
{
public:
	Frustum();
	~Frustum();

	void UpdateValues(XMMATRIX viewProj);
	void CalculateDebugLinePoints(D3DClass* d3d);
	bool CheckSphere(XMFLOAT3 pos, float radius);

	void SetDebugLines(vector<DebugLine*> list);
	void SetDebugLinesEnabled(bool enabled);

private:
	struct FrustumPlane
	{
		XMFLOAT3 Normal = XMFLOAT3_ZERO;
		float Distance = 0;
	};

	FrustumPlane m_frustumPlanes[6];
	vector<DebugLine*> m_debugLines;

	bool m_debugLinesEnabled = true;
};

