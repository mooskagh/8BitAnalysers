#pragma once

#include "Misc/EmuBase.h"

class FCPCEmu;

struct FOverviewStats
{
	float PercentReadOnlyData = 0.0f;
	float PercentReadWriteData = 0.0f;
	float PercentWriteOnlyData = 0.0f;
	float PercentCommentedCode = 0.0f;
	float PercentUncommentedCode = 0.0f;
	float PercentUnknown = 0.0f;
};

class FOverviewViewer : public FViewerBase
{
public:
			FOverviewViewer(FEmuBase* pEmu) : FViewerBase(pEmu) { Name = "Overview"; }

	bool	Init(void) override { return true; }
	void	DrawUI(void) override;

	void	DrawStats();
	void	CalculateStats();
private:
	FOverviewStats	Stats;
};