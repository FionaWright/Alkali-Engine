#include "pch.h"
#include "Frustum.h"
#include "Utils.h"

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
    m_nearIndex = 4;

    // Far Plane
    m_frustumPlanes[5].Normal.x = viewProj.r[0].m128_f32[3] - viewProj.r[0].m128_f32[2];
    m_frustumPlanes[5].Normal.y = viewProj.r[1].m128_f32[3] - viewProj.r[1].m128_f32[2];
    m_frustumPlanes[5].Normal.z = viewProj.r[2].m128_f32[3] - viewProj.r[2].m128_f32[2];
    m_frustumPlanes[5].Distance = viewProj.r[3].m128_f32[3] - viewProj.r[3].m128_f32[2];
    m_farIndex = 5;

    // Normalize the planes
    for (int i = 0; i < 6; ++i)
    {
        float length;
        m_frustumPlanes[i].Normal = Normalize(m_frustumPlanes[i].Normal, length);
        m_frustumPlanes[i].Distance /= length;
    }    
}

void Frustum::CalculateDebugLinePoints(D3DClass* d3d)
{
    auto IntersectPlanes = [](const FrustumPlane& p1, const FrustumPlane& p2, const FrustumPlane& p3) -> XMFLOAT3 
    {
        XMFLOAT3 result;
        XMVECTOR n1 = XMLoadFloat3(&p1.Normal);
        XMVECTOR n2 = XMLoadFloat3(&p2.Normal);
        XMVECTOR n3 = XMLoadFloat3(&p3.Normal);
        XMVECTOR n1xn2 = XMVector3Cross(n1, n2);
        XMVECTOR n2xn3 = XMVector3Cross(n2, n3);
        XMVECTOR n3xn1 = XMVector3Cross(n3, n1);
        float d1 = p1.Distance;
        float d2 = p2.Distance;
        float d3 = p3.Distance;
        XMVECTOR nom = (d1 * n2xn3 + d2 * n3xn1 + d3 * n1xn2);
        float denom = XMVectorGetX(XMVector3Dot(n1, n2xn3));
        XMVECTOR point = nom / denom;
        XMStoreFloat3(&result, point);
        //return result;
        return XMFLOAT3(-result.x, -result.y, -result.z);
    };

    // Extract the frustum corners by intersecting planes
    XMFLOAT3 NNW = IntersectPlanes(m_frustumPlanes[0], m_frustumPlanes[2], m_frustumPlanes[4]); // Near Top Left
    XMFLOAT3 NNE = IntersectPlanes(m_frustumPlanes[1], m_frustumPlanes[2], m_frustumPlanes[4]); // Near Top Right
    XMFLOAT3 NSW = IntersectPlanes(m_frustumPlanes[0], m_frustumPlanes[3], m_frustumPlanes[4]); // Near Bottom Left
    XMFLOAT3 NSE = IntersectPlanes(m_frustumPlanes[1], m_frustumPlanes[3], m_frustumPlanes[4]); // Near Bottom Right

    XMFLOAT3 FNW = IntersectPlanes(m_frustumPlanes[0], m_frustumPlanes[2], m_frustumPlanes[5]); // Far Top Left
    XMFLOAT3 FNE = IntersectPlanes(m_frustumPlanes[1], m_frustumPlanes[2], m_frustumPlanes[5]); // Far Top Right
    XMFLOAT3 FSW = IntersectPlanes(m_frustumPlanes[0], m_frustumPlanes[3], m_frustumPlanes[5]); // Far Bottom Left
    XMFLOAT3 FSE = IntersectPlanes(m_frustumPlanes[1], m_frustumPlanes[3], m_frustumPlanes[5]); // Far Bottom Right

    m_debugLines.at(0)->SetPositions(d3d, NNW, NNE);
    m_debugLines.at(1)->SetPositions(d3d, NNW, NSW);
    m_debugLines.at(2)->SetPositions(d3d, NNE, NSE);
    m_debugLines.at(3)->SetPositions(d3d, NSW, NSE);

    m_debugLines.at(4)->SetPositions(d3d, FNW, FNE);
    m_debugLines.at(5)->SetPositions(d3d, FNW, FSW);
    m_debugLines.at(6)->SetPositions(d3d, FNE, FSE);
    m_debugLines.at(7)->SetPositions(d3d, FSW, FSE);

    m_debugLines.at(8)->SetPositions(d3d, NNW, FNW);
    m_debugLines.at(9)->SetPositions(d3d, NNE, FNE);
    m_debugLines.at(10)->SetPositions(d3d, NSW, FSW);
    m_debugLines.at(11)->SetPositions(d3d, NSE, FSE);
}

bool Frustum::CheckSphere(XMFLOAT3 pos, float radius, float nearPercent, float farPercent)
{    
    float nearFarDist = m_frustumPlanes[m_farIndex].Distance - m_frustumPlanes[m_nearIndex].Distance;

    for (int i = 0; i < 6; ++i)
    {
        float bonusDist = 0;
        if (i == m_nearIndex)
            bonusDist = nearFarDist * nearPercent;
        else if (i == m_farIndex)
            bonusDist = -nearFarDist * (1 - farPercent);

        float distanceToPlane = Dot(m_frustumPlanes[i].Normal, pos) + m_frustumPlanes[i].Distance + bonusDist;
        if (distanceToPlane < -radius)
            return false;
    }

    return true;
}

void Frustum::SetDebugLines(vector<DebugLine*> list)
{
    m_debugLines = list;
}

void Frustum::SetDebugLinesEnabled(bool enabled)
{
    if (enabled == m_debugLinesEnabled)
        return;

    m_debugLinesEnabled = enabled;

    for (int i = 0; i < m_debugLines.size(); i++)
    {
        m_debugLines.at(i)->SetEnabled(enabled);
    }
}

void Frustum::GetBoundingBoxFromDir(const XMFLOAT3& origin, const XMFLOAT3& dir, float nearPercent, float farPercent, float& width, float& height, float& nearDist, float& farDist)
{   
    XMFLOAT3 frustumCorners[8];

    auto Intersection = [](const FrustumPlane& p1, const FrustumPlane& p2, const FrustumPlane& p3, float& nearFarOffset) -> XMFLOAT3
    {
        XMFLOAT3 u = Cross(p2.Normal, p3.Normal);
        float denom = Dot(p1.Normal, u);
        XMFLOAT3 point = Divide(
            Add(Mult(Cross(p2.Normal, p3.Normal), p1.Distance),
            Add(Mult(Cross(p3.Normal, p1.Normal), p2.Distance),
            Mult(Cross(p1.Normal, p2.Normal), p3.Distance + nearFarOffset))), denom);
        return point;
    };

    float nearFarDist = SettingsManager::ms_Dynamic.FarPlane - SettingsManager::ms_Dynamic.NearPlane;
    float nearOffset = nearFarDist * nearPercent;
    float farOffset = -nearFarDist * (1 - farPercent);

    frustumCorners[0] = Intersection(m_frustumPlanes[0], m_frustumPlanes[2], m_frustumPlanes[4], nearOffset); // Left, Top, Near
    frustumCorners[1] = Intersection(m_frustumPlanes[0], m_frustumPlanes[3], m_frustumPlanes[4], nearOffset); // Left, Bottom, Near
    frustumCorners[2] = Intersection(m_frustumPlanes[1], m_frustumPlanes[2], m_frustumPlanes[4], nearOffset); // Right, Top, Near
    frustumCorners[3] = Intersection(m_frustumPlanes[1], m_frustumPlanes[3], m_frustumPlanes[4], nearOffset); // Right, Bottom, Near

    frustumCorners[4] = Intersection(m_frustumPlanes[0], m_frustumPlanes[2], m_frustumPlanes[5], farOffset); // Left, Top, Far
    frustumCorners[5] = Intersection(m_frustumPlanes[0], m_frustumPlanes[3], m_frustumPlanes[5], farOffset); // Left, Bottom, Far
    frustumCorners[6] = Intersection(m_frustumPlanes[1], m_frustumPlanes[2], m_frustumPlanes[5], farOffset); // Right, Top, Far
    frustumCorners[7] = Intersection(m_frustumPlanes[1], m_frustumPlanes[3], m_frustumPlanes[5], farOffset); // Right, Bottom, Far

    XMFLOAT3 forwardBasis = Normalize(dir);
    if (Equals(forwardBasis, XMFLOAT3(0, -1, 0)))
        forwardBasis = Normalize(XMFLOAT3(0.01f, -1, 0.01f));

    XMFLOAT3 rightBasis = Normalize(Cross(forwardBasis, XMFLOAT3(0, 1, 0)));
    XMFLOAT3 upBasis = Normalize(Cross(forwardBasis, rightBasis));

    float maxX = -INFINITY, maxY = -INFINITY;

    for (int i = 0; i < 8; i++)
    {
        XMFLOAT3 pTransformed = Add(Add(Mult(rightBasis, frustumCorners[i].x), Mult(upBasis, frustumCorners[i].y)), Mult(forwardBasis, frustumCorners[i].z));
        pTransformed = Subtract(pTransformed, origin);

        maxX = std::max(maxX, abs(pTransformed.x));
        maxY = std::max(maxY, abs(pTransformed.y));

        nearDist = std::min(nearDist, pTransformed.z);
        farDist = std::max(farDist, pTransformed.z);
    }

    width = 2 * maxX;
    height = 2 * maxY;
}
