#pragma once

//#include <map>
#include <imgui.h>
#include <string>

#include "CodeAnalyser/CodeAnalyser.h"
#include "Util/Misc.h"
#include "../GamesList/GamesList.h"

struct FGame;
class IGameLoader;

struct FSystemParams
{
	std::string AppTitle;
	std::string WindowIconName;
	size_t ScreenBufferSize = 0;
	int ScreenTextureWidth = 0;
	int ScreenTextureHeight = 0;
	IGameLoader* pGameLoader = 0;
};

class FSystem : public ICPUInterface
{
public:
	FSystem() { }
	virtual ~FSystem() {}

	bool Init(const FSystemParams& params);
	void Shutdown();

	virtual void Tick();
	virtual void DrawMainMenu(double timeMS);
	
	// override any of these to add your own menu items after (or before) the system ones.
	// will need to call the FSystem function in the overriden code obviously.
	virtual void DrawFileMenu();
	virtual void DrawSystemMenu();
	virtual void DrawHardwareMenu();
	virtual void DrawOptionsMenu();
	virtual void DrawToolsMenu();
	virtual void DrawWindowsMenu();
	virtual void DrawDebugMenu();
	
	virtual void DrawMenus();
	virtual void DrawUI();

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

	FGamesList		GamesList;

	int				CurrentLayer = 0;	// layer ??

	unsigned char*	FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		Texture;		// texture 
	
	bool			ExecThisFrame = true; // Whether the emulator should execute this frame (controlled by UI)
	float			ExecSpeedScale = 1.0f;
	bool			bShowImGuiDemo = false;
	bool			bShowImPlotDemo = false;

protected:
	bool DrawDockingView();
	void ExportAsmGui();
		 
	bool bStepToNextFrame = false;
	bool bStepToNextScreenWrite = false;
		 
	bool bExportAsm = false;
		 
	bool bShowDebugLog = false;
	bool bInitialised = false;
};
