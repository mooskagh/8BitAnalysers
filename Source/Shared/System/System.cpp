#include "System.h"

#include <cstdint>
//#include <imgui.h>

//#include "ZXSpectrum/App.h"

#if SPECCY
typedef void (*z80dasm_output_t)(char c, void* user_data);

void DasmOutputU8(uint8_t val, z80dasm_output_t out_cb, void* user_data);
void DasmOutputU16(uint16_t val, z80dasm_output_t out_cb, void* user_data);
void DasmOutputD8(int8_t val, z80dasm_output_t out_cb, void* user_data);


#define _STR_U8(u8) DasmOutputU8((uint8_t)(u8),out_cb,user_data);
#define _STR_U16(u16) DasmOutputU16((uint16_t)(u16),out_cb,user_data);
#define _STR_D8(d8) DasmOutputD8((int8_t)(d8),out_cb,user_data);


#include "GlobalConfig.h" // ?
#include "GameData.h"
#include <ImGuiSupport/ImGuiTexture.h>
#include "GameViewers/GameViewer.h"
#include "GameViewers/MiscGameViewers.h"
#include "Viewers/GraphicsViewer.h"
#include "Viewers/BreakpointViewer.h"
#include "Viewers/OverviewViewer.h"
#include "Util/FileUtil.h"

#include "MemoryHandlers.h"
#include "CodeAnalyser/CodeAnalyser.h"
#include "CodeAnalyser/UI/CodeAnalyserUI.h"

#include <algorithm> // ?
#include "Debug/DebugLog.h"
#include "Debug/ImGuiLog.h"
#include <cassert>
#include <Util/Misc.h>

#include "Exporters/AssemblerExport.h" // ?
#include "Exporters/JsonExport.h" // ?
#include "CodeAnalyser/UI/CharacterMapViewer.h"
#include "GameConfig.h"


#define SAVE_ROM_JSON 0 // ?

#define ENABLE_CAPTURES 0 // ?

const int kCaptureTrapId = 0xffff;


const char* kGlobalConfigFilename = "GlobalConfig.json";
const char* kRomInfo48JsonFile = "RomInfo.json";

const char* kRomInfo128JsonFile = "RomInfo128.json";

const std::string kAppTitle = "Spectrum Analyser"; // get from System?


void FSpectrumEmu::GraphicsViewerSetView(uint16_t address, int charWidth)
{
	GraphicsViewerGoToAddress(address);
	GraphicsViewerSetCharWidth(charWidth);
}

bool	FSpectrumEmu::ShouldExecThisFrame(void) const
{
	return ExecThisFrame;
}

bool FSpectrumEmu::IsStopped(void) const
{
	return UIZX.dbg.dbg.stopped;
}

void CheckAddressSpaceItems(const FCodeAnalysisState& state)
{
	for (int addr = 0; addr < 1 << 16; addr++)
	{
		const FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(addr);
		assert(pDataInfo->Address == addr);
	}
}
#endif


bool FSystem::Init(const FConfig* pConfig)
{
	// need to move app.h to shared
	//SetWindowTitle(GetAppTitle().c_str());
	//SetWindowIcon(GetWindowIconName().c_str());

#if SPECCY
	// Initialise Emulator
	LoadGlobalConfig(kGlobalConfigFilename);
	FGlobalConfig& globalConfig = GetGlobalConfig();
	SetNumberDisplayMode(globalConfig.NumberDisplayMode);
	CodeAnalysis.Config.bShowOpcodeValues = globalConfig.bShowOpcodeValues;

	// init emulator

	GamesList.Init(this);
	GamesList.EnumerateGames(globalConfig.SnapshotFolder.c_str());

	// init emulator ui

	// This is where we add the viewers we want
	Viewers.push_back(new FBreakpointViewer(this));
	Viewers.push_back(new FOverviewViewer(this));

	// Initialise Viewers
	for (auto Viewer : Viewers)
	{
		if (Viewer->Init() == false)
		{
			// TODO: report error
		}
	}

	GraphicsViewer.pEmu = this;
	InitGraphicsViewer(GraphicsViewer);
	IOAnalysis.Init(this);

	SpectrumViewer.Init(this); // rename to platformviewer?

	FrameTraceViewer.Init(this);

	CodeAnalysis.ViewState[0].Enabled = true;	// always have first view enabled

	// Setup memory description handlers
	AddMemoryRegionDescGenerator(new FScreenPixMemDescGenerator());
	AddMemoryRegionDescGenerator(new FScreenAttrMemDescGenerator());	

	// register Viewers
	RegisterStarquakeViewer(this);
	RegisterGames(this);

	LoadGameConfigs(this);

	// Set up code analysis
	// initialise code analysis pages
	
	// ROM
	for (int pageNo = 0; pageNo < kNoROMPages; pageNo++)
	{
		char pageName[32];
		sprintf(pageName, "ROM:%d", pageNo);
		ROMPages[pageNo].Initialise(0);
		CodeAnalysis.RegisterPage(&ROMPages[pageNo], pageName);
	}
	// RAM
	for (int pageNo = 0; pageNo < kNoRAMPages; pageNo++)
	{
		char pageName[32];
		sprintf(pageName, "RAM:%d", pageNo);
		RAMPages[pageNo].Initialise(0);
		CodeAnalysis.RegisterPage(&RAMPages[pageNo], pageName);
	}

	// Setup initial machine memory config
	if (config.Model == ESpectrumModel::Spectrum48K)
	{
		SetROMBank(0);
		SetRAMBank(1, 0);	// 0x4000 - 0x7fff
		SetRAMBank(2, 1);	// 0x8000 - 0xBfff
		SetRAMBank(3, 2);	// 0xc000 - 0xffff
	}
	else
	{
		SetROMBank(0);
		SetRAMBank(1, 5);	// 0x4000 - 0x7fff
		SetRAMBank(2, 2);	// 0x8000 - 0xBfff
		SetRAMBank(3, 0);	// 0xc000 - 0xffff
	}

#ifdef _DEBUG
	CheckAddressSpaceItems(CodeAnalysis);
#endif
	// load the command line game if none specified then load the last game
	bool bLoadedGame = false;

	if (config.SpecificGame.empty() == false)
	{
		bLoadedGame = StartGame(config.SpecificGame.c_str());
	}
	else if (globalConfig.LastGame.empty() == false)
	{
		bLoadedGame = StartGame(globalConfig.LastGame.c_str());
	}
	
	// Start ROM if no game has been loaded
	if(bLoadedGame == false)
	{
		// start rom here

		std::string romJsonFName = kRomInfo48JsonFile;

		if (config.Model == ESpectrumModel::Spectrum128K)
			romJsonFName = kRomInfo128JsonFile;

		InitialiseCodeAnalysis(CodeAnalysis, this);

		if (FileExists(romJsonFName.c_str()))
			ImportAnalysisJson(CodeAnalysis, romJsonFName.c_str());
	}
#endif
	bInitialised = true;
	return true;
}

void FSystem::Shutdown()
{
#if SPECCY
	SaveCurrentGameData();	// save on close

	// Save Global Config - move to function?
	FGlobalConfig& config = GetGlobalConfig();

	if (pActiveGame != nullptr)
		config.LastGame = pActiveGame->pConfig->Name;

	config.NumberDisplayMode = GetNumberDisplayMode();
	config.bShowOpcodeValues = CodeAnalysis.Config.bShowOpcodeValues;

	SaveGlobalConfig(kGlobalConfigFilename);
#endif
}


#if SPECCY
void FSpectrumEmu::StartGame(FGameConfig *pGameConfig)
{
	MemoryAccessHandlers.clear();	// remove old memory handlers

	ResetMemoryStats(MemStats);

	const std::string windowTitle = kAppTitle + " - " + pGameConfig->Name;
	SetWindowTitle(windowTitle.c_str());
	
	// Reset Functions
	//FunctionStack.clear();
	//Functions.clear();

	// start up game
	if(pActiveGame!=nullptr)
		delete pActiveGame->pViewerData;
	delete pActiveGame;
	
	FGame *pNewGame = new FGame;
	pNewGame->pConfig = pGameConfig;
	pNewGame->pViewerConfig = pGameConfig->pViewerConfig;
	assert(pGameConfig->pViewerConfig != nullptr);
	GraphicsViewer.pGame = pNewGame;
	pActiveGame = pNewGame;
	pNewGame->pViewerData = pNewGame->pViewerConfig->pInitFunction(this, pGameConfig);
	GenerateSpriteListsFromConfig(GraphicsViewer, pGameConfig);

	// Initialise code analysis
	InitialiseCodeAnalysis(CodeAnalysis, this);

	// Set options from config
	for (int i = 0; i < FCodeAnalysisState::kNoViewStates; i++)
	{
		CodeAnalysis.ViewState[i].Enabled = pGameConfig->ViewConfigs[i].bEnabled;
		CodeAnalysis.ViewState[i].GoToAddress = pGameConfig->ViewConfigs[i].ViewAddress;
	}

	const std::string root = GetGlobalConfig().WorkspaceRoot;
	const std::string dataFName = root + "GameData/" + pGameConfig->Name + ".bin";
	std::string romJsonFName = kRomInfo48JsonFile;

	if (ZXEmuState.type == ZX_TYPE_128)
		romJsonFName = root + kRomInfo128JsonFile;

	const std::string analysisJsonFName = root + "AnalysisJson/" + pGameConfig->Name + ".json";
	const std::string saveStateFName = root + "SaveStates/" + pGameConfig->Name + ".state";
	if (FileExists(analysisJsonFName.c_str()))
		ImportAnalysisJson(CodeAnalysis, analysisJsonFName.c_str());
	else
		LoadGameData(this, dataFName.c_str());	// Load the old one - this needs to go in time

	LoadGameState(this, saveStateFName.c_str());

	if (FileExists(romJsonFName.c_str()))
		ImportAnalysisJson(CodeAnalysis, romJsonFName.c_str());

	// where do we want pokes to live?
	LoadPOKFile(*pGameConfig, std::string(GetGlobalConfig().PokesFolder + pGameConfig->Name + ".pok").c_str());
	ReAnalyseCode(CodeAnalysis);
	GenerateGlobalInfo(CodeAnalysis);
	FormatSpectrumMemory(CodeAnalysis);
	CodeAnalysis.SetCodeAnalysisDirty();

	// Start in break mode so the memory will be in it's initial state. 
	// Otherwise, if we export a skool/asm file once the game is running the memory could be in an arbitrary state.
	// 
	// decode whole screen
	const int oldScanlineVal = ZXEmuState.scanline_y;
	ZXEmuState.scanline_y = 0;
	for (int i = 0; i < ZXEmuState.frame_scan_lines; i++)
	{
		_zx_decode_scanline(&ZXEmuState);
	}
	ZXEmuState.scanline_y = oldScanlineVal;
	ImGui_UpdateTextureRGBA(Texture, FrameBuffer);

	Break();
}

bool FSpectrumEmu::StartGame(const char *pGameName)
{
	for (const auto& pGameConfig : GetGameConfigs())
	{
		if (pGameConfig->Name == pGameName)
		{
			const std::string snapFolder = GetGlobalConfig().SnapshotFolder;
			const std::string gameFile = snapFolder + pGameConfig->SnapshotFile;
			if (GamesList.LoadGame(gameFile.c_str()))
			{
				StartGame(pGameConfig);
				return true;
			}
		}
	}

	return false;
}

// save config & data
void FSpectrumEmu::SaveCurrentGameData()
{
	if (pActiveGame != nullptr)
	{
		FGameConfig *pGameConfig = pActiveGame->pConfig;
		if (pGameConfig->Name.empty())
		{
			
		}
		else
		{
			const std::string root = GetGlobalConfig().WorkspaceRoot;
			const std::string configFName = root + "Configs/" + pGameConfig->Name + ".json";
			const std::string dataFName = root + "GameData/" + pGameConfig->Name + ".bin";
			const std::string analysisJsonFName = root + "AnalysisJson/" + pGameConfig->Name + ".json";
			const std::string saveStateFName = root + "SaveStates/" + pGameConfig->Name + ".state";
			EnsureDirectoryExists(std::string(root + "Configs").c_str());
			EnsureDirectoryExists(std::string(root + "GameData").c_str());
			EnsureDirectoryExists(std::string(root + "AnalysisJson").c_str());
			EnsureDirectoryExists(std::string(root + "SaveStates").c_str());

			// set config values
			for (int i = 0; i < FCodeAnalysisState::kNoViewStates; i++)
			{
				const FCodeAnalysisViewState& viewState = CodeAnalysis.ViewState[i];
				FCodeAnalysisViewConfig& viewConfig = pGameConfig->ViewConfigs[i];

				viewConfig.bEnabled = viewState.Enabled;
				viewConfig.ViewAddress = viewState.GetCursorItem() ? viewState.GetCursorItem()->Address : 0;
			}

			SaveGameConfigToFile(*pGameConfig, configFName.c_str());
			SaveGameData(this, dataFName.c_str());		// The Past

			// The Future
			SaveGameState(this, saveStateFName.c_str());
			ExportGameJson(this, analysisJsonFName.c_str());
		}
	}

	// TODO: this could use
#if	SAVE_ROM_JSON
	const std::string romJsonFName = root + kRomInfoJsonFile;
	ExportROMJson(CodeAnalysis, romJsonFName.c_str());
#endif
}
#endif


void FSystem::DrawMainMenu(double timeMS)
{
#if SPECCY
	ui_zx_t* pZXUI = &UIZX;
	assert(pZXUI && pZXUI->zx && pZXUI->boot_cb);
		
	bool bExportAsm = false;

	if (ImGui::BeginMainMenuBar()) 
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::BeginMenu("New Game from Snapshot File"))
			{
				for(int gameNo=0;gameNo<GamesList.GetNoGames();gameNo++)
				{
					const FGameSnapshot& game = GamesList.GetGame(gameNo);
					
					if (ImGui::MenuItem(game.DisplayName.c_str()))
					{
						if (GamesList.LoadGame(gameNo))
						{
							FGameConfig *pNewConfig = CreateNewGameConfigFromSnapshot(game);
							if(pNewConfig != nullptr)
								StartGame(pNewConfig);
						}
					}
				}

				ImGui::EndMenu();
			}

#if ENABLE_RZX
			if (ImGui::BeginMenu("New Game from RZX File"))
			{
				for (int gameNo = 0; gameNo < RZXGamesList.GetNoGames(); gameNo++)
				{
					const FGameSnapshot& game = RZXGamesList.GetGame(gameNo);

					if (ImGui::MenuItem(game.DisplayName.c_str()))
					{
						if (RZXManager.Load(game.FileName.c_str()))
						{
							FGameConfig* pNewConfig = CreateNewGameConfigFromSnapshot(game);
							if (pNewConfig != nullptr)
								StartGame(pNewConfig);
						}
					}
				}
				ImGui::EndMenu();
			}
#endif
			if (ImGui::BeginMenu( "Open Game"))
			{
				for (const auto& pGameConfig : GetGameConfigs())
				{
					if (ImGui::MenuItem(pGameConfig->Name.c_str()))
					{
						const std::string snapFolder = GetGlobalConfig().SnapshotFolder;
						const std::string gameFile = snapFolder + pGameConfig->SnapshotFile;

						if(GamesList.LoadGame(gameFile.c_str()))
						{
							StartGame(pGameConfig);
						}
					}
				}

				ImGui::EndMenu();
			}

			/*if (ImGui::MenuItem("Open POK File..."))
			{
				std::string pokFile;
				OpenFileDialog(pokFile, ".\\POKFiles", "POK\0*.pok\0");
			}*/
			
			if (ImGui::MenuItem("Save Game Data"))
			{
				SaveCurrentGameData();
			}
			if (ImGui::MenuItem("Export Binary File"))
			{
				if (pActiveGame != nullptr)
				{
					const std::string dir = GetGlobalConfig().WorkspaceRoot + "OutputBin/";
					EnsureDirectoryExists(dir.c_str());
					std::string outBinFname = dir + pActiveGame->pConfig->Name + ".bin";
					uint8_t *pSpecMem = new uint8_t[65536];
					for (int i = 0; i < 65536; i++)
						pSpecMem[i] = ReadByte(i);
					SaveBinaryFile(outBinFname.c_str(), pSpecMem, 65536);
					delete [] pSpecMem;
				}
			}

			if (ImGui::MenuItem("Export ASM File"))
			{
				// ImGui popup windows can't be activated from within a Menu so we set a flag to act on outside of the menu code.
				bExportAsm = true;
			}
			
			if (ImGui::BeginMenu("Export Skool File"))
			{
				if (ImGui::MenuItem("Export as Hexadecimal"))
				{
					ExportSkoolFile(true /* bHexadecimal*/);
				}
				if (ImGui::MenuItem("Export as Decimal"))
				{
					ExportSkoolFile(false /* bHexadecimal*/);
				}
#ifndef NDEBUG
				if (ImGui::BeginMenu("DEBUG"))
				{
					if (ImGui::MenuItem("Export ROM"))
					{
						ExportSkoolFile(true /* bHexadecimal */, "rom");
					}
					ImGui::EndMenu();
				}
#endif // NDEBUG
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Export Region Info File"))
			{
			}
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("System")) 
		{
			
			if (pActiveGame && ImGui::MenuItem("Reload Snapshot"))
			{
				const std::string snapFolder = GetGlobalConfig().SnapshotFolder;
				const std::string gameFile = snapFolder + pActiveGame->pConfig->SnapshotFile;
				GamesList.LoadGame(gameFile.c_str());
			}
			if (ImGui::MenuItem("Reset")) 
			{
				zx_reset(pZXUI->zx);
				ui_dbg_reset(&pZXUI->dbg);
			}
			/*if (ImGui::MenuItem("ZX Spectrum 48K", 0, (pZXUI->zx->type == ZX_TYPE_48K)))
			{
				pZXUI->boot_cb(pZXUI->zx, ZX_TYPE_48K);
				ui_dbg_reboot(&pZXUI->dbg);
			}
			if (ImGui::MenuItem("ZX Spectrum 128", 0, (pZXUI->zx->type == ZX_TYPE_128)))
			{
				pZXUI->boot_cb(pZXUI->zx, ZX_TYPE_128);
				ui_dbg_reboot(&pZXUI->dbg);
			}*/
			if (ImGui::BeginMenu("Joystick")) 
			{
				if (ImGui::MenuItem("None", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_NONE)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_NONE;
				}
				if (ImGui::MenuItem("Kempston", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_KEMPSTON)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_KEMPSTON;
				}
				if (ImGui::MenuItem("Sinclair #1", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_SINCLAIR_1)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_SINCLAIR_1;
				}
				if (ImGui::MenuItem("Sinclair #2", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_SINCLAIR_2)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_SINCLAIR_2;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Hardware")) 
		{
			ImGui::MenuItem("Memory Map", 0, &pZXUI->memmap.open);
			ImGui::MenuItem("Keyboard Matrix", 0, &pZXUI->kbd.open);
			ImGui::MenuItem("Audio Output", 0, &pZXUI->audio.open);
			ImGui::MenuItem("Z80 CPU", 0, &pZXUI->cpu.open);
			if (pZXUI->zx->type == ZX_TYPE_128)
			{
				ImGui::MenuItem("AY-3-8912", 0, &pZXUI->ay.open);
			}
			else 
			{
				pZXUI->ay.open = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options"))
		{
			FGlobalConfig& config = GetGlobalConfig();

			if (ImGui::BeginMenu("Number Mode"))
			{
				bool bClearCode = false;
				if (ImGui::MenuItem("Decimal", 0, GetNumberDisplayMode() == ENumberDisplayMode::Decimal))
				{
					SetNumberDisplayMode(ENumberDisplayMode::Decimal);
					CodeAnalysis.SetCodeAnalysisDirty();
					bClearCode = true;
				}
				if (ImGui::MenuItem("Hex - FEh", 0, GetNumberDisplayMode() == ENumberDisplayMode::HexAitch))
				{
					SetNumberDisplayMode(ENumberDisplayMode::HexAitch);
					CodeAnalysis.SetCodeAnalysisDirty();
					bClearCode = true;
				}
				if (ImGui::MenuItem("Hex - $FE", 0, GetNumberDisplayMode() == ENumberDisplayMode::HexDollar))
				{
					SetNumberDisplayMode(ENumberDisplayMode::HexDollar);
					CodeAnalysis.SetCodeAnalysisDirty();
					bClearCode = true;
				}

				// clear code text so it can be written again
				if (bClearCode)
				{
					for (int i = 0; i < 1 << 16; i++)
					{
						FCodeInfo* pCodeInfo = CodeAnalysis.GetCodeInfoForAddress(i);
						if (pCodeInfo && pCodeInfo->Text.empty() == false)
							pCodeInfo->Text.clear();

					}
				}

				ImGui::EndMenu();
			}
			ImGui::MenuItem("Scan Line Indicator", 0, &config.bShowScanLineIndicator);
			ImGui::MenuItem("Enable Audio", 0, &config.bEnableAudio);
			ImGui::MenuItem("Edit Mode", 0, &CodeAnalysis.bAllowEditing);
			ImGui::MenuItem("Show Opcode Values", 0, &CodeAnalysis.Config.bShowOpcodeValues);
			if(pActiveGame!=nullptr)
				ImGui::MenuItem("Save Snapshot with game", 0, &pActiveGame->pConfig->WriteSnapshot);

#ifndef NDEBUG
			ImGui::MenuItem("ImGui Demo", 0, &bShowImGuiDemo);
			ImGui::MenuItem("ImPlot Demo", 0, &bShowImPlotDemo);
#endif // NDEBUG
			ImGui::EndMenu();
		}
		// Note: this is a WIP menu, it'll be added in when it works properly!
#ifndef NDEBUG
		if (ImGui::BeginMenu("Tools"))
		{
			if (ImGui::MenuItem("Find Ascii Strings"))
			{
				CodeAnalysis.FindAsciiStrings(0x4000);
			}
			ImGui::EndMenu();
		}
#endif
		if (ImGui::BeginMenu("Windows"))
		{
			ImGui::MenuItem("DebugLog", 0, &bShowDebugLog);
			if (ImGui::BeginMenu("Code Analysis"))
			{
				for (int codeAnalysisNo = 0; codeAnalysisNo < FCodeAnalysisState::kNoViewStates; codeAnalysisNo++)
				{
					char menuName[32];
					sprintf(menuName, "Code Analysis %d", codeAnalysisNo + 1);
					ImGui::MenuItem(menuName, 0, &CodeAnalysis.ViewState[codeAnalysisNo].Enabled);
				}

				ImGui::EndMenu();
			}

			for (auto Viewer : Viewers)
			{
				ImGui::MenuItem(Viewer->GetName(), 0, &Viewer->bOpen);

			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) 
		{
			//ImGui::MenuItem("CPU Debugger", 0, &pZXUI->dbg.ui.open);
			//ImGui::MenuItem("Breakpoints", 0, &pZXUI->dbg.ui.show_breakpoints);
			ImGui::MenuItem("Memory Heatmap", 0, &pZXUI->dbg.ui.show_heatmap);
			if (ImGui::BeginMenu("Memory Editor")) 
			{
				ImGui::MenuItem("Window #1", 0, &pZXUI->memedit[0].open);
				ImGui::MenuItem("Window #2", 0, &pZXUI->memedit[1].open);
				ImGui::MenuItem("Window #3", 0, &pZXUI->memedit[2].open);
				ImGui::MenuItem("Window #4", 0, &pZXUI->memedit[3].open);
				ImGui::EndMenu();
			}
			/*if (ImGui::BeginMenu("Disassembler")) 
			{
				ImGui::MenuItem("Window #1", 0, &pZXUI->dasm[0].open);
				ImGui::MenuItem("Window #2", 0, &pZXUI->dasm[1].open);
				ImGui::MenuItem("Window #3", 0, &pZXUI->dasm[2].open);
				ImGui::MenuItem("Window #4", 0, &pZXUI->dasm[3].open);
				ImGui::EndMenu();
			}*/
			ImGui::EndMenu();
		}
		/*if (ImGui::BeginMenu("ImGui"))
		{
			ImGui::MenuItem("Show Demo", 0, &pUI->bShowImGuiDemo);
			ImGui::EndMenu();
		}*/
		

		/*if (ImGui::BeginMenu("Game Viewers"))
		{
			for (auto &viewerIt : pUI->GameViewers)
			{
				FGameViewer &viewer = viewerIt.second;
				ImGui::MenuItem(viewerIt.first.c_str(), 0, &viewer.bOpen);
			}
			ImGui::EndMenu();
		}*/
		
		//ui_util_options_menu(timeMS, pZXUI->dbg.dbg.stopped);

		// draw emu timings
		ImGui::SameLine(ImGui::GetWindowWidth() - 120);
		if (pZXUI->dbg.dbg.stopped) 
			ImGui::Text("emu: stopped");
		else 
			ImGui::Text("emu: %.2fms", timeMS);

		ImGui::EndMainMenuBar();
	}

	if (bExportAsm)
	{
		ImGui::OpenPopup("Export ASM File");
	}
	if (ImGui::BeginPopupModal("Export ASM File", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static ImU16 addrStart = kScreenAttrMemEnd + 1;
		static ImU16 addrEnd = 0xffff;

		ImGui::Text("Address range to export");
		bool bHex = GetNumberDisplayMode() != ENumberDisplayMode::Decimal;
		const char* formatStr = bHex ? "%x" : "%u";
		ImGuiInputTextFlags flags = bHex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;

		ImGui::InputScalar("Start", ImGuiDataType_U16, &addrStart, NULL, NULL, formatStr, flags);
		ImGui::SameLine();
		ImGui::InputScalar("End", ImGuiDataType_U16, &addrEnd, NULL, NULL, formatStr, flags);

		if (ImGui::Button("Export", ImVec2(120, 0))) 
		{ 
			if (addrEnd > addrStart)
			{
				if (pActiveGame != nullptr)
				{
					const std::string dir = GetGlobalConfig().WorkspaceRoot + "OutputASM/";
					EnsureDirectoryExists(dir.c_str());
				
					char addrRangeStr[16];
					if (bHex)
						snprintf(addrRangeStr, 16, "_%x_%x", addrStart, addrEnd);
					else
						snprintf(addrRangeStr, 16, "_%u_%u", addrStart, addrEnd);

					std::string outBinFname = dir + pActiveGame->pConfig->Name + addrRangeStr + ".asm";

					ExportAssembler(CodeAnalysis, outBinFname.c_str(), addrStart, addrEnd);
				}
				ImGui::CloseCurrentPopup(); 
			}
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup(); 
		}
		ImGui::EndPopup();
	}
#endif
}

#if SPECCY
static void UpdateMemmap(ui_zx_t* ui)
{
	assert(ui && ui->zx);
	ui_memmap_reset(&ui->memmap);
	if (ZX_TYPE_48K == ui->zx->type) 
	{
		ui_memmap_layer(&ui->memmap, "System");
		ui_memmap_region(&ui->memmap, "ROM", 0x0000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM", 0x4000, 0xC000, true);
	}
	else 
	{
		const uint8_t m = ui->zx->last_mem_config;
		ui_memmap_layer(&ui->memmap, "Layer 0");
		ui_memmap_region(&ui->memmap, "ZX128 ROM", 0x0000, 0x4000, !(m & (1 << 4)));
		ui_memmap_region(&ui->memmap, "RAM 5", 0x4000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM 2", 0x8000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM 0", 0xC000, 0x4000, 0 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 1");
		ui_memmap_region(&ui->memmap, "ZX48K ROM", 0x0000, 0x4000, 0 != (m & (1 << 4)));
		ui_memmap_region(&ui->memmap, "RAM 1", 0xC000, 0x4000, 1 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 2");
		ui_memmap_region(&ui->memmap, "RAM 2", 0xC000, 0x4000, 2 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 3");
		ui_memmap_region(&ui->memmap, "RAM 3", 0xC000, 0x4000, 3 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 4");
		ui_memmap_region(&ui->memmap, "RAM 4", 0xC000, 0x4000, 4 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 5");
		ui_memmap_region(&ui->memmap, "RAM 5", 0xC000, 0x4000, 5 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 6");
		ui_memmap_region(&ui->memmap, "RAM 6", 0xC000, 0x4000, 6 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 7");
		ui_memmap_region(&ui->memmap, "RAM 7", 0xC000, 0x4000, 7 == (m & 7));
	}
}

void DrawDebuggerUI(ui_dbg_t *pDebugger)
{
	if (ImGui::Begin("CPU Debugger"))
	{
		ui_dbg_dbgwin_draw(pDebugger);
	}
	ImGui::End();
	ui_dbg_draw(pDebugger);
	/*
	if (!(pDebugger->ui.open || pDebugger->ui.show_heatmap || pDebugger->ui.show_breakpoints)) {
		return;
	}
	_ui_dbg_dbgwin_draw(pDebugger);
	_ui_dbg_heatmap_draw(pDebugger);
	_ui_dbg_bp_draw(pDebugger);*/
}

void StoreRegisters_Z80(FCodeAnalysisState& state);

void FSpectrumEmu::Tick()
{
	SpectrumViewer.Tick();

	ExecThisFrame = ui_zx_before_exec(&UIZX);

	if (ExecThisFrame)
	{
		const float frameTime = std::min(1000000.0f / ImGui::GetIO().Framerate, 32000.0f) * ExecSpeedScale;
		//const float frameTime = min(1000000.0f / 50, 32000.0f) * ExecSpeedScale;
		const uint32_t microSeconds = std::max(static_cast<uint32_t>(frameTime), uint32_t(1));

		// TODO: Start frame method in analyser
		CodeAnalysis.FrameTrace.clear();
		StoreRegisters_Z80(CodeAnalysis);
#if ENABLE_CAPTURES
		const uint32_t ticks_to_run = clk_ticks_to_run(&ZXEmuState.clk, microSeconds);
		uint32_t ticks_executed = 0;
		while (UIZX.dbg.dbg.z80->trap_id != kCaptureTrapId && ticks_executed < ticks_to_run)
		{
			ticks_executed += z80_exec(&ZXEmuState.cpu, ticks_to_run - ticks_executed);

			if (UIZX.dbg.dbg.z80->trap_id == kCaptureTrapId)
			{
				const uint16_t PC = GetPC();
				FMachineState* pMachineState = CodeAnalysis.GetMachineState(PC);
				if (pMachineState == nullptr)
				{
					pMachineState = AllocateMachineState(CodeAnalysis);
					CodeAnalysis.SetMachineStateForAddress(PC, pMachineState);
				}

				CaptureMachineState(pMachineState, this);
				UIZX.dbg.dbg.z80->trap_id = 0;
				_ui_dbg_continue(&UIZX.dbg);
			}
		}
		clk_ticks_executed(&ZXEmuState.clk, ticks_executed);
		kbd_update(&ZXEmuState.kbd);
#else
		zx_exec(&ZXEmuState, microSeconds);
#endif
		/*if (RZXManager.GetReplayMode() == EReplayMode::Playback)
		{
			assert(ZXEmuState.valid);
			uint32_t icount = RZXManager.Update();

			uint32_t ticks_to_run = clk_ticks_to_run(&ZXEmuState.clk, microSeconds);
			uint32_t ticks_executed = z80_exec(&ZXEmuState.cpu, ticks_to_run);
			clk_ticks_executed(&ZXEmuState.clk, ticks_executed);
			kbd_update(&ZXEmuState.kbd);
		}
		else
		{
			uint32_t frameTicks = ZXEmuState.frame_scan_lines* ZXEmuState.scanline_period;
			//zx_exec(&ZXEmuState, microSeconds);

			//uint32_t ticks_to_run = clk_ticks_to_run(&ZXEmuState.clk, microSeconds);
			//frameTicks = ticks_to_run;
			ZXEmuState.clk.ticks_to_run = frameTicks;
			const uint32_t ticksExecuted = z80_exec(&ZXEmuState.cpu, frameTicks);
			clk_ticks_executed(&ZXEmuState.clk, ticksExecuted);
			kbd_update(&ZXEmuState.kbd);
		}*/
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);

		FrameTraceViewer.CaptureFrame();
		FrameScreenPixWrites.clear();
		FrameScreenAttrWrites.clear();

		if (bStepToNextFrame)
		{
			_ui_dbg_break(&UIZX.dbg);
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
			bStepToNextFrame = false;
		}

		
		// on debug break send code analyser to address
		else if (UIZX.dbg.dbg.z80->trap_id >= UI_DBG_STEP_TRAPID)
		{
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
		}
	}

	UpdateCharacterSets(CodeAnalysis);

	// Draw UI
	DrawDockingView();
}

void FSpectrumEmu::DrawMemoryTools()
{
	if (ImGui::Begin("Memory Tools") == false)
	{
		ImGui::End();
		return;
	}
	if (ImGui::BeginTabBar("MemoryToolsTabBar"))
	{

		if (ImGui::BeginTabItem("Memory Handlers"))
		{
			DrawMemoryHandlers(this);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Memory Analysis"))
		{
			DrawMemoryAnalysis(this);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Memory Diff"))
		{
			DrawMemoryDiffUI(this);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("IO Analysis"))
		{
			IOAnalysis.DrawUI();
			ImGui::EndTabItem();
		}
		
		/*if (ImGui::BeginTabItem("Functions"))
		{
			DrawFunctionInfo(pUI);
			ImGui::EndTabItem();
		}*/

		ImGui::EndTabBar();
	}

	ImGui::End();
}
#endif

void FSystem::DrawUI()
{
#if SPECCY
	ui_zx_t* pZXUI = &UIZX;
	const double timeMS = 1000.0f / ImGui::GetIO().Framerate;
	
	if(ExecThisFrame)
		ui_zx_after_exec(pZXUI);

	const int instructionsThisFrame = (int)CodeAnalysis.FrameTrace.size();
	static int maxInst = 0;
	maxInst = std::max(maxInst, instructionsThisFrame);

	DrawMainMenu(timeMS);
	if (pZXUI->memmap.open)
	{
		UpdateMemmap(pZXUI);
	}

	// call the Chips UI functions
	ui_audio_draw(&pZXUI->audio, pZXUI->zx->sample_pos);
	ui_z80_draw(&pZXUI->cpu);
	ui_ay38910_draw(&pZXUI->ay);
	ui_kbd_draw(&pZXUI->kbd);
	ui_memmap_draw(&pZXUI->memmap);

	for (int i = 0; i < 4; i++)
	{
		ui_memedit_draw(&pZXUI->memedit[i]);
		ui_dasm_draw(&pZXUI->dasm[i]);
	}

	//DrawDebuggerUI(&pZXUI->dbg);

	// Draw registered viewers
	for (auto Viewer : Viewers)
	{
		if (Viewer->bOpen)
		{
			if (ImGui::Begin(Viewer->GetName(), &Viewer->bOpen))
				Viewer->DrawUI();
			ImGui::End();
		}
	}

	//DasmDraw(&pUI->FunctionDasm);
	// show spectrum window
	if (ImGui::Begin("Spectrum View"))
	{
		SpectrumViewer.Draw();
	}
	ImGui::End();

	if (ImGui::Begin("Frame Trace"))
	{
		FrameTraceViewer.Draw();
	}
	ImGui::End();

	if (RZXManager.GetReplayMode() == EReplayMode::Playback)
	{
		if (ImGui::Begin("RZX Info"))
		{
			RZXManager.DrawUI();
		}
		ImGui::End();
	}

	// cheats 
	if (ImGui::Begin("Pokes"))
	{
		DrawCheatsUI();
	}
	ImGui::End();

	// game viewer
	if (ImGui::Begin("Game Viewer"))
	{
		if (pActiveGame != nullptr)
		{
			ImGui::Text(pActiveGame->pConfig->Name.c_str());
			pActiveGame->pViewerConfig->pDrawFunction(this, pActiveGame);
		}
		
	}
	ImGui::End();

	DrawGraphicsViewer(GraphicsViewer);
	DrawMemoryTools();

	// COde analysis views
	for (int codeAnalysisNo = 0; codeAnalysisNo < FCodeAnalysisState::kNoViewStates; codeAnalysisNo++)
	{
		char name[32];
		sprintf(name, "Code Analysis %d", codeAnalysisNo + 1);
		if(CodeAnalysis.ViewState[codeAnalysisNo].Enabled)
		{
			if (ImGui::Begin(name,&CodeAnalysis.ViewState[codeAnalysisNo].Enabled))
			{
				DrawCodeAnalysisData(CodeAnalysis, codeAnalysisNo);
			}
			ImGui::End();
		}

	}

	if (ImGui::Begin("Call Stack"))
	{
		DrawStackInfo(CodeAnalysis);
	}
	ImGui::End();

	if (ImGui::Begin("Trace"))
	{
		DrawTrace(CodeAnalysis);
	}
	ImGui::End();

	if (ImGui::Begin("Registers"))
	{
		DrawRegisters(CodeAnalysis);
	}
	ImGui::End();

	if (ImGui::Begin("Watches"))
	{
		DrawWatchWindow(CodeAnalysis);
	}
	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Character Maps"))
	{
		DrawCharacterMapViewer(CodeAnalysis, CodeAnalysis.GetFocussedViewState());
	}
	ImGui::End();

	if (bShowDebugLog)
		g_ImGuiLog.Draw("Debug Log", &bShowDebugLog);
#endif
}

bool FSystem::DrawDockingView()
{
#if SPECCY
	//SCOPE_PROFILE_CPU("UI", "DrawUI", ProfCols::UI);

	static bool opt_fullscreen_persistant = true;
	bool opt_fullscreen = opt_fullscreen_persistant;
	//static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	bool bOpen = false;
	ImGuiDockNodeFlags dockFlags = 0;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}

	// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
	if (dockFlags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
	// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
	bool bQuit = false;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("DockSpace Demo", &bOpen, window_flags))
	{
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			const ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
		}

		//bQuit = MainMenu();
		//DrawDebugWindows(uiState);
		DrawUI();
		ImGui::End();
	}
	else
	{
		ImGui::PopStyleVar();
		bQuit = true;
	}

	return bQuit;
#endif
	return false;
}

// Cheats
#if SPECCY
void FSpectrumEmu::DrawCheatsUI()
{
	if (pActiveGame == nullptr)
		return;
	
	if (pActiveGame->pConfig->Cheats.size() == 0)
	{
		ImGui::Text("No pokes loaded");
		return;
	}

	static int bAdvancedMode = 0;
    ImGui::RadioButton("Standard", &bAdvancedMode, 0);
	ImGui::SameLine();
    ImGui::RadioButton("Advanced", &bAdvancedMode, 1);
	ImGui::Separator();
    
	FGameConfig &config = *pActiveGame->pConfig;

	for (FCheat &cheat : config.Cheats)
	{
		ImGui::PushID(cheat.Description.c_str());
		bool bToggleCheat = false;
		bool bWasEnabled = cheat.bEnabled;
		int userDefinedCount = 0;
		
		if (cheat.bHasUserDefinedEntries)
		{
			ImGui::Text(cheat.Description.c_str());
		}
		else
		{
			if (ImGui::Checkbox(cheat.Description.c_str(), &cheat.bEnabled))
			{
				bToggleCheat = true;
			}
		}
		
		for (auto &entry : cheat.Entries)
		{
			// Display memory locations in advanced mode
			if (bAdvancedMode)
			{
				ImGui::Text("%s:%s", NumStr(entry.Address), entry.bUserDefined ? "" : NumStr((uint8_t)entry.Value)); 
			}

			if (entry.bUserDefined)
			{
				char tempStr[16] = {0};
				sprintf(tempStr, "##Value %d", ++userDefinedCount);
				
				// Display the value of the memory location in the input field.
				// If the user has modified the value then display that instead.
				uint8_t value = entry.bUserDefinedValueDirty ? entry.Value : CodeAnalysis.CPUInterface->ReadByte(entry.Address);
				
				if (bAdvancedMode)
					ImGui::SameLine();

				ImGui::SetNextItemWidth(45);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

				if (ImGui::InputScalar(tempStr, ImGuiDataType_U8, &value, NULL, NULL, "%d", 0))
				{
					entry.Value = value;
					entry.bUserDefinedValueDirty = true;
				}
				ImGui::PopStyleVar();
			}

			if (bAdvancedMode)
			{
				DrawAddressLabel(CodeAnalysis, CodeAnalysis.GetFocussedViewState(), entry.Address);
			}
		}

		// Show the Apply and Revert buttons if we have any user defined values.
		if (cheat.bHasUserDefinedEntries)
		{
			if (ImGui::Button("Apply"))
			{
				cheat.bEnabled = true;
				bToggleCheat = true;
			}

			ImGui::SameLine();
			bool bDisabledRevert = !cheat.bEnabled; 
			if (bDisabledRevert)
				ImGui::BeginDisabled();
			if (ImGui::Button("Revert"))
			{
				cheat.bEnabled = false;
				bToggleCheat = true;
			}
			if (bDisabledRevert)
				ImGui::EndDisabled();
		}

		if (bToggleCheat)
		{
			for (auto &entry : cheat.Entries)
			{
				if (cheat.bEnabled)	// cheat activated
				{
					// store old value
					if (!bWasEnabled)
						entry.OldValue = ReadByte( entry.Address);
					WriteByte( entry.Address, static_cast<uint8_t>(entry.Value));
					entry.bUserDefinedValueDirty = false;
				}
				else
				{
					WriteByte( entry.Address, entry.OldValue);
				}

				// if code has been modified then clear the code text so it gets regenerated
				FCodeInfo* pCodeInfo = CodeAnalysis.GetCodeInfoForAddress(entry.Address);
				if (pCodeInfo)
					pCodeInfo->Text.clear();
			}
			CodeAnalysis.SetCodeAnalysisDirty();

			LOGINFO("Poke %s: '%s' [%d byte(s)]", cheat.bEnabled ? "applied" : "reverted", cheat.Description.c_str(), cheat.Entries.size());

		}
		ImGui::Separator();
		ImGui::PopID();
	}
}

// Util functions - move
uint16_t GetScreenPixMemoryAddress(int x, int y)
{
	if (x < 0 || x>255 || y < 0 || y> 191)
		return 0;

	const int char_x = x / 8;
	const uint16_t addr = 0x4000 | ((y & 7) << 8) | (((y >> 3) & 7) << 5) | (((y >> 6) & 3) << 11) | (char_x & 31);

	return addr;
}

uint16_t GetScreenAttrMemoryAddress(int x, int y)
{
	if (x < 0 || x>255 || y < 0 || y> 191)
		return 0;

	const int char_x = x / 8;
	const int char_y = y / 8;

	return 0x5800 + (char_y * 32) + char_x;
}

bool GetScreenAddressCoords(uint16_t addr, int& x, int &y)
{
	if (addr < 0x4000 || addr >= 0x5800)
		return false;

	const int y02 = (addr >> 8) & 7;	// bits 0-2
	const int y35 = (addr >> 5) & 7;	// bits 3-5
	const int y67 = (addr >> 11) & 3;	// bits 6 & 7
	x = (addr & 31) * 8;
	y = y02 | (y35 << 3) | (y67 << 6);
	return true;
}

bool GetAttribAddressCoords(uint16_t addr, int& x, int& y)
{
	if (addr < 0x5800 || addr >= 0x5B00)
		return false;
	int offset = addr - 0x5800;

	x = (offset & 31) << 3;
	y = (offset >> 5) << 3;

	return true;
}
#endif