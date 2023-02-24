#include "System.h"

#include "../../App.h"

#include <cstdint>
#include <imgui.h>
#include <ImGuiSupport/ImGuiTexture.h>

bool FSystem::Init(const FSystemParams& params)
{
	SetWindowTitle(params.AppTitle.c_str());
	SetWindowIcon(params.WindowIconName.c_str());

	// setup pixel buffer
	const size_t pixelBufferSize = params.ScreenBufferSize;
	FrameBuffer = new unsigned char[pixelBufferSize * 2];

	// setup texture
	Texture = ImGui_CreateTextureRGBA(FrameBuffer, params.ScreenTextureWidth, params.ScreenTextureHeight);

	GamesList.Init(params.pGameLoader);
	// todo: get snapshot folder
	GamesList.EnumerateGames(".\\");

	bInitialised = true;
	return true;
}

void FSystem::Shutdown()
{
}

void FSystem::Tick()
{
	DrawDockingView();
}

void FSystem::DrawUI()
{
	const double timeMS = 1000.0f / ImGui::GetIO().Framerate;
	DrawMainMenu(timeMS);
}

void FSystem::DrawFileMenu()
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
						/*FGameConfig *pNewConfig = CreateNewGameConfigFromSnapshot(game);
						if(pNewConfig != nullptr)
							StartGame(pNewConfig);*/
					}
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu( "Open Game"))
		{
			/*for (const auto& pGameConfig : GetGameConfigs())
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
			}*/

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Save Game Data"))
		{
			//SaveCurrentGameData();
		}
		if (ImGui::MenuItem("Export Binary File"))
		{
			/*if (pActiveGame != nullptr)
			{
				const std::string dir = GetGlobalConfig().WorkspaceRoot + "OutputBin/";
				EnsureDirectoryExists(dir.c_str());
				std::string outBinFname = dir + pActiveGame->pConfig->Name + ".bin";
				uint8_t *pSpecMem = new uint8_t[65536];
				for (int i = 0; i < 65536; i++)
					pSpecMem[i] = ReadByte(i);
				SaveBinaryFile(outBinFname.c_str(), pSpecMem, 65536);
				delete [] pSpecMem;
			}*/
		}

		if (ImGui::MenuItem("Export ASM File"))
		{
			// ImGui popup windows can't be activated from within a Menu so we set a flag to act on outside of the menu code.
			bExportAsm = true;
		}
		ImGui::EndMenu();
	}
}

void FSystem::DrawSystemMenu()
{
	if (ImGui::BeginMenu("System")) 
	{
		/*if (pActiveGame && ImGui::MenuItem("Reload Snapshot"))
		{
			const std::string snapFolder = GetGlobalConfig().SnapshotFolder;
			const std::string gameFile = snapFolder + pActiveGame->pConfig->SnapshotFile;
			GamesList.LoadGame(gameFile.c_str());
		}*/
		if (ImGui::MenuItem("Reset")) 
		{
			//cpc_reset(pCPCUI->cpc);
			//ui_dbg_reset(&pCPCUI->dbg);
		}
			
		if (ImGui::BeginMenu("Joystick")) 
		{
			/*if (ImGui::MenuItem("None", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_NONE)))
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
			}*/
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}

void FSystem::DrawHardwareMenu()
{

}

void FSystem::DrawOptionsMenu()
{
	if (ImGui::BeginMenu("Options"))
	{
		/*FGlobalConfig& config = GetGlobalConfig();

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
			ImGui::MenuItem("Save Snapshot with game", 0, &pActiveGame->pConfig->WriteSnapshot);*/

#ifndef NDEBUG
		ImGui::MenuItem("ImGui Demo", 0, &bShowImGuiDemo);
		ImGui::MenuItem("ImPlot Demo", 0, &bShowImPlotDemo);
#endif // NDEBUG
		ImGui::EndMenu();
	}
}

void FSystem::DrawToolsMenu()
{
	// Note: this is a WIP menu, it'll be added in when it works properly!
#ifndef NDEBUG
	if (ImGui::BeginMenu("Tools"))
	{
		/*if (ImGui::MenuItem("Find Ascii Strings"))
		{
			CodeAnalysis.FindAsciiStrings(0x4000);
		}*/
		ImGui::EndMenu();
	}
#endif
}

void FSystem::DrawWindowsMenu()
{
	if (ImGui::BeginMenu("Windows"))
	{
		/*ImGui::MenuItem("DebugLog", 0, &bShowDebugLog);
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

		}*/
		ImGui::EndMenu();
	}
}

void FSystem::DrawDebugMenu()
{
	/*if (ImGui::BeginMenu("Debug")) 
	{
		ImGui::MenuItem("Memory Heatmap", 0, &pCPCUI->dbg.ui.show_heatmap);
		if (ImGui::BeginMenu("Memory Editor")) 
		{
			ImGui::MenuItem("Window #1", 0, &pCPCUI->memedit[0].open);
			ImGui::MenuItem("Window #2", 0, &pCPCUI->memedit[1].open);
			ImGui::MenuItem("Window #3", 0, &pCPCUI->memedit[2].open);
			ImGui::MenuItem("Window #4", 0, &pCPCUI->memedit[3].open);
			ImGui::EndMenu();
		}
			
		ImGui::EndMenu();
	}*/
}

void FSystem::DrawMenus()
{
	DrawFileMenu();
	DrawSystemMenu();
	DrawHardwareMenu();
	DrawOptionsMenu();
	DrawToolsMenu();
	DrawWindowsMenu();
	DrawDebugMenu();
}

void FSystem::DrawMainMenu(double timeMS)
{
	bExportAsm = false;

	if (ImGui::BeginMainMenuBar()) 
	{
		DrawMenus();
		
		// draw emu timings
		ImGui::SameLine(ImGui::GetWindowWidth() - 120);
		if (IsStopped()) 
			ImGui::Text("emu: stopped");
		else 
			ImGui::Text("emu: %.2fms", timeMS);

		ImGui::EndMainMenuBar();
	}

	//if (bExportAsm)
	{
		ExportAsmGui();
	}
}

void FSystem::ExportAsmGui()
{
	if (bExportAsm)
	{
		ImGui::OpenPopup("Export ASM File");
	}
	if (ImGui::BeginPopupModal("Export ASM File", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static ImU16 addrStart = 0;//kScreenAttrMemEnd + 1;
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
				/*
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
				}*/
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
}

bool FSystem::DrawDockingView()
{
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
}
