#include "pch.h"
#include "Frustum.h"
#include "Utils.h"

Frustum::Frustum()
{
}

Frustum::~Frustum()
{
}

void Frustum::UpdateValues(XMMATRIX viewProj)
{
    // Left Plane
    m_frustumPlanes[0].Normal.x = viewProj.r[0].m128_f32[3] + viewProj.r[0].m128_f32[0];
    m_frustumPlanes[0].Normal.y = viewProj.r[1].m128_f32[3] + viewProj.r[1].m128_f32[0];
    m_frustumPlanes[0].Normal.z = viewProj.r[2].m128_f32[3] + viewProj.r[2].m128_f32[0];
    m_frustumPlanes[0].Distance = viewProj.r[3].m128_f32[3] + viewProj.r[3].m128_f32[0];

    // Right Plane
    m_frustumPlanes[1].Normal.x = viewProj.r[0].m128_f32[3] - viewProj.r[0].m128_f32[0];
    m_frustumPlanes[1].Normal.y = viewProj.r[1].m128_f32[3] - viewProj.r[1].m128_f32[0];
    m_frustumPlanes[1].Normal.z = viewProj.r[2].m128_f32[3] - viewProj.r[2].m128_f32[0];
    m_frustumPlanes[1].Distance = viewProj.r[3].m128_f32[3] - viewProj.r[3].m128_f32[0];

    // Top Plane
    m_frustumPlanes[2].Normal.x = viewProj.r[0].m128_f32[3] - viewProj.r[0].m128_f32[1];
    m_frustumPlanes[2].Normal.y = viewProj.r[1].m128_f32[3] - viewProj.r[1].m128_f32[1];
    m_frustumPlanes[2].Normal.z = viewProj.r[2].m128_f32[3] - viewProj.r[2].m128_f32[1];
    m_frustumPlanes[2].Distance = viewProj.r[3].m128_f32[3] - viewProj.r[3].m128_f32[1];

    // Bottom Plane
    m_frustumPlanes[3].Normal.x = viewProj.r[0].m128_f32[3] + viewProj.r[0].m128_f32[1];
    m_frustumPlanes[3].Normal.y = viewProj.r[1].m128_f32[3] + viewProj.r[1].m128_f32[1];
    m_frustumPlanes[3].Normal.z = viewProj.r[2].m128_f32[3] + viewProj.r[2].m128_f32[1];
    m_frustumPlanes[3].Distance = viewProj.r[3].m128_f32[3] + viewProj.r[3].m128_f32[1];

    // Near Plane
    m_frustumPlanes[4].Normal.x = viewProj.r[0].m128_f32[2];
    m_frustumPlanes[4].Normal.y = viewProj.r[1].m128_f32[2];
    m_frustumPlanes[4].Normal.z = viewProj.r[2].m128_f32[2];
    m_frustumPlanes[4].Distance = viewProj.r[3].m128_f32[2];

    // Far Plane
    m_frustumPlanes[5].Normal.x = viewProj.r[0].m128_f32[3] - viewProj.r[0].m128_f32[2];
    m_frustumPlanes[5].Normal.y = viewProj.r[1].m128_f32[3] - viewProj.r[1].m128_f32[2];
    m_frustumPlanes[5].Normal.z = viewProj.r[2].m128_f32[3] - viewProj.r[2].m128_f32[2];
    m_frustumPlanes[5].Distance = viewProj.r[3].m128_f32[3] - viewProj.r[3].m128_f32[2];

    // Normalize the planes
    for (int i = 0; i < 6; ++i)
    {
        float length;
        m_frustumPlanes[i].Normal = Normalize(m_frustumPlanes[i].Normal, length);
        m_frustumPlanes[i].Distance /= length;
    }
}

bool Frustum::CheckSphere(XMFLOAT3 pos, float radius)
{
    for (int i = 0; i < 6; ++i)
    {
        float distance = Dot(m_frustumPlanes[i].Normal, pos) + m_frustumPlanes[i].Distance;
        if (distance < -radius)
            return false;
    }

    return true;
}
