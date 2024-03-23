#pragma once

#include <Misc/GameConfig.h>
#include "CPCEmu.h"    // for ECPCModel enum

struct FGame;
struct FGameSnapshot;
class FCPCEmu;

// CPC specific
struct FCPCGameConfig : FGameConfig
{
	void	LoadFromJson(const nlohmann::json& jsonConfig) override;
	void	SaveToJson(nlohmann::json& jsonConfig) const override;

	ECPCModel  GetCPCModel() const { return bCPC6128Game ? ECPCModel::CPC_6128 : ECPCModel::CPC_464; }

	bool	bCPC6128Game = true;
};

FCPCGameConfig* CreateNewCPCGameConfigFromSnapshot(const FGameSnapshot& snapshot);
FCPCGameConfig* CreateNewAmstradBasicConfig(bool bIs6128);
bool LoadCPCGameConfigs(FCPCEmu* pUI);
