#pragma once

#include <string>
#include <vector>

class FSystem;

enum class ESnapshotType
{
	Z80,
	SNA,
	RZX,

	Unknown
};

struct FGameSnapshot
{
	ESnapshotType	Type;
	std::string		DisplayName;
	std::string		FileName;
};

class IGameLoader
{
public:
	virtual bool LoadGame(const char* pFileName) = 0;
	virtual ESnapshotType GetSnapshotTypeFromFileName(const std::string& fn) = 0;
};

class FGamesList
{
public:
	void	Init(IGameLoader* pLoader) 
	{ 
		pGameLoader = pLoader;
	}
	bool	EnumerateGames(const char* pRootDir);
	bool	LoadGame(int index);
	// make pure virtual
	//bool	LoadGame(const char* pFileName);

	int		GetNoGames() const { return (int)GamesList.size(); }
	const FGameSnapshot& GetGame(int index) const { return GamesList[index]; }
	//const std::string& GetGameName(int index) const { return GamesList[index].DisplayName; }

private:
	//FSystem* pSystem = nullptr;
	std::vector< FGameSnapshot>	GamesList;
	std::string RootDir;
	IGameLoader* pGameLoader = 0;
};

// we will need to provide an implmentation per platform
//ESnapshotType GetSnapshotTypeFromFileName(const std::string& fn);