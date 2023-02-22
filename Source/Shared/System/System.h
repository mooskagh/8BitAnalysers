#pragma once

//#include <map>
#include <imgui.h>
#include <string>

#include "CodeAnalyser/CodeAnalyser.h"
#if SPECCY
#include "Viewers/SpriteViewer.h"
#include "MemoryHandlers.h"
#include "Viewers/ViewerBase.h"
#include "Viewers/GraphicsViewer.h"
#include "Viewers/FrameTraceViewer.h"
#include "SnapshotLoaders/GamesList.h"
#include "IOAnalysis.h"
#endif
#include "Util/Misc.h"

struct FGame;
#if SPECCY
struct FGameViewer;
struct FGameViewerData;
struct FGameConfig;
struct FViewerConfig;
#endif

struct FConfig
{
};

#if SPECCY
struct FSpectrumConfig : public FConfig
{
	ESpectrumModel	Model;
	int				NoStateBuffers = 0;
	std::string		SpecificGame;
};

struct FGame
{
	FGameConfig *		pConfig	= nullptr;
	FViewerConfig *		pViewerConfig = nullptr;
	FGameViewerData *	pViewerData = nullptr;
};
#endif


class FSystem : public ICPUInterface
{
public:
	FSystem()
	{
		//CPUType = ECPUType::Z80;
	}
	virtual ~FSystem() {}

	virtual bool Init(const FConfig* pConfig);
	virtual void Shutdown();
	virtual std::string GetAppTitle() = 0;
	virtual std::string GetWindowIconName() = 0;

#if SPECCY
	void	StartGame(FGameConfig* pGameConfig);
	bool	StartGame(const char* pGameName);
	void	SaveCurrentGameData();
#endif
	virtual void DrawMainMenu(double timeMS);
#if SPECCY
	void	DrawCheatsUI();

	int		TrapFunction(uint16_t pc, int ticks, uint64_t pins);
	uint64_t Z80Tick(int num, uint64_t pins);
#endif

#if SPECCY
	void	Tick();
	void	DrawMemoryTools();
#endif
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

#if SPECCY

	void SetROMBank(int bankNo);
	void SetRAMBank(int slot, int bankNo);

	void AddMemoryHandler(const FMemoryAccessHandler& handler)
	{
		MemoryAccessHandlers.push_back(handler);
	}

	void GraphicsViewerGoToAddress(uint16_t address)
	{
		GraphicsViewer.Address = address;
	}

	void GraphicsViewerSetCharWidth(uint16_t width)
	{
		GraphicsViewer.XSize = width;
	}
	// TODO: Make private
#endif
	int				CurrentLayer = 0;	// layer ??

	unsigned char*	FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		Texture;		// texture 
	
	bool			ExecThisFrame = true; // Whether the emulator should execute this frame (controlled by UI)
	float			ExecSpeedScale = 1.0f;

#if SPECCY
	FGame *			pActiveGame = nullptr;

	FGamesList		GamesList;

	//Viewers
	FFrameTraceViewer		FrameTraceViewer;
	FGraphicsViewerState	GraphicsViewer;
	FCodeAnalysisState		CodeAnalysis;
	FIOAnalysis				IOAnalysis;

	// Code analysis pages - to cover 48K & 128K Spectrums
	static const int kNoBankPages = 16;	// no of pages per physical address slot (16k)
	static const int kNoSlotPages = 16;	// no of pages per physical address slot (16k)
	static const int kNoROMPages = 16 + 16;	// 48K ROM & 128K ROM
	static const int kNoRAMPages = 128;
	FCodeAnalysisPage	ROMPages[kNoROMPages];
	FCodeAnalysisPage	RAMPages[kNoRAMPages];
	int					ROMBank = -1;
	int					RAMBanks[4] = { -1,-1,-1,-1 };
	// Memory handling
	std::string				SelectedMemoryHandler;
	std::vector< FMemoryAccessHandler>	MemoryAccessHandlers;
	std::vector< FMemoryAccess>	FrameScreenPixWrites;
	std::vector< FMemoryAccess>	FrameScreenAttrWrites;

	FMemoryStats	MemStats;

	// interrupt handling info
	bool		bHasInterruptHandler = false;
	uint16_t	InterruptHandlerAddress = 0;

	uint16_t dasmCurr = 0;

	static const int kPCHistorySize = 32;
	uint16_t PCHistory[kPCHistorySize];
	int PCHistoryPos = 0;
#endif

	bool bShowImGuiDemo = false;
	bool bShowImPlotDemo = false;

protected:

#if SPECCY
	z80_tick_t	OldTickCB = nullptr;
	void*		OldTickUserData = nullptr;
	std::vector<FViewerBase*>	Viewers;
#endif

	bool	bStepToNextFrame = false;
	bool	bStepToNextScreenWrite = false;

	bool	bShowDebugLog = false;
	bool	bInitialised = false;
};

#if SPECCY
uint16_t GetScreenPixMemoryAddress(int x, int y);
uint16_t GetScreenAttrMemoryAddress(int x, int y);
bool GetScreenAddressCoords(uint16_t addr, int& x, int& y);
bool GetAttribAddressCoords(uint16_t addr, int& x, int& y);
#endif