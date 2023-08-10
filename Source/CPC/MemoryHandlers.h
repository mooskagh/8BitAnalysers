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

enum ESearchMemoryType
{
	Data = 0,		// search data only
	Code,			// search code only
	CodeAndData,	// search code and data
};

struct FSearchOptions
{
	ESearchMemoryType memoryType = ESearchMemoryType::CodeAndData;
	bool bSearchUnreferenced = true;
	bool bSearchUnaccessed = true;
};

class FFinder
{
public:
	void Init(FCpcEmu* pEmu)
	{
		pCpcEmu = pEmu;
	}
	virtual void Reset();
	virtual bool FindNextMatch(uint16_t offset, uint16_t& outAddr) = 0;
	virtual bool HasValueChanged(uint16_t addr) const;
	virtual void Find(const FSearchOptions& opt);
	virtual const char* GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const = 0;
	virtual void RemoveUnchangedResults() {}

	std::vector<uint16_t> SearchResults;

protected:	
	FCpcEmu* pCpcEmu = nullptr;
};

class FDataFinder : public FFinder
{
};

class FByteFinder : public FDataFinder
{
public:
	virtual bool FindNextMatch(uint16_t offset, uint16_t& outAddr) override;
	virtual bool HasValueChanged(uint16_t addr) const override;
	virtual void Find(const FSearchOptions& opt) override
	{
		LastValue = SearchValue;
		FFinder::Find(opt);
	}
	virtual const char* GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const override;
	virtual void RemoveUnchangedResults() override;
	uint8_t SearchValue = 0;
	uint8_t LastValue = 0;
};

class FWordFinder : public FDataFinder
{
public:
	virtual bool FindNextMatch(uint16_t offset, uint16_t& outAddr) override;
	virtual bool HasValueChanged(uint16_t addr) const override;
	virtual void Find(const FSearchOptions& opt) override
	{
		LastValue = SearchValue;
		FFinder::Find(opt);
	}
	virtual const char* GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const override;
	virtual void RemoveUnchangedResults() override;
	uint16_t SearchValue = 0;
	uint16_t LastValue = 0;
};

class FTextFinder : public FFinder
{
public:
	virtual bool FindNextMatch(uint16_t offset, uint16_t& outAddr) override;
	virtual const char* GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const override { return ""; }
	std::string SearchText;
};

enum ESearchType
{
	Value,
	Text,
};

enum ESearchDataType
{
	Byte = 0,
	Word,
};


class FDataFindTool
{
public:
	void Init(FCpcEmu* pEmu);
	void DrawUI();
	void Reset();

private:
	ESearchType SearchType = ESearchType::Value;
	ESearchDataType DataSize = ESearchDataType::Byte;
	ESearchMemoryType MemoryType = ESearchMemoryType::CodeAndData;
	
	bool bDecimal = true;

	bool bSearchCode = false;
	bool bSearchUnreferenced = true;
	bool bSearchUnaccessed = true;

	FFinder* pCurFinder = nullptr;
	
	FByteFinder ByteFinder;
	FWordFinder WordFinder;
	FTextFinder TextFinder;

	FCpcEmu* pCpcEmu = nullptr;
};

int MemoryHandlerTrapFunction(uint16_t pc, int ticks, uint64_t pins, FCpcEmu* pEmu);

void AnalyseMemory(FMemoryStats &memStats);
void ResetMemoryStats(FMemoryStats &memStats);

// UI
void DrawMemoryAnalysis(FCpcEmu* pEmu);
void DrawMemoryHandlers(FCpcEmu* pEmu);
