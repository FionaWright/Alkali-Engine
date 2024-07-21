#pragma once

#include "pch.h"
#include "DebugLine.h"

using std::vector;

class Frustum
{
public:
	void UpdateValues(XMMATRIX viewProj);
	void CalculateDebugLinePoints(D3DClass* d3d);
	bool CheckSphere(XMFLOAT3 pos, float radius, float nearPercent = 0, float farPercent = 1);

	void SetDebugLines(vector<DebugLine*> list);
	void SetDebugLinesEnabled(bool enabled);

	void GetBoundingBoxFromDir(const XMFLOAT3& origin, const XMFLOAT3& dir, float nearPercent, float farPercent, float& width, float& height, float& nearDist, float& farDist);

private:
	struct FrustumPlane
	{
		XMFLOAT3 Normal = XMFLOAT3_ZERO;
		float Distance = 0;
	};

	FrustumPlane m_frustumPlanes[6];
	vector<DebugLine*> m_debugLines;

	int m_nearIndex = -1, m_farIndex = -1;

	bool m_debugLinesEnabled = true;
};

