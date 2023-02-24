#include <cstdint>

#define CHIPS_IMPL
#include <imgui.h>

//typedef void (*z80dasm_output_t)(char c, void* user_data);

#include "CPCEmu.h"

#include <sokol_audio.h>
#include "cpc-roms.h"

#include <ImGuiSupport/ImGuiTexture.h>

// TODO
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

const uint8_t* FCpcEmu::GetMemPtr(uint16_t address) const 
{
	return 0;
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
	// A class that deals with loading games.
	// This gets passed to the FSystem class so the GamesList can load cpc games.
	GameLoader.Init(this);

	// Initialise the System
	FSystemParams params;
	params.AppTitle = "CPC Analyser";
	params.WindowIconName = "CPCALogo.png";
	params.ScreenBufferSize = AM40010_DBG_DISPLAY_WIDTH * AM40010_DBG_DISPLAY_HEIGHT * 4;
	params.ScreenTextureWidth = AM40010_DISPLAY_WIDTH;
	params.ScreenTextureHeight = AM40010_DISPLAY_HEIGHT;
	params.pGameLoader = &GameLoader;
	FSystem::Init(params);
	
	// Initialise the CPC emulator

	cpc_type_t type = CPC_TYPE_464;
	cpc_joystick_type_t joy_type = CPC_JOYSTICK_NONE;

	cpc_desc_t desc;
	memset(&desc, 0, sizeof(cpc_desc_t));
	desc.type = type;
	desc.user_data = this;
	desc.joystick_type = joy_type;
	desc.pixel_buffer = FrameBuffer;
	desc.pixel_buffer_size = static_cast<int>(params.ScreenBufferSize);
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

	CpcViewer.Init(this);

	bInitialised = true;

	return true;
}

void FCpcEmu::Shutdown()
{
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
		
		// do in FSystem?
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);
	}
	
	// Draw the docking view.
	FSystem::Tick();
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
	FSystem::DrawUI();

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
}

void FCpcEmu::DrawHardwareMenu()
{
	FSystem::DrawHardwareMenu();

	if (ImGui::BeginMenu("Hardware")) 
	{
		ImGui::MenuItem("Memory Map", 0, &UICpc.memmap.open);
		ImGui::MenuItem("Keyboard Matrix", 0, &UICpc.kbd.open);
		ImGui::MenuItem("Audio Output", 0, &UICpc.audio.open);
		ImGui::MenuItem("Z80 CPU", 0, &UICpc.cpu.open);
		ImGui::EndMenu();
	}
}

