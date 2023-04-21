#pragma once

#include <string>
#include "Util/Misc.h"

struct FGlobalConfig
{
	bool				bEnableAudio;
	bool				bShowScanLineIndicator = false;
	bool				bShowOpcodeValues = false;
	ENumberDisplayMode	NumberDisplayMode = ENumberDisplayMode::HexAitch;
	std::string			LastGame;

	std::string			WorkspaceRoot = "./";
	std::string			SnapshotFolder = "./Games/";
	std::string			SnapshotFolder128 = "./Games128/";

#if SPECCY
	std::string			PokesFolder = "./Pokes/";
#endif
};

FGlobalConfig& GetGlobalConfig();
bool LoadGlobalConfig(const char* fileName);
bool SaveGlobalConfig(const char* fileName);
