#include "../Shared/GamesList/GamesList.h"

class FCpcEmu;

class FCpcGameLoader : public IGameLoader
{
public:
	void Init(FCpcEmu* pCpc)
	{
		pCpcEmu = pCpc;
	}
	bool LoadGame(const char* pFileName) override;
	ESnapshotType GetSnapshotTypeFromFileName(const std::string& fn) override;
	FCpcEmu* pCpcEmu = 0;
};