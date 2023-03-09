#pragma once

#include "SnapshotLoaders/GameLoader.h"

#define UI_DBG_USE_Z80
#define UI_DASM_USE_Z80
#include "chips/z80.h"
#include "chips/m6502.h" // ?
#include "chips/beeper.h"
#include "chips/ay38910.h"
#include "util/z80dasm.h"
#include "util/m6502dasm.h" // ?
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
#include "ui/ui_cpc.h"

#include <map>
#include <string>

#include "CodeAnalyser/CodeAnalyser.h"
#include "Viewers/CPCViewer.h"
#include "Viewers/GraphicsViewer.h"

enum class ECpcModel
{
	CPC_464,
	CPC_6128,
	CPC_KCKompact,
};

struct FCpcConfig
{
	ECpcModel Model;
	std::string	SpecificGame;
};

class FCpcEmu : public ICPUInterface
{
public:
	FCpcEmu()
	{
		CPUType = ECPUType::Z80;
	}

	bool	Init(const FCpcConfig& config);
	void	Shutdown();

	int TrapFunction(uint16_t pc, int ticks, uint64_t pins);
	uint64_t Z80Tick(int num, uint64_t pins);

	void	Tick();
	void	DrawUI();
	bool	DrawDockingView();

	void	DrawFileMenu();
	void	DrawSystemMenu();
	void	DrawHardwareMenu();
	void	DrawOptionsMenu();
	void	DrawToolsMenu();
	void	DrawWindowsMenu();
	void	DrawDebugMenu();
	void	DrawMenus();
	void	DrawMainMenu(double timeMS);
	void	ExportAsmGui();

	// disable copy & assign because this class is big!
	FCpcEmu(const FCpcEmu&) = delete;
	FCpcEmu& operator= (const FCpcEmu&) = delete;

	//ICPUInterface Begin
	uint8_t		ReadByte(uint16_t address) const override { /* todo */ return 0; }
	uint16_t	ReadWord(uint16_t address) const override { /* todo */ return 0; }
	const uint8_t* GetMemPtr(uint16_t address) const override;
	void		WriteByte(uint16_t address, uint8_t value) override { /* todo */ }
	uint16_t	GetPC(void) override { /* todo */ return 0; }
	uint16_t	GetSP(void) override { /* todo */ return 0; }
	bool		IsAddressBreakpointed(uint16_t addr) { /* todo */ return 0; }
	bool		ToggleExecBreakpointAtAddress(uint16_t addr) override { /* todo */ return 0; }
	bool		ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize) override { /* todo */ return 0; }
	void		Break(void) override { /* todo */ }
	void		Continue(void) override { /* todo */ }
	void		StepOver(void) override { /* todo */ }
	void		StepInto(void) override { /* todo */ }
	void		StepFrame(void) override { /* todo */ }
	void		StepScreenWrite(void) override { /* todo */ }
	void		GraphicsViewerSetView(uint16_t address, int charWidth) override { /* todo */ }
	bool		ShouldExecThisFrame(void) const override;
	bool		IsStopped(void) const override;
	void*		GetCPUEmulator(void) override { /* todo */ return 0; }
	//ICPUInterface End

	void SetROMBank(int bankNo);
	void SetRAMBank(int slot, int bankNo);

	// Emulator 
	cpc_t			CpcEmuState;		// Chips CPC State
	int				CurrentLayer = 0;	// layer ??

	bool			ExecThisFrame = true; // Whether the emulator should execute this frame (controlled by UI)
	float			ExecSpeedScale = 1.0f;

	ui_cpc_t		UICpc;

	// Viewers
	FCpcViewer				CpcViewer;
	FCodeAnalysisState		CodeAnalysis;
	FGraphicsViewerState	GraphicsViewer;

	// Code analysis pages - to cover 464 & 6128 
	static const int kNoBankPages = 16;	// no of pages per physical address slot (16k)
	static const int kNoSlotPages = 16;	// no of pages per physical address slot (16k)
	static const int kNoROMPages = 16 + 16;	// 48K ROM & 128K ROM
	static const int kNoRAMPages = 128;
	FCodeAnalysisPage	ROMPages[kNoROMPages];
	FCodeAnalysisPage	RAMPages[kNoRAMPages];
	int					ROMBank = -1;
	int					RAMBanks[4] = { -1,-1,-1,-1 };

	unsigned char* FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		Texture;		// texture 

	bool	bShowImGuiDemo = false;
	bool	bShowImPlotDemo = false;

private:
	FGamesList		GamesList;
	FCpcGameLoader	GameLoader;

	z80_tick_t	OldTickCB = nullptr;
	void*		OldTickUserData = nullptr;

	bool bExportAsm = false;

	bool	bStepToNextFrame = false;
	bool	bStepToNextScreenWrite = false;
	bool	bInitialised = false;
};
