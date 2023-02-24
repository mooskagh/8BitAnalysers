#include "GameLoader.h"

#include "SNALoader.h"
#include <string>
#include <algorithm>

bool FCpcGameLoader::LoadGame(const char* pFileName)
{
	const std::string fn(pFileName);

	switch (GetSnapshotTypeFromFileName(fn))
	{
	case ESnapshotType::SNA:
		return LoadSNAFile(pCpcEmu, pFileName);
	default: return false;
	}
}

ESnapshotType FCpcGameLoader::GetSnapshotTypeFromFileName(const std::string& fn)
{
	std::string fnLower = fn;
	std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), [](unsigned char c){ return std::tolower(c); });

	if (fnLower.substr(fn.find_last_of(".") + 1) == "sna")
		return ESnapshotType::SNA;
	else
		return ESnapshotType::Unknown;
}
