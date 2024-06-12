#pragma once

#include "pch.h"

std::string wstringToString(const std::wstring& wstr);

XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b);

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b);

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b);

XMFLOAT3 Normalize(const XMFLOAT3& v);

XMFLOAT3 Divide(const XMFLOAT3& a, float d);

XMFLOAT3 Mult(const XMFLOAT3& a, float d);

XMFLOAT3 Normalize(const XMFLOAT3& v, float& length);

XMFLOAT3 Negate(const XMFLOAT3& v);

float Magnitude(const XMFLOAT3& v);

float Dot(const XMFLOAT3& a, const XMFLOAT3& b);

bool Equals(XMFLOAT3 a, XMFLOAT3 b);

std::string ToString(XMFLOAT3& v);

bool NextCharactersMatch(std::ifstream& file, const std::string& expected, bool resetPos);