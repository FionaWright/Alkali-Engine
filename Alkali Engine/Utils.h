#pragma once

#include "pch.h"

struct Transform;

std::string wstringToString(const std::wstring& wstr);

XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b);

XMFLOAT4 Add(XMFLOAT4 a, XMFLOAT4 b);

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b);

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b);

XMFLOAT2 Frac(const XMFLOAT2& a);

XMFLOAT2 NaiveFrac(XMFLOAT2 a);

XMFLOAT2 Abs(const XMFLOAT2& a);

XMFLOAT3 Abs(const XMFLOAT3& a);

XMFLOAT3 Normalize(const XMFLOAT3& v);

XMFLOAT3 Divide(const XMFLOAT3& a, float d);

XMFLOAT3 Mult(const XMFLOAT3& a, float d);

XMFLOAT4 Mult(const XMFLOAT4& a, float d);

XMFLOAT3 Mult(const XMFLOAT3& a, const XMFLOAT3& b);

float SqDist(const XMFLOAT3& a, const XMFLOAT3& b);

XMFLOAT3 Normalize(const XMFLOAT3& v, float& length);

XMFLOAT3 Negate(const XMFLOAT3& v);

XMFLOAT3 Saturate(const XMFLOAT3& v);

float Magnitude(const XMFLOAT3& v);

float Dot(const XMFLOAT3& a, const XMFLOAT3& b);

XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b);

bool Equals(XMFLOAT3 a, XMFLOAT3 b);

std::string ToString(XMFLOAT3& v);

bool NextCharactersMatch(std::ifstream& file, const std::string& expected, bool resetPos);

bool XOR(bool a, bool b);

template <typename T>
bool Contains(vector<T>& v, T& obj)
{
	return std::find(v.begin(), v.end(), obj) != v.end();
};