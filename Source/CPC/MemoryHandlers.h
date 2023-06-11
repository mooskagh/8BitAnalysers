#pragma once

#include <CodeAnalyser/CodeAnalysisPage.h> 

#include <cstdint>
#include <vector>
#include <string>


class FCpcEmu;
struct FGame;

enum class MemoryUse
{
	Code,
	Data,
	Unknown	// unknown/unused - never read from or written to
};

struct FMemoryBlock
{
	MemoryUse	Use;
	uint16_t	StartAddress;
	uint16_t	EndAddress;
	bool		ContainsSelfModifyingCode = false;
};

struct FMemoryStats
{
	// counters for each memory address
	int		ExecCount[0x10000];
	int		ReadCount[0x10000];
	int		WriteCount[0x10000];

	std::vector< FMemoryBlock>	MemoryBlockInfo;

	std::vector<uint16_t>	CodeAndDataList;
};

enum class MemoryAccessType
{
	Read,
	Write,
	Execute
};

struct FMemoryAccessHandler
{
	// configuration
	std::string			Name;
	bool				bEnabled = true;
	bool				bBreak = false;
	MemoryAccessType	Type;
	uint16_t			MemStart;
	uint16_t			MemEnd;

	void(*pHandlerFunction)(FMemoryAccessHandler &handler, FGame* pGame, uint16_t pc, uint64_t pins) = nullptr;

	// stats
	int						TotalCount = 0;
	FItemReferenceTracker	Callers;
};

class FDiffTool
{
public:
	void Init(FCpcEmu* pEmu);
	void DrawUI();

private:
	bool bDiffVideoMem = false;
	bool bSnapshotAvailable = false;

	uint8_t DiffSnapShotMemory[1 << 16];	// 64 Kb
	uint16_t SnapShotGfxMemStart = 0xffff;
	uint16_t SnapShotGfxMemEnd = 0xffff;
	uint16_t DiffStartAddr = 0;
	uint16_t DiffEndAddr = 0xffff;

	std::vector<uint16_t> DiffChangedLocations;
	int DiffSelectedAddr = -1;
	
	std::string CommentText = "byte was modified";
	bool bReplaceExistingComment = false;

	FCpcEmu* pCpcEmu = nullptr;
};

int MemoryHandlerTrapFunction(uint16_t pc, int ticks, uint64_t pins, FCpcEmu* pEmu);

void AnalyseMemory(FMemoryStats &memStats);
void ResetMemoryStats(FMemoryStats &memStats);

// UI
void DrawMemoryAnalysis(FCpcEmu* pEmu);
void DrawMemoryHandlers(FCpcEmu* pEmu);
