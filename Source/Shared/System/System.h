#pragma once

//#include <map>
#include <imgui.h>
#include <string>

#include "CodeAnalyser/CodeAnalyser.h"
#include "Util/Misc.h"

struct FGame;

struct FConfig
{
};

class FSystem : public ICPUInterface
{
public:
	FSystem() { }
	virtual ~FSystem() {}

	virtual bool Init(const FConfig* pConfig);
	virtual void Shutdown();

	virtual std::string GetAppTitle() = 0;
	virtual std::string GetWindowIconName() = 0;
	virtual std::string GetROMFilename() = 0;

	virtual void DrawMainMenu(double timeMS);
	virtual void DrawUI();
	virtual bool DrawDockingView();

#if SPECCY
	// put this in the base?
	
	// disable copy & assign because this class is big!
	FSystem(const FSpectrumEmu&) = delete;
	FSpectrumEmu& operator= (const FSpectrumEmu&) = delete;
#endif

	//ICPUInterface Begin
	virtual uint8_t		ReadByte(uint16_t address) const = 0;
	virtual uint16_t	ReadWord(uint16_t address) const = 0;
	virtual const uint8_t*	GetMemPtr(uint16_t address) const  = 0;
	virtual void		WriteByte(uint16_t address, uint8_t value) = 0;
	virtual uint16_t	GetPC(void) = 0;
	virtual uint16_t	GetSP(void) = 0;
	virtual bool		IsAddressBreakpointed(uint16_t addr) = 0;
	virtual bool		ToggleExecBreakpointAtAddress(uint16_t addr) = 0;
	virtual bool		ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize) = 0;
	virtual void		Break(void) = 0;
	virtual void		Continue(void) = 0;
	virtual void		StepOver(void) = 0;
	virtual void		StepInto(void) = 0;
	virtual void		StepFrame(void) = 0;
	virtual void		StepScreenWrite(void) = 0;
	virtual void		GraphicsViewerSetView(uint16_t address, int charWidth) = 0;
	virtual bool		ShouldExecThisFrame(void) const = 0;
	virtual bool		IsStopped(void) const = 0;
	virtual void*		GetCPUEmulator(void) = 0;
	//ICPUInterface End

	int				CurrentLayer = 0;	// layer ??

	unsigned char*	FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		Texture;		// texture 
	
	bool			ExecThisFrame = true; // Whether the emulator should execute this frame (controlled by UI)
	float			ExecSpeedScale = 1.0f;
	bool bShowImGuiDemo = false;
	bool bShowImPlotDemo = false;

protected:

	bool	bStepToNextFrame = false;
	bool	bStepToNextScreenWrite = false;

	bool	bShowDebugLog = false;
	bool	bInitialised = false;
};
