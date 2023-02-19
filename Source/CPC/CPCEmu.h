#pragma once

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

class FCPCEmu : public ICPUInterface
{
public:
	FCPCEmu()
	{
		CPUType = ECPUType::Z80;
	}

	bool	Init(/*const FSpectrumConfig& config*/);
	void	Shutdown();
	void	DrawMainMenu(double timeMS);

	int TrapFunction(uint16_t pc, int ticks, uint64_t pins);
	uint64_t Z80Tick(int num, uint64_t pins);

	void	Tick();
	void	DrawUI();
	bool	DrawDockingView();

	// disable copy & assign because this class is big!
	FCPCEmu(const FCPCEmu&) = delete;
	FCPCEmu& operator= (const FCPCEmu&) = delete;

	//ICPUInterface Begin
	uint8_t		ReadByte(uint16_t address) const override;
	uint16_t	ReadWord(uint16_t address) const override;
	const uint8_t*	GetMemPtr(uint16_t address) const override;
	void		WriteByte(uint16_t address, uint8_t value) override;
	uint16_t	GetPC(void) override;
	uint16_t	GetSP(void) override;
	bool		IsAddressBreakpointed(uint16_t addr) override;
	bool		ToggleExecBreakpointAtAddress(uint16_t addr) override;
	bool		ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize) override;
	void		Break(void) override;
	void		Continue(void) override;
	void		StepOver(void) override;
	void		StepInto(void) override;
	void		StepFrame(void) override;
	void		StepScreenWrite(void) override;
	void		GraphicsViewerSetView(uint16_t address, int charWidth) override;
	bool		ShouldExecThisFrame(void) const override;
	bool		IsStopped(void) const override;
	void*		GetCPUEmulator(void) override;
	//ICPUInterface End

	// Emulator 
	cpc_t			CPCEmuState;	// Chips CPC State
	int				CurrentLayer = 0;	// layer ??

	unsigned char*	FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		Texture;		// texture 
	
	bool			ExecThisFrame = true; // Whether the emulator should execute this frame (controlled by UI)
	float			ExecSpeedScale = 1.0f;

	ui_cpc_t		UICPC;

	FCodeAnalysisState		CodeAnalysis;

private:
	z80_tick_t	OldTickCB = nullptr;
	void*		OldTickUserData = nullptr;

	bool	bStepToNextFrame = false;
	bool	bStepToNextScreenWrite = false;
	bool	bInitialised = false;
};
