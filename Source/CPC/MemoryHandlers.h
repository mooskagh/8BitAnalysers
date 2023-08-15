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
	ESearchMemoryType MemoryType = ESearchMemoryType::CodeAndData; // What type of memory locations to search?
	bool bSearchUnreferenced = true; // Search locations with no references
	bool bSearchUnaccessed = true;	 // Search locations that have not been written or read.
	bool bSearchGraphicsMem = false; // Include graphics memory in the search?
	bool bSearchAllBanks = false;	 // If true, will search the all memory banks, including ones paged out. If false, will search only physical memory.
	bool bSearchROM = false;		 // Include ROM in the search?
};

class FFinder
{
public:
	void Init(FCpcEmu* pEmu) // pass in codeanalysis state here
	{
		pCpcEmu = pEmu;
	}
	virtual void Find(const FSearchOptions& opt);
	virtual void Reset();
	virtual void FindInAllBanks(const FSearchOptions& opt);
	virtual void FindInPhysicalMemory(const FSearchOptions& opt);
	virtual bool FindNextMatchInPhysicalMemory(uint16_t offset, uint16_t& outAddr) = 0;
	virtual void ProcessMatch(FAddressRef addr, const FSearchOptions& opt);

	virtual bool HasValueChanged(FAddressRef addr) const; // convert to addressref
	virtual const char* GetValueString(FAddressRef addr, ENumberDisplayMode numberMode) const = 0;
	virtual void RemoveUnchangedResults() {}

	size_t GetNumResults() const { return SearchResults.size(); }
	FAddressRef GetResult(size_t index) const { return SearchResults[index]; }

protected:
	virtual std::vector<FAddressRef> FindAllMatchesInBanks(const FSearchOptions& opt) = 0;

protected:
	std::vector<FAddressRef> SearchResults;

protected:	
	FCpcEmu* pCpcEmu = nullptr;
};

class FDataFinder : public FFinder
{
};

class FByteFinder : public FDataFinder
{
public:
	virtual bool FindNextMatchInPhysicalMemory(uint16_t offset, uint16_t& outAddr) override;
	virtual bool HasValueChanged(FAddressRef addr) const override;
	virtual void Find(const FSearchOptions& opt) override
	{
		LastValue = SearchValue;
		FFinder::Find(opt);
	}
	virtual std::vector<FAddressRef> FindAllMatchesInBanks(const FSearchOptions& opt) override;
	virtual const char* GetValueString(FAddressRef addr, ENumberDisplayMode numberMode) const override;
	virtual void RemoveUnchangedResults() override;
	uint8_t SearchValue = 0;
	uint8_t LastValue = 0;
};

class FWordFinder : public FDataFinder
{
public:
	virtual bool FindNextMatchInPhysicalMemory(uint16_t offset, uint16_t& outAddr) override;
	virtual bool HasValueChanged(FAddressRef addr) const override;
	virtual void Find(const FSearchOptions& opt) override
	{
		LastValue = SearchValue;
		SearchBytes[1] = static_cast<uint8_t>(SearchValue >> 8);
		SearchBytes[0] = static_cast<uint8_t>(SearchValue);
		FFinder::Find(opt);
	}
	virtual std::vector<FAddressRef> FindAllMatchesInBanks(const FSearchOptions& opt) override;
	virtual const char* GetValueString(FAddressRef addr, ENumberDisplayMode numberMode) const override;
	virtual void RemoveUnchangedResults() override;
	uint16_t SearchValue = 0;
	uint8_t SearchBytes[2];
	uint16_t LastValue = 0;
};

class FTextFinder : public FFinder
{
public:
	virtual bool FindNextMatchInPhysicalMemory(uint16_t offset, uint16_t& outAddr) override;
	virtual std::vector<FAddressRef> FindAllMatchesInBanks(const FSearchOptions& opt) override;
	virtual const char* GetValueString(FAddressRef addr, ENumberDisplayMode numberMode) const override { return ""; }
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
	void Init(FCpcEmu* pEmu); // pass in codeanalysisstate.
	void DrawUI();
	void Reset();

	// need getphysicalbyte() getphysicalword() function here to read ram (not rom) that can be overloaded?
	//virtual GetByte
private:
	FSearchOptions Options;
	ESearchType SearchType = ESearchType::Value;
	ESearchDataType DataSize = ESearchDataType::Byte;

	bool bDecimal = true;

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
