#pragma once

#include "pch.h"
#include <unordered_map>
#include "Texture.h"

using std::string;
using std::unordered_map;

class ResourceTracker
{
public:
	static void AddTexture(string filePath, shared_ptr<Texture> tex);
	static bool TryGetTexture(string filePath, shared_ptr<Texture>& tex);

	static void ReleaseAll();

private:
	static unordered_map<string, shared_ptr<Texture>> ms_textureMap;
};

