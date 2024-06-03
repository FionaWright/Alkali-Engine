#pragma once

#include "pch.h"

std::string wstringToString(const std::wstring& wstr);

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b);

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b);

XMFLOAT3 Normalize(const XMFLOAT3& v);

bool Equals(XMFLOAT3 a, XMFLOAT3 b);

bool NextCharactersMatch(std::ifstream& file, const std::string& expected, bool resetPos);