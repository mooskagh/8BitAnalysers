#include "Shared/GamesList/GamesList.h"

#include "SNALoader.h"

class FCpcEmu;

class FCpcGameLoader : public IGameLoader
{
public:
	void Init(FCpcEmu* pCpc)
	{
		pCpcEmu = pCpc;
	}
	bool LoadGame(const char* pFileName) override
	{
		const std::string fn(pFileName);

		switch (GetSnapshotTypeFromFileName(fn))
		{
		case ESnapshotType::SNA:
			return LoadSNAFile(pCpcEmu, pFileName);
		default: return false;
		}
	};
	ESnapshotType GetSnapshotTypeFromFileName(const std::string& fn) override
	{
		std::string fnLower = fn;
		std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), [](unsigned char c){ return std::tolower(c); });

		if (fnLower.substr(fn.find_last_of(".") + 1) == "sna")
			return ESnapshotType::SNA;
		else
			return ESnapshotType::Unknown;
	}
	FCpcEmu* pCpcEmu = 0;
};