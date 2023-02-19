#include <cstdint>

#define CHIPS_IMPL
#include <imgui.h>

//typedef void (*z80dasm_output_t)(char c, void* user_data);

#include "CPCEmu.h"

#include <ImGuiSupport/ImGuiTexture.h>
#include <sokol_audio.h>

#include "cpc-roms.h"

#if SPECCY
#include "GlobalConfig.h"
#include "GameData.h"
#include "GameViewers/GameViewer.h"
#include "GameViewers/StarquakeViewer.h"
#include "GameViewers/MiscGameViewers.h"
#include "Viewers/SpectrumViewer.h"
#include "Viewers/GraphicsViewer.h"
#include "Viewers/BreakpointViewer.h"
#include "Viewers/OverviewViewer.h"
#include "Util/FileUtil.h"

#include "ui/ui_dbg.h"
#include "MemoryHandlers.h"
#include "CodeAnalyser/CodeAnalyser.h"
#include "CodeAnalyser/UI/CodeAnalyserUI.h"

#include "zx-roms.h"
#include <algorithm>
#include "Exporters/SkoolkitExporter.h"
#include "Importers/SkoolkitImporter.h"
#include "Debug/DebugLog.h"
#include "Debug/ImGuiLog.h"
#include <cassert>
#include <Util/Misc.h>

#include "SpectrumConstants.h"

#include "Exporters/SkoolFileInfo.h"
#include "Exporters/AssemblerExport.h"
#include "Exporters/JsonExport.h"
#include "CodeAnalyser/UI/CharacterMapViewer.h"
#include "GameConfig.h"
#include "App.h"


#define ENABLE_RZX 0
#define SAVE_ROM_JSON 0

#define ENABLE_CAPTURES 0
const int kCaptureTrapId = 0xffff;

const char* kGlobalConfigFilename = "GlobalConfig.json";
const char* kRomInfo48JsonFile = "RomInfo.json";
const char* kRomInfo128JsonFile = "RomInfo128.json";
const std::string kAppTitle = "CPC Analyser";
#endif //#if SPECCY

// Memory access functions

uint8_t* MemGetPtr(cpc_t* zx, int layer, uint16_t addr)
{
#if SPECCY
	if (layer == 0)
	{
		/* ZX128 ROM, RAM 5, RAM 2, RAM 0 */
		if (addr < 0x4000)
			return &zx->rom[0][addr];
		else if (addr < 0x8000)
			return &zx->ram[5][addr - 0x4000];
		else if (addr < 0xC000)
			return &zx->ram[2][addr - 0x8000];
		else
			return &zx->ram[0][addr - 0xC000];
	}
	else if (layer == 1)
	{
		/* 48K ROM, RAM 1 */
		if (addr < 0x4000)
			return &zx->rom[1][addr];
		else if (addr >= 0xC000)
			return &zx->ram[1][addr - 0xC000];
	}
	else if (layer < 8)
	{
		if (addr >= 0xC000)
			return &zx->ram[layer][addr - 0xC000];
	}
#endif //#if SPECCY
	/* fallthrough: unmapped memory */
	return 0;
}

uint8_t MemReadFunc(int layer, uint16_t addr, void* user_data)
{
#if SPECCY
	assert(user_data);
	zx_t* zx = (zx_t*)user_data;
	if ((layer == 0) || (ZX_TYPE_48K == zx->type))
	{
		/* CPU visible layer */
		return mem_rd(&zx->mem, addr);
	}
	else
	{
		uint8_t* ptr = MemGetPtr(zx, layer - 1, addr);
		if (ptr)
			return *ptr;
		else
			return 0xFF;
	}
#endif //#if SPECCY
	return 0xFF;
}

void MemWriteFunc(int layer, uint16_t addr, uint8_t data, void* user_data)
{
#if SPECCY
	assert(user_data);
	zx_t* zx = (zx_t*)user_data;
	if ((layer == 0) || (ZX_TYPE_48K == zx->type)) 
	{
		mem_wr(&zx->mem, addr, data);
	}
	else 
	{
		uint8_t* ptr = MemGetPtr(zx, layer - 1, addr);
		if (ptr) 
		{
			*ptr = data;
		}
	}
#endif //#if SPECCY
}

uint8_t		FCPCEmu::ReadByte(uint16_t address) const
{
	return MemReadFunc(CurrentLayer, address, const_cast<cpc_t *>(&CPCEmuState));

}
uint16_t	FCPCEmu::ReadWord(uint16_t address) const 
{
	return ReadByte(address) | (ReadByte(address + 1) << 8);
}

const uint8_t* FCPCEmu::GetMemPtr(uint16_t address) const 
{
#if SPECCY
	if (CPCEmuState.type == CPC_TYPE_464)
	{
		const int bank = address >> 14;
		const int bankAddr = address & 0x3fff;

		if (bank == 0)
			return &CPCEmuState.rom[0][bankAddr];
		else
			return &CPCEmuState.ram[bank - 1][bankAddr];
	}
	else
	{
		return MemGetPtr(const_cast<cpc_t*>(&CPCEmuState), CurrentLayer, address);
	}
#endif //#if SPECCY
	return 0;
}


void FCPCEmu::WriteByte(uint16_t address, uint8_t value)
{
	MemWriteFunc(CurrentLayer, address, value, &CPCEmuState);
}


uint16_t	FCPCEmu::GetPC(void) 
{
	return z80_pc(&CPCEmuState.cpu);
} 

uint16_t	FCPCEmu::GetSP(void)
{
	return z80_sp(&CPCEmuState.cpu);
}

void* FCPCEmu::GetCPUEmulator(void)
{
	return &CPCEmuState.cpu;
}

bool FCPCEmu::IsAddressBreakpointed(uint16_t addr)
{
	for (int i = 0; i < UICPC.dbg.dbg.num_breakpoints; i++) 
	{
		if (UICPC.dbg.dbg.breakpoints[i].addr == addr)
			return true;
	}

	return false;
}

bool FCPCEmu::ToggleExecBreakpointAtAddress(uint16_t addr)
{
	int index = _ui_dbg_bp_find(&UICPC.dbg, UI_DBG_BREAKTYPE_EXEC, addr);
	if (index >= 0) 
	{
		/* breakpoint already exists, remove */
		_ui_dbg_bp_del(&UICPC.dbg, index);
		return false;
	}
	else 
	{
		/* breakpoint doesn't exist, add a new one */
		return _ui_dbg_bp_add_exec(&UICPC.dbg, true, addr);
	}
}

bool FCPCEmu::ToggleDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize)
{
	const int type = dataSize == 1 ? UI_DBG_BREAKTYPE_BYTE : UI_DBG_BREAKTYPE_WORD;
	int index = _ui_dbg_bp_find(&UICPC.dbg, type, addr);
	if (index >= 0)
	{
		// breakpoint already exists, remove 
		_ui_dbg_bp_del(&UICPC.dbg, index);
		return false;
	}
	else
	{
		// breakpoint doesn't exist, add a new one 
		if (UICPC.dbg.dbg.num_breakpoints < UI_DBG_MAX_BREAKPOINTS)
		{
			ui_dbg_breakpoint_t* bp = &UICPC.dbg.dbg.breakpoints[UICPC.dbg.dbg.num_breakpoints++];
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

void FCPCEmu::Break(void)
{
	_ui_dbg_break(&UICPC.dbg);
}

void FCPCEmu::Continue(void) 
{
	_ui_dbg_continue(&UICPC.dbg);
}

void FCPCEmu::StepOver(void)
{
	_ui_dbg_step_over(&UICPC.dbg);
}

void FCPCEmu::StepInto(void)
{
	_ui_dbg_step_into(&UICPC.dbg);
}

void FCPCEmu::StepFrame()
{
	_ui_dbg_continue(&UICPC.dbg);
	bStepToNextFrame = true;
}

void FCPCEmu::StepScreenWrite()
{
	_ui_dbg_continue(&UICPC.dbg);
	bStepToNextScreenWrite = true;
}

void FCPCEmu::GraphicsViewerSetView(uint16_t address, int charWidth)
{
#if SPECCY
	GraphicsViewerGoToAddress(address);
	GraphicsViewerSetCharWidth(charWidth);
#endif //#if SPECCY
}

bool	FCPCEmu::ShouldExecThisFrame(void) const
{
	return ExecThisFrame;
}

bool FCPCEmu::IsStopped(void) const
{
	return UICPC.dbg.dbg.stopped;
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
	FCPCEmu* pEmu = (FCPCEmu*)user_data;
	return pEmu->TrapFunction(pc, ticks, pins);
}

// Note - you can't read register values in Trap function
// They are only written back at end of exec function
int	FCPCEmu::TrapFunction(uint16_t pc, int ticks, uint64_t pins)
{
	return 0;

#if SPECCY
	FCodeAnalysisState &state = CodeAnalysis;
	const uint16_t addr = Z80_GET_ADDR(pins);
	const bool bMemAccess = !!((pins & Z80_CTRL_MASK) & Z80_MREQ);
	const bool bWrite = (pins & Z80_CTRL_MASK) == (Z80_MREQ | Z80_WR);
	const bool irq = (pins & Z80_INT) && z80_iff1(&ZXEmuState.cpu);	

	const uint16_t nextpc = pc;
	// store program count in history
	const uint16_t prevPC = PCHistory[PCHistoryPos];
	PCHistoryPos = (PCHistoryPos + 1) % FSpectrumEmu::kPCHistorySize;
	PCHistory[PCHistoryPos] = pc;

	pc = prevPC;	// set PC to pc of instruction just executed

	if (irq)
	{
		FCPUFunctionCall callInfo;
		callInfo.CallAddr = prevPC;
		callInfo.FunctionAddr = pc;
		callInfo.ReturnAddr = prevPC;
		state.CallStack.push_back(callInfo);
		//return UI_DBG_BP_BASE_TRAPID + 255;	//hack
	}

	bool bBreak = RegisterCodeExecuted(state, pc, nextpc);
	//FCodeInfo* pCodeInfo = state.GetCodeInfoForAddress(pc);
	//pCodeInfo->FrameLastAccessed = state.CurrentFrameNo;
	// check for breakpointed code line
	if (bBreak)
		return UI_DBG_BP_BASE_TRAPID;
	
	int trapId = MemoryHandlerTrapFunction(pc, ticks, pins, this);

	// break on screen memory write
	if (bWrite && addr >= 0x4000 && addr < 0x5800)
	{
		if (bStepToNextScreenWrite)
		{
			bStepToNextScreenWrite = false;
			return UI_DBG_BP_BASE_TRAPID;
		}
	}
#if ENABLE_CAPTURES
	FLabelInfo* pLabel = state.GetLabelForAddress(pc);
	if (pLabel != nullptr)
	{
		if(pLabel->LabelType == ELabelType::Function)
			trapId = kCaptureTrapId;
	}
#endif
	// work out stack size
	const uint16_t sp = z80_sp(&ZXEmuState.cpu);	// this won't get the proper stack pos (see comment above function)
	if (sp == state.StackMin - 2 || state.StackMin == 0xffff)
		state.StackMin = sp;
	if (sp == state.StackMax + 2 || state.StackMax == 0 )
		state.StackMax = sp;

	// work out instruction count
	int iCount = 1;
	uint8_t opcode = ReadByte(pc);
	if (opcode == 0xED || opcode == 0xCB)
		iCount++;
	return trapId;
#endif // #if SPECCY
}

// Note - you can't read the cpu vars during tick
// They are only written back at end of exec function
uint64_t FCPCEmu::Z80Tick(int num, uint64_t pins)
{
#if SPECCY
	FCodeAnalysisState &state = CodeAnalysis;

	// we have to pass data to the tick through an internal state struct because the z80_t struct only gets updated after an emulation exec period
	z80_t* pCPU = (z80_t*)state.CPUInterface->GetCPUEmulator();
	const FZ80InternalState& cpuState = pCPU->internal_state;
	const uint16_t pc = cpuState.PC;	

	/* memory and IO requests */
	if (pins & Z80_MREQ) 
	{
		/* a memory request machine cycle
			FIXME: 'contended memory' accesses should inject wait states
		*/
		const uint16_t addr = Z80_GET_ADDR(pins);
		const uint8_t value = Z80_GET_DATA(pins);
		if (pins & Z80_RD)
		{
			if (cpuState.IRQ)
			{
				// TODO: read is to fetch interrupt handler address
				//LOGINFO("Interrupt Handler at: %x", value);
				const uint8_t im = z80_im(pCPU);

				if (im == 2)
				{
					const uint8_t i = z80_i(pCPU);	// I register has high byte of interrupt vector
					const uint16_t interruptVector = (i << 8) | value;
					const uint16_t interruptHandler = state.CPUInterface->ReadWord(interruptVector);
					bHasInterruptHandler = true;
					InterruptHandlerAddress = interruptHandler;
				}
			}
			else
			{
				if (state.bRegisterDataAccesses)
					RegisterDataRead(state, pc, addr);
			}
		}
		else if (pins & Z80_WR) 
		{
			if (state.bRegisterDataAccesses)
				RegisterDataWrite(state, pc, addr);

			state.SetLastWriterForAddress(addr,pc);

			// Log screen pixel writes
			if (addr >= 0x4000 && addr < 0x5800)
			{
				FrameScreenPixWrites.push_back({ addr,value, pc });
			}
			// Log screen attribute writes
			if (addr >= 0x5800 && addr < 0x5800 + 0x400)
			{
				FrameScreenAttrWrites.push_back({ addr,value, pc });
			}
			FCodeInfo *pCodeWrittenTo = state.GetCodeInfoForAddress(addr);
			if (pCodeWrittenTo != nullptr && pCodeWrittenTo->bSelfModifyingCode == false)
			{
				// TODO: record some info such as what byte was written
				pCodeWrittenTo->bSelfModifyingCode = true;
			}
		}
	}
	else if (pins & Z80_IORQ)
	{
		IOAnalysis.IOHandler(pc, pins);

		if (ZXEmuState.type == ZX_TYPE_128)
		{
			if (pins & Z80_WR)
			{
				// an IO write
				const uint8_t data = Z80_GET_DATA(pins);

				if ((pins & (Z80_A15 | Z80_A1)) == 0)
				{
					if (!ZXEmuState.memory_paging_disabled)
					{
						const int ramBank = data & 0x7;
						const int romBank = (data & (1 << 4)) ? 1 : 0;
						const int displayRamBank = (data & (1 << 3)) ? 7 : 5;

						SetROMBank(romBank);
						SetRAMBank(3, ramBank);
					}
				}
			}
		}
	}

	pins =  OldTickCB(num, pins, OldTickUserData);

	if (pins & Z80_INT)	// have we had a vblank interrupt?
	{
	}

	// RZX playback
	if (RZXManager.GetReplayMode() == EReplayMode::Playback)
	{
		if ((pins & Z80_IORQ) && (pins & Z80_RD))
		{
			uint8_t inVal = 0;

			if (RZXManager.GetInput(inVal))
			{
				Z80_SET_DATA(pins, (uint64_t)inVal);
			}
		}
	}
#endif //#if SPECCY

	return pins;
}

static uint64_t Z80TickThunk(int num, uint64_t pins, void* user_data)
{
	FCPCEmu* pEmu = (FCPCEmu*)user_data;
	return pEmu->Z80Tick(num, pins);
}

bool FCPCEmu::Init(/*const FSpectrumConfig& config*/)
{
	// setup pixel buffer
	// todo: make this smaller?
	const size_t pixelBufferSize = 1024 * 312 * 4;
	FrameBuffer = new unsigned char[pixelBufferSize * 2];

	// setup texture
	Texture = ImGui_CreateTextureRGBA(static_cast<unsigned char*>(FrameBuffer), 1024, 312);

	cpc_type_t type = CPC_TYPE_464;
	cpc_joystick_type_t joy_type = CPC_JOYSTICK_NONE;

	cpc_desc_t desc;
	memset(&desc, 0, sizeof(cpc_desc_t));
	desc.type = type;
	desc.user_data = this;
	desc.joystick_type = joy_type;
	desc.pixel_buffer = FrameBuffer;
	desc.pixel_buffer_size = pixelBufferSize;
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

	cpc_init(&CPCEmuState, &desc);

	// Clear UI
	memset(&UICPC, 0, sizeof(ui_cpc_t));

	// Trap callback needs to be set before we create the UI
	z80_trap_cb(&CPCEmuState.cpu, CPCTrapCallback, this);

	// Setup out tick callback
	OldTickCB = CPCEmuState.cpu.tick_cb;
	OldTickUserData = CPCEmuState.cpu.user_data;
	CPCEmuState.cpu.tick_cb = Z80TickThunk;
	CPCEmuState.cpu.user_data = this;

	//ui_init(zxui_draw);
	{
		ui_cpc_desc_t desc = { 0 };
		desc.cpc = &CPCEmuState;
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
		ui_cpc_init(&UICPC, &desc);
	}
#if SPECCY
	SetWindowTitle(kAppTitle.c_str());
	SetWindowIcon("SALOGO.png");

	// Initialise Emulator
	LoadGlobalConfig(kGlobalConfigFilename);
	FGlobalConfig& globalConfig = GetGlobalConfig();
	SetNumberDisplayMode(globalConfig.NumberDisplayMode);
	CodeAnalysis.Config.bShowOpcodeValues = globalConfig.bShowOpcodeValues;

	// setup pixel buffer
	const size_t pixelBufferSize = 320 * 256 * 4;
	FrameBuffer = new unsigned char[pixelBufferSize * 2];

	// setup texture
	Texture = ImGui_CreateTextureRGBA(static_cast<unsigned char*>(FrameBuffer), 320, 256);
	// setup emu
	zx_type_t type = config.Model == ESpectrumModel::Spectrum128K ? ZX_TYPE_128 : ZX_TYPE_48K;
	zx_joystick_type_t joy_type = ZX_JOYSTICKTYPE_NONE;

	zx_desc_t desc;
	memset(&desc, 0, sizeof(zx_desc_t));
	desc.type = type;
	desc.user_data = this;
	desc.joystick_type = joy_type;
	desc.pixel_buffer = FrameBuffer;
	desc.pixel_buffer_size = pixelBufferSize;
	desc.audio_cb = PushAudio;	// our audio callback
	desc.audio_sample_rate = saudio_sample_rate();
	desc.rom_zx48k = dump_amstrad_zx48k_bin;
	desc.rom_zx48k_size = sizeof(dump_amstrad_zx48k_bin);
	desc.rom_zx128_0 = dump_amstrad_zx128k_0_bin;
	desc.rom_zx128_0_size = sizeof(dump_amstrad_zx128k_0_bin);
	desc.rom_zx128_1 = dump_amstrad_zx128k_1_bin;
	desc.rom_zx128_1_size = sizeof(dump_amstrad_zx128k_1_bin);

	zx_init(&ZXEmuState, &desc);

	GamesList.Init(this);
	GamesList.EnumerateGames(globalConfig.SnapshotFolder.c_str());

	RZXManager.Init(this);
	RZXGamesList.Init(this);
	RZXGamesList.EnumerateGames(globalConfig.RZXFolder.c_str());

	// Clear UI
	memset(&UICPC, 0, sizeof(ui_zx_t));

	// Trap callback needs to be set before we create the UI
	z80_trap_cb(&ZXEmuState.cpu, ZXSpectrumTrapCallback, this);

	// Setup out tick callback
	OldTickCB = ZXEmuState.cpu.tick_cb;
	OldTickUserData = ZXEmuState.cpu.user_data;
	ZXEmuState.cpu.tick_cb = Z80TickThunk;
	ZXEmuState.cpu.user_data = this;

	//ui_init(zxui_draw);
	{
		ui_zx_desc_t desc = { 0 };
		desc.zx = &ZXEmuState;
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
		ui_zx_init(&UICPC, &desc);
	}

	// additional debugger config
	//pUI->UICPC.dbg.ui.open = true;
	//UICPC.dbg.break_cb = UIEvalBreakpoint;
	
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
	SpectrumViewer.Init(this);
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
		std::string romJsonFName = kRomInfo48JsonFile;

		if (config.Model == ESpectrumModel::Spectrum128K)
			romJsonFName = kRomInfo128JsonFile;

		InitialiseCodeAnalysis(CodeAnalysis, this);

		if (FileExists(romJsonFName.c_str()))
			ImportAnalysisJson(CodeAnalysis, romJsonFName.c_str());
	}

#endif //#if SPECCY
	bInitialised = true;
	return true;
}

void FCPCEmu::Shutdown()
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
#endif //#if SPECCY
}

void FCPCEmu::Tick()
{
	ExecThisFrame = ui_cpc_before_exec(&UICPC);

	if (ExecThisFrame)
	{
		const float frameTime = std::min(1000000.0f / ImGui::GetIO().Framerate, 32000.0f) * ExecSpeedScale;
		//const float frameTime = min(1000000.0f / 50, 32000.0f) * ExecSpeedScale;
		const uint32_t microSeconds = std::max(static_cast<uint32_t>(frameTime), uint32_t(1));

		// TODO: Start frame method in analyser
		CodeAnalysis.FrameTrace.clear();
		
		cpc_exec(&CPCEmuState, microSeconds);
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);
	}
	
	DrawDockingView();

#if SPECCY
	SpectrumViewer.Tick();

	ExecThisFrame = ui_zx_before_exec(&UICPC);

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
		while (UICPC.dbg.dbg.z80->trap_id != kCaptureTrapId && ticks_executed < ticks_to_run)
		{
			ticks_executed += z80_exec(&ZXEmuState.cpu, ticks_to_run - ticks_executed);

			if (UICPC.dbg.dbg.z80->trap_id == kCaptureTrapId)
			{
				const uint16_t PC = GetPC();
				FMachineState* pMachineState = CodeAnalysis.GetMachineState(PC);
				if (pMachineState == nullptr)
				{
					pMachineState = AllocateMachineState(CodeAnalysis);
					CodeAnalysis.SetMachineStateForAddress(PC, pMachineState);
				}

				CaptureMachineState(pMachineState, this);
				UICPC.dbg.dbg.z80->trap_id = 0;
				_ui_dbg_continue(&UICPC.dbg);
			}
		}
		clk_ticks_executed(&ZXEmuState.clk, ticks_executed);
		kbd_update(&ZXEmuState.kbd);
#else
		zx_exec(&ZXEmuState, microSeconds);
#endif
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);

		FrameTraceViewer.CaptureFrame();
		FrameScreenPixWrites.clear();
		FrameScreenAttrWrites.clear();

		if (bStepToNextFrame)
		{
			_ui_dbg_break(&UICPC.dbg);
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
			bStepToNextFrame = false;
		}

		
		// on debug break send code analyser to address
		else if (UICPC.dbg.dbg.z80->trap_id >= UI_DBG_STEP_TRAPID)
		{
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
		}
	}

	UpdateCharacterSets(CodeAnalysis);

	// Draw UI
	DrawDockingView();
#endif //#if SPECCY
}


void FCPCEmu::DrawUI()
{
	ui_cpc_t* pCPCUI = &UICPC;
	const double timeMS = 1000.0f / ImGui::GetIO().Framerate;
	
	if(ExecThisFrame)
		ui_cpc_after_exec(pCPCUI);

	const int instructionsThisFrame = (int)CodeAnalysis.FrameTrace.size();
	static int maxInst = 0;
	maxInst = std::max(maxInst, instructionsThisFrame);

	DrawMainMenu(timeMS);
	/*if (pZXUI->memmap.open)
	{
		UpdateMemmap(pZXUI);
	}*/

	// call the Chips UI functions
	ui_audio_draw(&pCPCUI->audio, pCPCUI->cpc->sample_pos);
	ui_z80_draw(&pCPCUI->cpu);
	ui_ay38910_draw(&pCPCUI->psg);
	ui_kbd_draw(&pCPCUI->kbd);
	ui_memmap_draw(&pCPCUI->memmap);

	if (ImGui::Begin("CPC View"))
	{
		//SpectrumViewer.Draw();
	}
	ImGui::End();

#if SPECCY
	ui_zx_t* pZXUI = &UICPC;
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
#endif //#if SPECCY
}

void FCPCEmu::DrawMainMenu(double timeMS)
{
	ui_cpc_t* pCPCUI = &UICPC;
	assert(pCPCUI && pCPCUI->cpc && pCPCUI->boot_cb);
		
	bool bExportAsm = false;

	if (ImGui::BeginMainMenuBar()) 
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::BeginMenu("New Game from Snapshot File"))
			{
				/*for(int gameNo=0;gameNo<GamesList.GetNoGames();gameNo++)
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
				}*/

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
		}
		
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
				cpc_reset(pCPCUI->cpc);
				ui_dbg_reset(&pCPCUI->dbg);
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
		if (ImGui::BeginMenu("Hardware")) 
		{
			ImGui::MenuItem("Memory Map", 0, &pCPCUI->memmap.open);
			ImGui::MenuItem("Keyboard Matrix", 0, &pCPCUI->kbd.open);
			ImGui::MenuItem("Audio Output", 0, &pCPCUI->audio.open);
			ImGui::MenuItem("Z80 CPU", 0, &pCPCUI->cpu.open);
			ImGui::EndMenu();
		}
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
				ImGui::MenuItem("Save Snapshot with game", 0, &pActiveGame->pConfig->WriteSnapshot);

#ifndef NDEBUG
			ImGui::MenuItem("ImGui Demo", 0, &bShowImGuiDemo);
			ImGui::MenuItem("ImPlot Demo", 0, &bShowImPlotDemo);
#endif // NDEBUG
			ImGui::EndMenu();*/
		}
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
		if (ImGui::BeginMenu("Debug")) 
		{
			//ImGui::MenuItem("CPU Debugger", 0, &pZXUI->dbg.ui.open);
			//ImGui::MenuItem("Breakpoints", 0, &pZXUI->dbg.ui.show_breakpoints);
			ImGui::MenuItem("Memory Heatmap", 0, &pCPCUI->dbg.ui.show_heatmap);
			if (ImGui::BeginMenu("Memory Editor")) 
			{
				ImGui::MenuItem("Window #1", 0, &pCPCUI->memedit[0].open);
				ImGui::MenuItem("Window #2", 0, &pCPCUI->memedit[1].open);
				ImGui::MenuItem("Window #3", 0, &pCPCUI->memedit[2].open);
				ImGui::MenuItem("Window #4", 0, &pCPCUI->memedit[3].open);
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
		if (pCPCUI->dbg.dbg.stopped) 
			ImGui::Text("emu: stopped");
		else 
			ImGui::Text("emu: %.2fms", timeMS);

		ImGui::EndMainMenuBar();
	}

	/*if (bExportAsm)
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
	}*/
}

bool FCPCEmu::DrawDockingView()
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