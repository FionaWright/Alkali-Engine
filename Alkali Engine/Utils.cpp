#include "Utils.h"
#include "pch.h"

std::string wstringToString(const std::wstring& wstr)
{
	std::string str;
	std::transform(wstr.begin(), wstr.end(), std::back_inserter(str), [](wchar_t c) {
		return (char)c;
		});
	return str;
}

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b)
{
	return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b)
{
	return XMFLOAT2(a.x - b.x, a.y - b.y);
}

XMFLOAT3 Normalize(const XMFLOAT3& v)
{
	float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return XMFLOAT3(v.x / length, v.y / length, v.z / length);
}

bool Equals(XMFLOAT3 a, XMFLOAT3 b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}