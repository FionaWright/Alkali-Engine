#pragma once

#include "pch.h"

class Frustum
{
public:
	Frustum();
	~Frustum();

	void UpdateValues(XMMATRIX viewProj);
	bool CheckSphere(XMFLOAT3 pos, float radius);

private:
	struct FrustumPlane
	{
		XMFLOAT3 Normal;
		float Distance;
	};

	FrustumPlane m_frustumPlanes[6];
};

