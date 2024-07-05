#include "Utils.h"
#include "pch.h"
#include <iostream>
#include <fstream>

using std::string;

string wstringToString(const std::wstring& wstr)
{
	std::string str;
	std::transform(wstr.begin(), wstr.end(), std::back_inserter(str), [](wchar_t c) {
		return (char)c;
		});
	return str;
}

XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b)
{
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

XMFLOAT4 Add(XMFLOAT4 a, XMFLOAT4 b)
{
    return XMFLOAT4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

XMFLOAT3 Subtract(XMFLOAT3 a, XMFLOAT3 b)
{
	return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

XMFLOAT2 Subtract(XMFLOAT2 a, XMFLOAT2 b)
{
	return XMFLOAT2(a.x - b.x, a.y - b.y);
}

XMFLOAT2 Frac(const XMFLOAT2& a)
{
    float fracX = a.x - std::floor(a.x);
    float fracY = a.y - std::floor(a.y);
    return XMFLOAT2(fracX, fracY);
}

XMFLOAT2 NaiveFrac(XMFLOAT2 a)
{
    a.x -= a.x < 0 ? std::ceil(a.x) : std::floor(a.x);
    a.y -= a.y < 0 ? std::ceil(a.y) : std::floor(a.y);
    return a;
}

XMFLOAT2 Abs(const XMFLOAT2& a)
{
    return XMFLOAT2(std::abs(a.x), std::abs(a.y));
}

XMFLOAT3 Normalize(const XMFLOAT3& v)
{
	float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return XMFLOAT3(v.x / length, v.y / length, v.z / length);
}

XMFLOAT3 Divide(const XMFLOAT3& a, float d) 
{
    return XMFLOAT3(a.x / d, a.y / d, a.z / d);
}

XMFLOAT3 Mult(const XMFLOAT3& a, float d)
{
    return XMFLOAT3(a.x * d, a.y * d, a.z * d);
}

XMFLOAT4 Mult(const XMFLOAT4& a, float d)
{
    return XMFLOAT4(a.x * d, a.y * d, a.z * d, a.w * d);
}

XMFLOAT3 Mult(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x * b.x, a.y * b.y, a.z * b.z);
}

XMFLOAT3 Normalize(const XMFLOAT3& v, float& length)
{
    length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return XMFLOAT3(v.x / length, v.y / length, v.z / length);
}

XMFLOAT3 Negate(const XMFLOAT3& v) 
{
    return XMFLOAT3(-v.x, -v.y, -v.z);
}

float Magnitude(const XMFLOAT3& v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float Dot(const XMFLOAT3& a, const XMFLOAT3& b) 
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float SqDist(const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMFLOAT3 a_b = Subtract(a, b);
    return Dot(a_b, a_b);
}

XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
{
    XMFLOAT3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

bool Equals(XMFLOAT3 a, XMFLOAT3 b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

string ToString(XMFLOAT3& v) 
{
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}

bool NextCharactersMatch(std::ifstream& file, const std::string& expected, bool resetPos) 
{
    std::streampos startPosition = file.tellg();

    for (char c : expected)
    {
        if (file.peek() != c)
        {
            if (resetPos)
                file.seekg(startPosition);
            return false;            
        }
        
        file.get();
    }

    if (resetPos)
        file.seekg(startPosition);
    return true;
}

bool XOR(bool a, bool b)
{
    return !a != !b;
}