#pragma once

#include "CodeAnalyser/CodeAnalyser.h"
#include "Misc/EmuBase.h"

//#define UI_DASM_USE_Z80
#define UI_DASM_USE_M6502
#define UI_DBG_USE_M6502
//#include "ui.h"
// Chips includes
#include <chips/chips_common.h>
#include <chips/m6502.h>
#include <chips/m6526.h>
#include <chips/m6569.h>
#include <chips/m6581.h>
#include <chips/kbd.h>
#include <chips/mem.h>
#include <chips/clk.h>
#include <systems/c1530.h>
#include <chips/m6522.h>
#include <systems/c1541.h>
#include <systems/c64.h>


#include <util/m6502dasm.h>
#include <util/z80dasm.h>

// Chips UI includes
/*
#include <ui/ui_util.h>
#include <ui/ui_chip.h>
#include <ui/ui_util.h>
#include <ui/ui_m6502.h>
#include <ui/ui_m6526.h>
#include <ui/ui_m6569.h>
#include <ui/ui_m6581.h>
#include <ui/ui_audio.h>
#include <ui/ui_dasm.h>
#include <ui/ui_dbg.h>
#include <ui/ui_memedit.h>
#include <ui/ui_memmap.h>
#include <ui/ui_kbd.h>
#include <ui/ui_snapshot.h>
#include <ui/ui_c64.h>
#include <ui/ui_ay38910.h>
*/
#include "C64GamesList.h"
#include "C64Display.h"

#include "IOAnalysis/C64IOAnalysis.h"
#include "GraphicsViewer/C64GraphicsViewer.h"

enum class EC64Event
{
	None = 0,

	VICRegisterWrite,
    VICScreenModeChar,
    VICScreenModeBmp,
    VICScreenModeMultiColour,
    VICScreenModeHiRes,
    
	SIDRegisterWrite,
	CIA1RegisterWrite,
	CIA1RegisterRead,
	CIA2RegisterWrite,
	CIA2RegisterRead,

};

enum class EC64FileType
{
	None,
	PRG,
	Tape,
	Disk,
	Cartridge
};

enum class EFileLoadPhase
{
	Idle,
	Reset,
	BasicReady,
	Loaded,
	Run,
	TapePlaying
};

struct FC64Config;
struct FC64ProjectConfig;
class FC64Emulator;

// Cartridges
enum class ECartridgeSlot
{
	Addr_8000,
	Addr_A000,
	Addr_E000,

	Unknown,
	Max = Unknown
};

// information on cartridge bank
struct FCartridgeBank
{
	//~FCartridgeBank() { delete[] Data; Data = nullptr; }

	int			BankNo = -1;
	int16_t		BankId = -1;
	uint16_t	Address = 0;
	uint32_t	DataSize = 0;
	uint8_t*	Data = nullptr;
};

struct FCartridgeSlot
{
	void Init(uint16_t baseAddress, uint16_t size)
	{
		BaseAddress = baseAddress;
		Size = size;
	}
	void Reset()
	{
		for(auto& bank : Banks)
			delete[] bank.Data;

		Banks.clear();
		CurrentBank = -1;
	}

	bool		bActive = false;
	uint16_t	BaseAddress = 0;	// where slot is mapped in address space
	uint16_t	Size = 0;			// size in bytes

	int			CurrentBank = -1;
	std::vector<FCartridgeBank>	Banks;
};

struct FC64LaunchConfig : public FEmulatorLaunchConfig
{
};

class FC64Emulator : public FEmuBase
{
public:
	FC64Emulator() = default;

	bool    Init(const FEmulatorLaunchConfig& launchConfig) override;
	void    Shutdown() override;
	void	DrawEmulatorUI() override;
	void    Tick() override;
	void    Reset() override;

	void	FileMenuAdditions(void) override;
	void	SystemMenuAdditions(void) override;
	void	OptionsMenuAdditions(void) override;
	void	WindowsMenuAdditions(void) override;

	// Begin IInputEventHandler interface implementation
	void	OnKeyUp(int keyCode);
	void	OnKeyDown(int keyCode);
	void	OnChar(int charCode);
	void    OnGamepadUpdated(int mask);
	// End IInputEventHandler interface implementation

	// Begin ICPUInterface interface implementation
	uint8_t		ReadByte(uint16_t address) const override
	{
		return mem_rd(const_cast<mem_t*>(&C64Emu.mem_cpu), address);
	}
	uint16_t	ReadWord(uint16_t address) const override
	{
		return mem_rd16(const_cast<mem_t*>(&C64Emu.mem_cpu), address);
	}
	const uint8_t* GetMemPtr(uint16_t address) const override
	{
		return mem_readptr(const_cast<mem_t*>(&C64Emu.mem_cpu), address);
	}

	void WriteByte(uint16_t address, uint8_t value) override
	{
		mem_wr(&C64Emu.mem_cpu, address, value);
	}

	FAddressRef GetPC() override
	{
		return CodeAnalysis.Debugger.GetPC();
	}

	uint16_t	GetSP(void) override
	{
		return m6502_s(&C64Emu.cpu) + 0x100;    // stack begins at 0x100
	}

	void* GetCPUEmulator(void) const override
	{
		return (void*)&C64Emu.cpu;
	}

	// End ICPUInterface interface implementation

	c64_desc_t GenerateC64Desc(c64_joystick_type_t joy_type);
	void SetupCodeAnalysisLabels(void);
	void UpdateCodeAnalysisPages(uint8_t cpuPort);
	FAddressRef	GetVICMemoryAddress(uint16_t vicAddress) const	// VIC address is 14bit (16K range)
	{
		const uint16_t physicalAddress = C64Emu.vic_bank_select + vicAddress;
		return FAddressRef(VICBankMapping[physicalAddress >> 12], physicalAddress);
	}
	bool IsAddressedByVIC(FAddressRef addr)
	{
		return VICBankMapping[addr.Address >> 12] == addr.BankId;
	}
	
	uint16_t GetIOAreaBankId() const { return IOAreaId; }

	FAddressRef	GetColourRAMAddress(uint16_t colRamAddress)	const // VIC address is 14bit (16K range)
	{
		return FAddressRef(IOAreaId, colRamAddress + 0xD800);
	}

	bool	LoadEmulatorFile(const FEmulatorFile* pEmuFile) override;
	bool	NewProjectFromEmulatorFile(const FEmulatorFile& emuFile) override;
	bool	LoadProject(FProjectConfig *pConfig, bool bLoadGame) override;
	bool	SaveProject(void) override;

	void	ResetCartridgeBanks();
	FCartridgeSlot&	GetCartridgeSlot(ECartridgeSlot slot) { assert(slot!=ECartridgeSlot::Unknown); return CartridgeSlots[(int)slot]; }
	FCartridgeBank&	AddCartridgeBank(int bankNo, uint16_t address, uint32_t dataSize);
	void	InitCartMapping(void);
	bool	MapCartridgeBank(ECartridgeSlot slot, int bankNo);
	void	UnMapCartridge(ECartridgeSlot slot);

	void ResetCodeAnalysis(void);
	bool LoadMachineState(const char* fname);
	bool SaveMachineState(const char* fname);

	// Emulator Event Handlers
	void    OnBoot(void);
	int     OnCPUTrap(uint16_t pc, int ticks, uint64_t pins);
	uint64_t    OnCPUTick(uint64_t pins);

	c64_t*	GetEmu() {return &C64Emu;}
	const FC64IOAnalysis&	GetC64IOAnalysis() { return IOAnalysis; }

	const FC64Config* GetC64GlobalConfig() const { return (const FC64Config*)pGlobalConfig; }
	FC64Config* GetC64GlobalConfig() { return (FC64Config*)pGlobalConfig; }

	void	SetLoadedFileType(EC64FileType type) { LoadedFileType = type;}
private:
	c64_t       C64Emu;
	double      ExecTime;

	EC64FileType	LoadedFileType = EC64FileType::None;
	EFileLoadPhase	FileLoadPhase = EFileLoadPhase::Idle;

	const FGameInfo*	CurrentGame = nullptr;

	FC64Display         Display;

	uint8_t				IOMemBuffer[0x1000];	// Buffer for IO memory

	uint8_t             LastMemPort = 0x7;  // Default startup
	uint16_t            PreviousPC = 0;

	//static const int	kMaxCartridgeBanks = 64;
	//int					ActiveCartridgeBanks = 0;
	//int					CurrentCartridgeBank = -1;
	//FCartridgeBank		CartridgeBanks[kMaxCartridgeBanks];

	FCartridgeSlot		CartridgeSlots[(int)ECartridgeSlot::Max];
	int16_t				FirstCartridgeBankId = -1;

	FC64IOAnalysis      IOAnalysis;
	std::set<FAddressRef>  InterruptHandlers;

	// Mapping status
	bool                bBasicROMMapped = true;
	bool                bKernelROMMapped = true;
	bool                bCharacterROMMapped = false;
	bool                bIOMapped = true;

	// Bank Ids
	uint16_t            LowerRAMId = -1;
	uint16_t			LowerRAM2Id = -1;
	uint16_t            HighRAMId = -1;
	uint16_t            IOAreaId = -1;
	uint16_t            BasicROMId = -1;
	uint16_t            RAMBehindBasicROMId = -1;
	uint16_t            KernelROMId = -1;
	uint16_t            RAMBehindKernelROMId = -1;
	uint16_t            CharacterROMId = -1;
	uint16_t            RAMBehindCharROMId = -1;

	uint16_t			VICBankMapping[16];

	FC64Emulator(const FC64Emulator&) = delete;                 // Prevent copy-construction
	FC64Emulator& operator=(const FC64Emulator&) = delete;      // Prevent assignment

};
