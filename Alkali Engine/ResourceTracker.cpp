#include "pch.h"
#include "ResourceTracker.h"

unordered_map<string, shared_ptr<Texture>> ResourceTracker::ms_textureMap;

void ResourceTracker::AddTexture(string filePath, shared_ptr<Texture> tex)
{
	if (ms_textureMap.contains(filePath))
		return;

	ms_textureMap.emplace(filePath, tex);
}

bool ResourceTracker::TryGetTexture(string filePath, shared_ptr<Texture>& tex)
{
	if (!ms_textureMap.contains(filePath))
	{
		tex = std::make_shared<Texture>();
		AddTexture(filePath, tex);
		return false;
	}		

	tex = ms_textureMap.at(filePath);
	return true;
}

void ResourceTracker::ReleaseAll()
{
	ms_textureMap.clear();
}
