#pragma once

#include "SnapshotLoaders/GameLoader.h"

#define UI_DBG_USE_Z80
#define UI_DASM_USE_Z80

#pragma once

#include "chips/chips_common.h"
#include "chips/z80.h"
#include "chips/ay38910.h"
#include "util/z80dasm.h"
#include "chips/mem.h"
#include "chips/kbd.h"
#include "chips/clk.h"

#include "chips/i8255.h"
#include "chips/mc6845.h"
#include "chips/am40010.h"
#include "chips/upd765.h"
#include "chips/fdd.h"
#include "chips/fdd_cpc.h"
#include "systems/cpc.h"
#include "chips/mem.h"

#include "ui/ui_util.h"
#include "ui/ui_fdd.h"
#include "ui/ui_chip.h"
#include "ui/ui_z80.h"
#include "ui/ui_ay38910.h"
#include "ui/ui_audio.h"
#include "ui/ui_kbd.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_memedit.h"
#include "ui/ui_memmap.h"
#include "ui/ui_mc6845.h"
#include "ui/ui_i8255.h"
#include "ui/ui_upd765.h"
#include "ui/ui_am40010.h"
#include "ui/ui_snapshot.h"
#include "ui/ui_cpc.h"

#include <map>
#include <string>

#include "CPCScreen.h"
#include "CodeAnalyser/CodeAnalyser.h"
#include "Viewers/CPCViewer.h"
#include "Viewers/CPCGraphicsViewer.h"
#include "MemoryHandlers.h"
#include "IOAnalysis.h"
#include "Misc/EmuBase.h"
#include "ExternalROMSupport.h"

struct FGameViewerData;
struct FGameConfig;
struct FViewerConfig;
class FViewerBase;
class FScreenPixMemDescGenerator;
struct FCPCConfig;
struct FCPCGameConfig;

enum class ECPCModel
{
	CPC_464,
	CPC_6128,
};

// Built in ROM banks
enum EROMBank
{
	OS = 0,
	AMSDOS,
	BASIC,

	COUNT,
	NONE,
	INVALID,
};

struct FCPCLaunchConfig : public FEmulatorLaunchConfig
{
	void ParseCommandline(int argc, char** argv) override;

	// moved this to the global config
	//ECPCModel Model = ECPCModel::CPC_464;
};

struct FGame
{
	FGameConfig* pConfig = nullptr;
	FViewerConfig* pViewerConfig = nullptr;
	FGameViewerData* pViewerData = nullptr;
};

class FCPCEmu : public FEmuBase
{
public:
	FCPCEmu()
	{
		CPUType = ECPUType::Z80;
	}

	// FEmuBase Begin
	bool				Init(const FEmulatorLaunchConfig& config) override;
	void				Shutdown() override;
	bool				NewGameFromSnapshot(const FGameSnapshot& snaphot) override;
	bool				StartGame(FGameConfig* pGameConfig, bool bLoadGameData) override;
	bool				SaveCurrentGameData() override;
	void				Reset() override;
	void				Tick() override;
	bool				LoadLua() override;
	void				DrawEmulatorUI(void) override;
	// ~FEmuBase End

	bool				SaveGameState(const char* fname);
	bool				LoadGameState(const char* fname);

	void				OnInstructionExecuted(int ticks, uint64_t pins);
	uint64_t		Z80Tick(int num, uint64_t pins);

	// FEmuBase Begin
	void				FileMenuAdditions(void) override;		
	void				SystemMenuAdditions(void) override;
	void				OptionsMenuAdditions(void) override;
	void				WindowsMenuAdditions(void) override;
	// ~FEmuBase End

	// disable copy & assign because this class is big!
	FCPCEmu(const FCPCEmu&) = delete;
	FCPCEmu& operator= (const FCPCEmu&) = delete;

	//ICPUInterface Begin
	uint8_t				ReadByte(uint16_t address) const override;
	uint16_t				ReadWord(uint16_t address) const override;
	const uint8_t*		GetMemPtr(uint16_t address) const override;
	void					WriteByte(uint16_t address, uint8_t value) override;
	FAddressRef			GetPC(void) override;
	uint16_t				GetSP(void) override;
	void*					GetCPUEmulator(void) const override;
	//ICPUInterface End
	
	bool				InitForModel(ECPCModel model);
	bool				CanSelectUpperROM(uint8_t romSlot);
	bool				InitBankMappings();
	void				UpdateBankMappings();
	ECPCModel		GetCurrentCPCModel() const { return CPCEmuState.type == CPC_TYPE_6128 ? ECPCModel::CPC_6128 : ECPCModel::CPC_464; }
	void				SetRAMBank(int slot, int bankNo, EBankAccess access);

	void				UpdatePalette();
	void				OnScreenRAMAddressChanged();
	void				FixupAddressRefs();

	const FCPCConfig* GetCPCGlobalConfig() const { return (const FCPCConfig*)pGlobalConfig; }
	FCPCConfig* GetCPCGlobalConfig() { return (FCPCConfig*)pGlobalConfig; }

	// Emulator 
	cpc_t				CPCEmuState;		// Chips CPC State
	uint8_t*		MappedInMemory = nullptr;

	float				ExecSpeedScale = 1.0f;

	ui_cpc_t		UICPC;

	FGame*			pActiveGame = nullptr;

	// Viewers
	FCPCViewer	CPCViewer;
	
	// todo: refactor this to move all event related code out of it
	FIOAnalysis				IOAnalysis;

	static const int	kNoBankPages = 16;	// no of pages per physical address slot (16k)
	static const int	kNoRAMPages = 128;
	static const int	kNoROMBanks = EROMBank::COUNT;
	static const int	kNoRAMBanks = 8;

	// Banks IDs of all ROM/RAM banks that have been created
	int16_t			ROMBanks[kNoROMBanks];
	int16_t			RAMBanks[kNoRAMBanks];

	// Bank IDs of upper ROM slots
	int16_t			UpperROMSlot[kNumUpperROMSlots];
	int				CurUpperROMSlot = -1;

	int16_t			CurRAMBank[4] = { -1,-1,-1,-1 };

	// Temp variables so we can tell when Chips registers are dirty
	uint8_t			LastGateArrayRAMConfig = 0;
	uint8_t			LastGateArrayConfig = 0;

	// Memory handling
	std::string	SelectedMemoryHandler;
	std::vector< FMemoryAccessHandler>	MemoryAccessHandlers;

	FMemoryStats	MemStats;

	uint16_t		PreviousPC = 0;		// store previous pc
	int			InstructionsTicks = 0;

	FCPCScreen	Screen;

	const FGamesList& GetGamesList() const { return GamesList; }

private:
	FCPCGameLoader	GameLoader;

	FScreenPixMemDescGenerator* pScreenMemDescGenerator = 0;

	bool				bReplaceGamePopup = false;
	int				ReplaceGameSnapshotIndex = 0;

	bool				bExportAsm = false;

	bool				bInitialised = false;
	
	// temporarily disabled
	bool				bExternalROMSupport = false;
};
