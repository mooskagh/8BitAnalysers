#include "System.h"

#include <cstdint>
#include <imgui.h>


bool FSystem::Init(const FConfig* pConfig)
{
	// need to move app.h to shared
	//SetWindowTitle(GetAppTitle().c_str());
	//SetWindowIcon(GetWindowIconName().c_str());

	// can we do init in this order?:
	// 1. base init stuff
	// 2. platform init stuff

	// or do things need to be interspersed?

#if SPECCY
	// Initialise Emulator
	LoadGlobalConfig(kGlobalConfigFilename);
	FGlobalConfig& globalConfig = GetGlobalConfig();
	SetNumberDisplayMode(globalConfig.NumberDisplayMode);
	CodeAnalysis.Config.bShowOpcodeValues = globalConfig.bShowOpcodeValues;

	// init emulator
	InitEmulator()

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
	InitialiseCodeAnalysisPages();

#if SPECCY
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
#endif
	
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

		// get rom name from derived class?
		std::string romJsonFName = GetRomFname();
#if SPECCY	
		std::string romJsonFName = kRomInfo48JsonFile;

		if (config.Model == ESpectrumModel::Spectrum128K)
			romJsonFName = kRomInfo128JsonFile;
#endif
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
