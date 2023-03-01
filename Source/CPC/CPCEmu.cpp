#include <cstdint>

#define CHIPS_IMPL
#include <imgui.h>

//typedef void (*z80dasm_output_t)(char c, void* user_data);

#include "CPCEmu.h"

#include <sokol_audio.h>
#include "cpc-roms.h"

#include <ImGuiSupport/ImGuiTexture.h>
#include "App.h"

const std::string kAppTitle = "CPC Analyser";

const uint8_t* FCpcEmu::GetMemPtr(uint16_t address) const 
{
	// todo: deal with 464/6128 differences

	const int bank = address >> 14;
	const int bankAddr = address & 0x3fff;

	return &CpcEmuState.ram[bank][bankAddr];
}

#if 0
// Memory access functions
uint8_t* MemGetPtr(cpc_t* cpc, int layer, uint16_t addr)
{

	return 0;
}

uint8_t MemReadFunc(int layer, uint16_t addr, void* user_data)
{

	return 0xFF;
}

void MemWriteFunc(int layer, uint16_t addr, uint8_t data, void* user_data)
{

}

uint8_t	FCpcEmu::ReadByte(uint16_t address) const
{
	return MemReadFunc(CurrentLayer, address, const_cast<cpc_t *>(&CpcEmuState));

}
uint16_tFCpcEmu::ReadWord(uint16_t address) const 
{
	return ReadByte(address) | (ReadByte(address + 1) << 8);
}



void FCpcEmu::WriteByte(uint16_t address, uint8_t value)
{
	MemWriteFunc(CurrentLayer, address, value, &CpcEmuState);
}


uint16_t FCpcEmu::GetPC(void) 
{
	return z80_pc(&CpcEmuState.cpu);
} 

uint16_t FCpcEmu::GetSP(void)
{
	return z80_sp(&CpcEmuState.cpu);
}

void* FCpcEmu::GetCPUEmulator(void)
{
	return &CpcEmuState.cpu;
}

bool FCpcEmu::IsAddressBreakpointed(uint16_t addr)
{
	for (int i = 0; i < UICpc.dbg.dbg.num_breakpoints; i++) 
	{
		if (UICpc.dbg.dbg.breakpoints[i].addr == addr)
			return true;
	}

	return false;
}

bool FCpcEmu::ToggleExecBreakpointAtAddress(uint16_t addr)
{
	int index = _ui_dbg_bp_find(&UICpc.dbg, UI_DBG_BREAKTYPE_EXEC, addr);
	if (index >= 0) 
	{
		/* breakpoint already exists, remove */
		_ui_dbg_bp_del(&UICpc.dbg, index);
		return false;
	}
	else 
	{
		/* breakpoint doesn't exist, add a new one */
		return _ui_dbg_bp_add_exec(&UICpc.dbg, true, addr);
	}
}

bool FCpcEmu::ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize)
{
	const int type = dataSize == 1 ? UI_DBG_BREAKTYPE_BYTE : UI_DBG_BREAKTYPE_WORD;
	int index = _ui_dbg_bp_find(&UICpc.dbg, type, addr);
	if (index >= 0)
	{
		// breakpoint already exists, remove 
		_ui_dbg_bp_del(&UICpc.dbg, index);
		return false;
	}
	else
	{
		// breakpoint doesn't exist, add a new one 
		if (UICpc.dbg.dbg.num_breakpoints < UI_DBG_MAX_BREAKPOINTS)
		{
			ui_dbg_breakpoint_t* bp = &UICpc.dbg.dbg.breakpoints[UICpc.dbg.dbg.num_breakpoints++];
			bp->type = type;
			bp->cond = UI_DBG_BREAKCOND_NONEQUAL;
			bp->addr = addr;
			bp->val = ReadByte(addr);
			bp->enabled = true;
			return true;
		}
		else 
		{
			return false;
		}
	}
}

void FCpcEmu::Break(void)
{
	_ui_dbg_break(&UICpc.dbg);
}

void FCpcEmu::Continue(void) 
{
	_ui_dbg_continue(&UICpc.dbg);
}

void FCpcEmu::StepOver(void)
{
	_ui_dbg_step_over(&UICpc.dbg);
}

void FCpcEmu::StepInto(void)
{
	_ui_dbg_step_into(&UICpc.dbg);
}

void FCpcEmu::StepFrame()
{
	_ui_dbg_continue(&UICpc.dbg);
	bStepToNextFrame = true;
}

void FCpcEmu::StepScreenWrite()
{
	_ui_dbg_continue(&UICpc.dbg);
	bStepToNextScreenWrite = true;
}

void FCpcEmu::GraphicsViewerSetView(uint16_t address, int charWidth)
{
#if SPECCY
	GraphicsViewerGoToAddress(address);
	GraphicsViewerSetCharWidth(charWidth);
#endif //#if SPECCY
}
#endif

bool FCpcEmu::ShouldExecThisFrame(void) const
{
	return ExecThisFrame;
}

bool FCpcEmu::IsStopped(void) const
{
	return UICpc.dbg.dbg.stopped;
}

/* reboot callback */
static void boot_cb(cpc_t* sys, cpc_type_t type)
{
	cpc_desc_t desc = {}; // TODO
	cpc_init(sys, &desc);
}

void* gfx_create_texture(int w, int h)
{
	return ImGui_CreateTextureRGBA(nullptr, w, h);
}

void gfx_update_texture(void* h, void* data, int data_byte_size)
{
	ImGui_UpdateTextureRGBA(h, (unsigned char *)data);
}

void gfx_destroy_texture(void* h)
{
	
}

/* audio-streaming callback */
static void PushAudio(const float* samples, int num_samples, void* user_data)
{
	// todo
	//FCPCEmu* pEmu = (FCPCEmu*)user_data;
	//if(GetGlobalConfig().bEnableAudio)
	//	saudio_push(samples, num_samples);
}

int CPCTrapCallback(uint16_t pc, int ticks, uint64_t pins, void* user_data)
{
	FCpcEmu* pEmu = (FCpcEmu*)user_data;
	return pEmu->TrapFunction(pc, ticks, pins);
}

// Note - you can't read register values in Trap function
// They are only written back at end of exec function
int	FCpcEmu::TrapFunction(uint16_t pc, int ticks, uint64_t pins)
{
	return 0;


}

// Note - you can't read the cpu vars during tick
// They are only written back at end of exec function
uint64_t FCpcEmu::Z80Tick(int num, uint64_t pins)
{
	return pins;
}

static uint64_t Z80TickThunk(int num, uint64_t pins, void* user_data)
{
	FCpcEmu* pEmu = (FCpcEmu*)user_data;
	return pEmu->Z80Tick(num, pins);
}


bool FCpcEmu::Init(const FCpcConfig& config)
{
	SetWindowTitle(kAppTitle.c_str());
	SetWindowIcon("CPCALogo.png");

	// setup pixel buffer
	const size_t pixelBufferSize = AM40010_DBG_DISPLAY_WIDTH * AM40010_DBG_DISPLAY_HEIGHT * 4;
	FrameBuffer = new unsigned char[pixelBufferSize * 2];

	// setup texture
	Texture = ImGui_CreateTextureRGBA(FrameBuffer, AM40010_DISPLAY_WIDTH, AM40010_DISPLAY_HEIGHT);

	// A class that deals with loading games.
	GameLoader.Init(this);
	GamesList.Init(&GameLoader);
	
	// todo: get snapshot folder
	GamesList.EnumerateGames(".\\");

	// Initialise the CPC emulator

	cpc_type_t type = CPC_TYPE_464;
	//cpc_type_t type = CPC_TYPE_6128;
	cpc_joystick_type_t joy_type = CPC_JOYSTICK_NONE;

	cpc_desc_t desc;
	memset(&desc, 0, sizeof(cpc_desc_t));
	desc.type = type;
	desc.user_data = this;
	desc.joystick_type = joy_type;
	desc.pixel_buffer = FrameBuffer;
	desc.pixel_buffer_size = static_cast<int>(pixelBufferSize);
	desc.audio_cb = PushAudio;	// our audio callback
	desc.audio_sample_rate = saudio_sample_rate();
	desc.rom_464_os = dump_cpc464_os_bin;
	desc.rom_464_os_size = sizeof(dump_cpc464_os_bin);
	desc.rom_464_basic = dump_cpc464_basic_bin;
	desc.rom_464_basic_size = sizeof(dump_cpc464_basic_bin);
	desc.rom_6128_os = dump_cpc6128_os_bin;
	desc.rom_6128_os_size = sizeof(dump_cpc6128_os_bin);
	desc.rom_6128_basic = dump_cpc6128_basic_bin;
	desc.rom_6128_basic_size = sizeof(dump_cpc6128_basic_bin);
	desc.rom_6128_amsdos = dump_cpc6128_amsdos_bin;
	desc.rom_6128_amsdos_size = sizeof(dump_cpc6128_amsdos_bin);
	desc.rom_kcc_os = dump_kcc_os_bin;
	desc.rom_kcc_os_size = sizeof(dump_kcc_os_bin);
	desc.rom_kcc_basic = dump_kcc_bas_bin;
	desc.rom_kcc_basic_size = sizeof(dump_kcc_bas_bin);

	cpc_init(&CpcEmuState, &desc);

	// Clear UI
	memset(&UICpc, 0, sizeof(ui_cpc_t));

	// Trap callback needs to be set before we create the UI
	z80_trap_cb(&CpcEmuState.cpu, CPCTrapCallback, this);

	// todo: put this back in
	// Setup out tick callback
	//OldTickCB = CPCEmuState.cpu.tick_cb;
	//OldTickUserData = CPCEmuState.cpu.user_data;
	//CPCEmuState.cpu.tick_cb = Z80TickThunk;
	//CPCEmuState.cpu.user_data = this;

	//ui_init(zxui_draw);
	{
		ui_cpc_desc_t desc = { 0 };
		desc.cpc = &CpcEmuState;
		desc.boot_cb = boot_cb;
		desc.create_texture_cb = gfx_create_texture;
		desc.update_texture_cb = gfx_update_texture;
		desc.destroy_texture_cb = gfx_destroy_texture;
		desc.dbg_keys.break_keycode = ImGui::GetKeyIndex(ImGuiKey_Space);
		desc.dbg_keys.break_name = "F5";
		desc.dbg_keys.continue_keycode = ImGui::GetKeyIndex(ImGuiKey_F5);
		desc.dbg_keys.continue_name = "F5";
		desc.dbg_keys.step_over_keycode = ImGui::GetKeyIndex(ImGuiKey_F6);
		desc.dbg_keys.step_over_name = "F6";
		desc.dbg_keys.step_into_keycode = ImGui::GetKeyIndex(ImGuiKey_F7);
		desc.dbg_keys.step_into_name = "F7";
		desc.dbg_keys.toggle_breakpoint_keycode = ImGui::GetKeyIndex(ImGuiKey_F9);
		desc.dbg_keys.toggle_breakpoint_name = "F9";
		ui_cpc_init(&UICpc, &desc);
	}

	GraphicsViewer.pEmu = this;
	InitGraphicsViewer(GraphicsViewer);

	CpcViewer.Init(this);

	bInitialised = true;

	return true;
}

void FCpcEmu::Shutdown()
{
}

void FCpcEmu::DrawFileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::BeginMenu("New Game from Snapshot File"))
		{
			for (int gameNo = 0; gameNo < GamesList.GetNoGames(); gameNo++)
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

		if (ImGui::BeginMenu("Open Game"))
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

void FCpcEmu::DrawSystemMenu()
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

void FCpcEmu::DrawHardwareMenu()
{
	if (ImGui::BeginMenu("Hardware"))
	{
		ImGui::MenuItem("Memory Map", 0, &UICpc.memmap.open);
		ImGui::MenuItem("Keyboard Matrix", 0, &UICpc.kbd.open);
		ImGui::MenuItem("Audio Output", 0, &UICpc.audio.open);
		ImGui::MenuItem("Z80 CPU", 0, &UICpc.cpu.open);
		ImGui::EndMenu();
	}
}

void FCpcEmu::DrawOptionsMenu()
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

void FCpcEmu::DrawToolsMenu()
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

void FCpcEmu::DrawWindowsMenu()
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

void FCpcEmu::DrawDebugMenu()
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

// This can be overriden in the derived class if the platform code wants to insert it's own menu.
// This might be OTT?
void FCpcEmu::DrawMenus()
{
	DrawFileMenu();
	DrawSystemMenu();
	DrawHardwareMenu();
	DrawOptionsMenu();
	DrawToolsMenu();
	DrawWindowsMenu();
	DrawDebugMenu();
}

void FCpcEmu::DrawMainMenu(double timeMS)
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

	ExportAsmGui();
}

void FCpcEmu::ExportAsmGui()
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

void FCpcEmu::Tick()
{
	ExecThisFrame = ui_cpc_before_exec(&UICpc);

	if (ExecThisFrame)
	{
		const float frameTime = std::min(1000000.0f / ImGui::GetIO().Framerate, 32000.0f) * ExecSpeedScale;
		//const float frameTime = min(1000000.0f / 50, 32000.0f) * ExecSpeedScale;
		const uint32_t microSeconds = std::max(static_cast<uint32_t>(frameTime), uint32_t(1));

		// TODO: Start frame method in analyser
		CodeAnalysis.FrameTrace.clear();
		
		cpc_exec(&CpcEmuState, microSeconds);
		
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);
	}
	
	DrawDockingView();
}

void FCpcEmu::DrawUI()
{
	ui_cpc_t* pCPCUI = &UICpc;
	const double timeMS = 1000.0f / ImGui::GetIO().Framerate;
	
	if(ExecThisFrame)
		ui_cpc_after_exec(pCPCUI);

	const int instructionsThisFrame = (int)CodeAnalysis.FrameTrace.size();
	static int maxInst = 0;
	maxInst = std::max(maxInst, instructionsThisFrame);

	// Draw the main menu
	DrawMainMenu(timeMS);

	/*if (pCPCUI->memmap.open)
	{
		UpdateMemmap(pCPCUI);
	}*/

	// call the Chips UI functions
	ui_audio_draw(&pCPCUI->audio, pCPCUI->cpc->sample_pos);
	ui_z80_draw(&pCPCUI->cpu);
	ui_ay38910_draw(&pCPCUI->psg);
	ui_kbd_draw(&pCPCUI->kbd);
	ui_memmap_draw(&pCPCUI->memmap);

	if (ImGui::Begin("CPC View"))
	{
		CpcViewer.Draw();
	}
	ImGui::End();

	DrawGraphicsViewer(GraphicsViewer);
}

bool FCpcEmu::DrawDockingView()
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

