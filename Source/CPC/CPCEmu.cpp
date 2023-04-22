#include <cstdint>

#define CHIPS_IMPL
#include <imgui.h>

typedef void (*z80dasm_output_t)(char c, void* user_data);

void DasmOutputU8(uint8_t val, z80dasm_output_t out_cb, void* user_data);
void DasmOutputU16(uint16_t val, z80dasm_output_t out_cb, void* user_data);
void DasmOutputD8(int8_t val, z80dasm_output_t out_cb, void* user_data);


#define _STR_U8(u8) DasmOutputU8((uint8_t)(u8),out_cb,user_data);
#define _STR_U16(u16) DasmOutputU16((uint16_t)(u16),out_cb,user_data);
#define _STR_D8(d8) DasmOutputD8((int8_t)(d8),out_cb,user_data);

#include "CPCEmu.h"

#include "GlobalConfig.h"
#include "GameConfig.h"
#include "Util/FileUtil.h"
#include "CodeAnalyser/UI/CodeAnalyserUI.h"
#include <CodeAnalyser/CodeAnalysisState.h>
#include "CodeAnalyser/CodeAnalysisJson.h"
#include "Debug/DebugLog.h"

#include "App.h"
#include "Viewers/BreakpointViewer.h"

#include <sokol_audio.h>
#include "cpc-roms.h"

#include <ImGuiSupport/ImGuiTexture.h>

const std::string kAppTitle = "CPC Analyser";

const char* kGlobalConfigFilename = "GlobalConfig.json";

void StoreRegisters_Z80(FCodeAnalysisState& state);

/* output an unsigned 8-bit value as hex string */
void DasmOutputU8(uint8_t val, z80dasm_output_t out_cb, void* user_data) 
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if(pNumberOutput)
		pNumberOutput->OutputU8(val, out_cb);
	
}

/* output an unsigned 16-bit value as hex string */
void DasmOutputU16(uint16_t val, z80dasm_output_t out_cb, void* user_data) 
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if (pNumberOutput)
		pNumberOutput->OutputU16(val, out_cb);
}

/* output a signed 8-bit offset as hex string */
void DasmOutputD8(int8_t val, z80dasm_output_t out_cb, void* user_data) 
{
	IDasmNumberOutput* pNumberOutput = GetNumberOutput();
	if (pNumberOutput)
		pNumberOutput->OutputD8(val, out_cb);
}

#if 0
// sam. this was taken from  _ui_cpc_memptr()
// do we need to replace it somehow?
// Memory access functions
uint8_t* MemGetPtr(cpc_t* cpc, int layer, uint16_t addr)
{
	if (layer == 1) // GA
	{
		uint8_t* ram = &cpc->ram[0][0];
		return ram + addr;
	}
	else if (layer == 2) // ROMS
	{
		if (addr < 0x4000)
		{
			return &cpc->rom_os[addr];
		}
		else if (addr >= 0xC000)
		{
			return &cpc->rom_basic[addr - 0xC000];
		}
		else
		{
			return 0;
		}
	}
	else if (layer == 3) // AMSDOS
	{
		if ((CPC_TYPE_6128 == cpc->type) && (addr >= 0xC000))
		{
			return &cpc->rom_amsdos[addr - 0xC000];
		}
		else
		{
			return 0;
		}
	}
	else
	{
		/* one of the 7 RAM layers */
		const int ram_config_index = (CPC_TYPE_6128 == cpc->type) ? (cpc->ga.ram_config & 7) : 0;
		const int ram_bank = layer - _UI_CPC_MEMLAYER_RAM0;
		bool ram_mapped = false;
		for (int i = 0; i < 4; i++)
		{
			if (ram_bank == _ui_cpc_ram_config[ram_config_index][i])
			{
				const uint16_t start = 0x4000 * i;
				const uint32_t end = start + 0x4000;
				ram_mapped = true;
				if ((addr >= start) && (addr < end))
				{
					return &cpc->ram[ram_bank][addr - start];
				}
			}
		}
		if (!ram_mapped && (CPC_TYPE_6128 != cpc->type))
		{
			/* if the RAM bank is not currently mapped to a CPU visible address,
					just use start address zero, this will throw off disassemblers
					though
			*/
			if (addr < 0x4000)
			{
				return &cpc->ram[ram_bank][addr];
			}
		}
	}
	/* fallthrough: address isn't mapped to physical RAM */
	return 0;
}

uint8_t MemReadFunc(int layer, uint16_t addr, void* user_data)
{
	cpc_t* cpc = (cpc_t*) user_data;
    //cpc_t* cpc = ui_cpc->cpc;
    if (layer == _UI_CPC_MEMLAYER_CPU) {
        /* CPU mapped RAM layer */
        return mem_rd(&cpc->mem, addr);
    }
    else {
        uint8_t* ptr = _ui_cpc_memptr(cpc, layer, addr);
        if (ptr) {
            return *ptr;
        }
        else {
            return 0xFF;
        }
    }
}

void MemWriteFunc(int layer, uint16_t addr, uint8_t data, void* user_data)
{
	cpc_t* cpc = (cpc_t*)user_data;
	if (layer == 0) 
	{
		mem_wr(&cpc->mem, addr, data);
	}
	else 
	{
		uint8_t* ptr = MemGetPtr(cpc, layer, addr);
		if (ptr) 
		{
			*ptr = data;
		}
	}
}
#endif


uint8_t	FCpcEmu::ReadByte(uint16_t address) const
{
	return mem_rd(const_cast<mem_t*>(&CpcEmuState.mem), address);
	//return MemReadFunc(CurrentLayer, address, const_cast<cpc_t *>(&CpcEmuState));

}
uint16_t FCpcEmu::ReadWord(uint16_t address) const 
{
	return ReadByte(address) | (ReadByte(address + 1) << 8);
}

const uint8_t* FCpcEmu::GetMemPtr(uint16_t address) const 
{
	const int bank = address >> 14;
	const int bankAddr = address & 0x3fff;

	return &CpcEmuState.ram[bank][bankAddr];

	// sam todo figure this out
#if SPECCY
	if (CpcEmuState.type == CPC_TYPE_464)
	{
		// this logic is from the Speccy so wont work on the cpc
	
		const int bank = address >> 14;
		const int bankAddr = address & 0x3fff;

		if (bank == 0)
			return &CpcEmuState.rom[0][bankAddr];
		else
			return &CpcEmuState.ram[bank - 1][bankAddr];
	}
	else
	{
		const uint8_t memConfig = CpcEmuState.last_mem_config; // last mem config doesn't exist on cpc

		if (address < 0x4000)
			return &ZXEmuState.rom[(memConfig & (1 << 4)) ? 1 : 0][address];
		else if (address < 0x8000)
			return &ZXEmuState.ram[5][address - 0x4000];
		else if (address < 0xC000)
			return &ZXEmuState.ram[2][address - 0x8000];
		else
			return &ZXEmuState.ram[memConfig & 7][address - 0xC000];
	}
#endif
}

void FCpcEmu::WriteByte(uint16_t address, uint8_t value)
{
	mem_wr(&CpcEmuState.mem, address, value);

	//MemWriteFunc(CurrentLayer, address, value, &CpcEmuState);
}

uint16_t FCpcEmu::GetPC(void) 
{
	return z80_pc(&CpcEmuState.cpu);
} 

uint16_t FCpcEmu::GetSP(void)
{
	return z80_sp(&CpcEmuState.cpu);
}

void* FCpcEmu::GetCPUEmulator(void) const
{
	return (void*)&CpcEmuState.cpu;
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

bool FCpcEmu::SetExecBreakpointAtAddress(uint16_t addr, bool bSet)
{
	const bool bAlreadySet = IsAddressBreakpointed(addr);
	if (bAlreadySet == bSet)
		return false;

	if (bSet)
	{
		_ui_dbg_bp_add_exec(&UICpc.dbg, true, addr);
	}
	else
	{
		const int index = _ui_dbg_bp_find(&UICpc.dbg, UI_DBG_BREAKTYPE_EXEC, addr);
		/* breakpoint already exists, remove */
		assert(index >= 0);
		_ui_dbg_bp_del(&UICpc.dbg, index);
	}

	return true;
}

bool FCpcEmu::SetDataBreakpointAtAddress(uint16_t addr, uint16_t dataSize, bool bSet)
{
	const bool bAlreadySet = IsAddressBreakpointed(addr);
	if (bAlreadySet == bSet)
		return false;
	const int type = dataSize == 1 ? UI_DBG_BREAKTYPE_BYTE : UI_DBG_BREAKTYPE_WORD;

	if (bSet)
	{
		// breakpoint doesn't exist, add a new one 
		if (UICpc.dbg.dbg.num_breakpoints < UI_DBG_MAX_BREAKPOINTS)
		{
			ui_dbg_breakpoint_t* bp = &UICpc.dbg.dbg.breakpoints[UICpc.dbg.dbg.num_breakpoints++];
			bp->type = type;
			bp->cond = UI_DBG_BREAKCOND_NONEQUAL;
			bp->addr = addr;
			bp->val = type == UI_DBG_BREAKTYPE_BYTE ? ReadByte(addr) : ReadWord(addr);
			bp->enabled = true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		int index = _ui_dbg_bp_find(&UICpc.dbg, type, addr);
		assert(index >= 0);
		_ui_dbg_bp_del(&UICpc.dbg, index);
	}

	return true;
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

#if 0
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
	FCpcEmu* pEmu = (FCpcEmu*)user_data;
	if(GetGlobalConfig().bEnableAudio)
		saudio_push(samples, num_samples);
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
	FCodeAnalysisState &state = CodeAnalysis;
	const uint16_t addr = Z80_GET_ADDR(pins);
	const bool bMemAccess = !!((pins & Z80_CTRL_MASK) & Z80_MREQ);
	const bool bWrite = (pins & Z80_CTRL_MASK) == (Z80_MREQ | Z80_WR);
	const bool irq = (pins & Z80_INT) && z80_iff1(&CpcEmuState.cpu);	

	const uint16_t nextpc = pc;
	// store program count in history
	const uint16_t prevPC = PCHistory[PCHistoryPos];
	PCHistoryPos = (PCHistoryPos + 1) % FCpcEmu::kPCHistorySize;
	PCHistory[PCHistoryPos] = pc;

	pc = prevPC;	// set PC to pc of instruction just executed

	if (irq)
	{
		FCPUFunctionCall callInfo;
		callInfo.CallAddr = state.AddressRefFromPhysicalAddress(pc);
		callInfo.FunctionAddr = state.AddressRefFromPhysicalAddress(pc);
		callInfo.ReturnAddr = state.AddressRefFromPhysicalAddress(pc);
		state.CallStack.push_back(callInfo);
		//return UI_DBG_BP_BASE_TRAPID + 255;	//hack
	}

	const bool bBreak = RegisterCodeExecuted(state, pc, nextpc);
	//FCodeInfo* pCodeInfo = state.GetCodeInfoForAddress(pc);
	//pCodeInfo->FrameLastAccessed = state.CurrentFrameNo;
	// check for breakpointed code line
	if (bBreak)
		return UI_DBG_BP_BASE_TRAPID;
	
	int trapId = MemoryHandlerTrapFunction(pc, ticks, pins, this);

	// break on screen memory write
	if (bWrite && addr >= GetScreenAddrStart() && addr <= GetScreenAddrEnd())
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
	const uint16_t sp = z80_sp(&CpcEmuState.cpu);	// this won't get the proper stack pos (see comment above function)
	if (sp == state.StackMin - 2 || state.StackMin == 0xffff)
		state.StackMin = sp;
	if (sp == state.StackMax + 2 || state.StackMax == 0 )
		state.StackMax = sp;

	// work out instruction count
	int iCount = 1;
	uint8_t opcode = ReadByte(pc);
	if (opcode == 0xED || opcode == 0xCB)
		iCount++;

	// Store the screen mode per scanline.
	// Shame to do this here. Would be nice to have a horizontal blank callback
	const int curScanline = CpcEmuState.ga.crt.pos_y;
	if (LastScanline != curScanline)
	{
		const uint8_t screenMode = CpcEmuState.ga.video.mode;
		ScreenModePerScanline[curScanline] = screenMode;
		LastScanline = CpcEmuState.ga.crt.pos_y;
	}
	return trapId;
}

// Note - you can't read the cpu vars during tick
// They are only written back at end of exec function
uint64_t FCpcEmu::Z80Tick(int num, uint64_t pins)
{
	FCodeAnalysisState& state = CodeAnalysis;

	// we have to pass data to the tick through an internal state struct because the z80_t struct only gets updated after an emulation exec period
	z80_t* pCPU = (z80_t*)state.CPUInterface->GetCPUEmulator();
	const FZ80InternalState& cpuState = pCPU->internal_state;
	const uint16_t pc = cpuState.PC;

	/* memory and IO requests */
	if (pins & Z80_MREQ) 
	{
		const uint16_t addr = Z80_GET_ADDR(pins);
		const uint8_t value = Z80_GET_DATA(pins);
		if (pins & Z80_RD) 
		{
			if (cpuState.IRQ)
			{
				// todo interrupt handler?
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
				RegisterDataWrite(state, pc, addr, value);
			const FAddressRef addrRef = state.AddressRefFromPhysicalAddress(addr);
			const FAddressRef pcAddrRef = state.AddressRefFromPhysicalAddress(pc);
			state.SetLastWriterForAddress(addr, pcAddrRef);


			// Log screen pixel writes
			if (addr >= GetScreenAddrStart() && addr <= GetScreenAddrEnd())
			{
				FrameScreenPixWrites.push_back({ addrRef,value, pcAddrRef });
			}
			
			FCodeInfo* pCodeWrittenTo = state.GetCodeInfoForAddress(addr);
			if (pCodeWrittenTo != nullptr && pCodeWrittenTo->bSelfModifyingCode == false)
			{
				// TODO: record some info such as what byte was written
				pCodeWrittenTo->bSelfModifyingCode = true;
			}
		}
	}

	// Memory gets remapped here
	pins =  OldTickCB(num, pins, OldTickUserData);

	if (pins & Z80_IORQ)
	{
		// sam todo
		// look at _cpc_bankswitch()?
#if SPECCY	
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
#endif
	}

	return pins;
}

static uint64_t Z80TickThunk(int num, uint64_t pins, void* user_data)
{
	FCpcEmu* pEmu = (FCpcEmu*)user_data;
	return pEmu->Z80Tick(num, pins);
}

// sam todo get working for cpc
// for cpc we probably want something like this
// slot: 0 = lo, 1 = hi
// need to be able to page rom in and out 
/*void FCpcEmu::SetROMBank(int slot, int bankNo)
{
}*/

// Bank is ROM bank 0 or 1
// this is always slot 0
void FCpcEmu::SetROMBank(int bankNo)
{
	const int16_t bankId = ROMBanks[bankNo];
	if (CurROMBank == bankId)
		return;
	// Unmap old bank
	CodeAnalysis.UnMapBank(CurROMBank, 0);
	CodeAnalysis.MapBank(bankId, 0);
	CurROMBank = bankId;
}

// sam todo get working for cpc

// Slot is physical 16K memory region (0-3) 
// Bank is a 16K CPC RAM bank (0-7)
void FCpcEmu::SetRAMBank(int slot, int bankNo)
{
	const int16_t bankId = RAMBanks[bankNo];
	if (CurRAMBank[slot] == bankId)
		return;

	// Unmap old bank
	const int startPage = slot * kNoBankPages;
	CodeAnalysis.UnMapBank(CurRAMBank[slot], startPage);
	CodeAnalysis.MapBank(bankId, startPage);

	CurRAMBank[slot] = bankId;
}

bool FCpcEmu::Init(const FCpcConfig& config)
{
	const std::string memStr = config.Model == ECpcModel::CPC_6128 ? " (128K)" : " (64K)";
	SetWindowTitle((std::string(kAppTitle) + memStr).c_str());
	SetWindowIcon("CPCALogo.png");

	// setup pixel buffer
	const size_t pixelBufferSize = AM40010_DBG_DISPLAY_WIDTH * AM40010_DBG_DISPLAY_HEIGHT * 4;
	FrameBuffer = new unsigned char[pixelBufferSize * 2];

	// setup texture
	Texture = ImGui_CreateTextureRGBA(FrameBuffer, AM40010_DISPLAY_WIDTH, AM40010_DISPLAY_HEIGHT);

	// Initialise the CPC emulator
	LoadGlobalConfig(kGlobalConfigFilename);
	FGlobalConfig& globalConfig = GetGlobalConfig();
	SetNumberDisplayMode(globalConfig.NumberDisplayMode);
	CodeAnalysis.Config.bShowOpcodeValues = globalConfig.bShowOpcodeValues;
	CodeAnalysis.Config.bShowBanks = config.Model == ECpcModel::CPC_6128;
#if SPECCY
	CodeAnalysis.Config.CharacterColourLUT = FZXGraphicsView::GetColourLUT();
#endif

	// A class that deals with loading games.
	GameLoader.Init(this);
	GamesList.Init(&GameLoader);
	if (config.Model == ECpcModel::CPC_6128)
		GamesList.EnumerateGames(globalConfig.SnapshotFolder128.c_str());
	else
		GamesList.EnumerateGames(globalConfig.SnapshotFolder.c_str());
	
	// Turn caching on for the game loader. This means that if the same game is loaded more than once
	// the second time will involve no disk i/o.
	// We use this so we can quickly restore the game state after running ahead to generate the frame buffer in StartGame().
	GameLoader.SetCachingEnabled(true);

	//cpc_type_t type = CPC_TYPE_464;
	//cpc_type_t type = CPC_TYPE_6128;
	cpc_type_t type = config.Model == ECpcModel::CPC_6128 ? CPC_TYPE_6128 : CPC_TYPE_464;
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
	// Setup our tick callback
	OldTickCB = CpcEmuState.cpu.tick_cb;
	OldTickUserData = CpcEmuState.cpu.user_data;
	CpcEmuState.cpu.tick_cb = Z80TickThunk;
	CpcEmuState.cpu.user_data = this;

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

	// This is where we add the viewers we want
	Viewers.push_back(new FBreakpointViewer(this));
	//Viewers.push_back(new FOverviewViewer(this));

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

	CpcViewer.Init(this);

	CodeAnalysis.ViewState[0].Enabled = true;	// always have first view enabled

	LoadGameConfigs(this);

	// Set up code analysis
	// initialise code analysis pages
/*

	// sam todo look at _ui_cpc_update_memmap
	// 
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
	*/

	// sam todo get this working for cpc
	// 
	// create & register ROM banks
	for (int bankNo = 0; bankNo < kNoROMBanks; bankNo++)
	{
		char bankName[32];
		sprintf(bankName, "ROM %d", bankNo);
		ROMBanks[bankNo] = CodeAnalysis.CreateBank(bankName, 16, CpcEmuState.rom_os, true); // no idea if this is right or not
		CodeAnalysis.GetBank(ROMBanks[bankNo])->PrimaryMappedPage = 0;
	}

	// create & register RAM banks
	for (int bankNo = 0; bankNo < kNoRAMBanks; bankNo++)
	{
		char bankName[32];
		sprintf(bankName, "RAM %d", bankNo);
		RAMBanks[bankNo] = CodeAnalysis.CreateBank(bankName, 16, CpcEmuState.ram[bankNo], false);
		CodeAnalysis.GetBank(RAMBanks[bankNo])->PrimaryMappedPage = 48; // ?
	}

	// CreateBank(name,size in kb,r/w)
	// return bank id

	// Setup initial machine memory config
	if (config.Model == ECpcModel::CPC_464)
	{
		CodeAnalysis.GetBank(RAMBanks[0])->PrimaryMappedPage = 0;
		CodeAnalysis.GetBank(RAMBanks[1])->PrimaryMappedPage = 16;
		CodeAnalysis.GetBank(RAMBanks[2])->PrimaryMappedPage = 32;
		CodeAnalysis.GetBank(RAMBanks[3])->PrimaryMappedPage = 48;

		SetROMBank(0);
		SetRAMBank(0, 0);	// 0x0000 - 0x3fff
		SetRAMBank(1, 1);	// 0x4000 - 0x7fff
		SetRAMBank(2, 2);	// 0x8000 - 0xBfff
		SetRAMBank(3, 3);	// 0xc000 - 0xffff
	}
	else
	{
		CodeAnalysis.GetBank(RAMBanks[0])->PrimaryMappedPage = 0;
		CodeAnalysis.GetBank(RAMBanks[1])->PrimaryMappedPage = 16;
		CodeAnalysis.GetBank(RAMBanks[2])->PrimaryMappedPage = 32;
		CodeAnalysis.GetBank(RAMBanks[3])->PrimaryMappedPage = 48;

		SetROMBank(0);
		SetRAMBank(0, 0);	// 0x0000 - 0x3fff
		SetRAMBank(1, 1);	// 0x4000 - 0x7fff
		SetRAMBank(2, 2);	// 0x8000 - 0xBfff
		SetRAMBank(3, 3);	// 0xc000 - 0xffff
	}

//#endif

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
	if (bLoadedGame == false)
	{
#if SPECCY
		std::string romJsonFName = kRomInfo48JsonFile;

		if (config.Model == ESpectrumModel::Spectrum128K)
			romJsonFName = kRomInfo128JsonFile;
#endif
		CodeAnalysis.Init(this);

#if SPECCY
		if (FileExists(romJsonFName.c_str()))
			ImportAnalysisJson(CodeAnalysis, romJsonFName.c_str());
#endif
	}

	bInitialised = true;

	return true;
}

void FCpcEmu::Shutdown()
{
	SaveCurrentGameData();	// save on close

	// Save Global Config - move to function?
	FGlobalConfig& config = GetGlobalConfig();

	if (pActiveGame != nullptr)
		config.LastGame = pActiveGame->pConfig->Name;

	config.NumberDisplayMode = GetNumberDisplayMode();
	config.bShowOpcodeValues = CodeAnalysis.Config.bShowOpcodeValues;

	SaveGlobalConfig(kGlobalConfigFilename);
}

void FCpcEmu::StartGame(FGameConfig* pGameConfig)
{
	// reset systems
	MemoryAccessHandlers.clear();	// remove old memory handlers
	ResetMemoryStats(MemStats);
#if SPECCY
	FrameTraceViewer.Reset();
#endif

	const std::string memStr = CpcEmuState.type == CPC_TYPE_6128 ? " (128K)" : " (64K)";
	const std::string windowTitle = kAppTitle + " - " + pGameConfig->Name + memStr;
	SetWindowTitle(windowTitle.c_str());

	// start up game
#if SPECCY
	if (pActiveGame != nullptr)
		delete pActiveGame->pViewerData;
#endif
	delete pActiveGame;

	FGame* pNewGame = new FGame;
	pNewGame->pConfig = pGameConfig;
	pGameConfig->Cpc6128Game = CpcEmuState.type == CPC_TYPE_6128;
#if SPECCY
	pNewGame->pViewerConfig = pGameConfig->pViewerConfig;
	assert(pGameConfig->pViewerConfig != nullptr);
#endif
	GraphicsViewer.pGame = pNewGame;
	pActiveGame = pNewGame;

#if SPECCY
	pNewGame->pViewerData = pNewGame->pViewerConfig->pInitFunction(this, pGameConfig);
	GenerateSpriteListsFromConfig(GraphicsViewer, pGameConfig);
#endif
	// Initialise code analysis
	CodeAnalysis.Init(this);

	// Set options from config
	for (int i = 0; i < FCodeAnalysisState::kNoViewStates; i++)
	{
		CodeAnalysis.ViewState[i].Enabled = pGameConfig->ViewConfigs[i].bEnabled;
		CodeAnalysis.ViewState[i].GoToAddress(pGameConfig->ViewConfigs[i].ViewAddress);

	}

	const std::string root = GetGlobalConfig().WorkspaceRoot;
	const std::string dataFName = root + "GameData/" + pGameConfig->Name + ".bin";
#if SPECCY
	std::string romJsonFName = kRomInfo48JsonFile;

	if (ZXEmuState.type == ZX_TYPE_128)
		romJsonFName = root + kRomInfo128JsonFile;
#endif

	const std::string analysisJsonFName = root + "AnalysisJson/" + pGameConfig->Name + ".json";
	const std::string analysisStateFName = root + "AnalysisState/" + pGameConfig->Name + ".astate";
	const std::string saveStateFName = root + "SaveStates/" + pGameConfig->Name + ".state";
	
	ImportAnalysisJson(CodeAnalysis, analysisJsonFName.c_str());
	ImportAnalysisState(CodeAnalysis, analysisStateFName.c_str());
	
#if SPECCY
	LoadGameState(this, saveStateFName.c_str());

	if (FileExists(romJsonFName.c_str()))
		ImportAnalysisJson(CodeAnalysis, romJsonFName.c_str());

	// where do we want pokes to live?
	LoadPOKFile(*pGameConfig, std::string(GetGlobalConfig().PokesFolder + pGameConfig->Name + ".pok").c_str());
#endif

	ReAnalyseCode(CodeAnalysis);
	GenerateGlobalInfo(CodeAnalysis);
#if SPECCY
	FormatSpectrumMemory(CodeAnalysis);
#endif
	CodeAnalysis.SetAddressRangeDirty();

	// Run the cpc for long enough to generate a frame buffer, otherwise the user will be staring at a black screen.
	// sam todo: run for exactly 1 video frame. The current technique is crude and can render >1 frame, including partial frames and produce 
	// a glitch when continuing execution.
	// todo mute audio so we don't hear a frame of audio
	cpc_exec(&CpcEmuState, 48000);

	ImGui_UpdateTextureRGBA(Texture, FrameBuffer);

	// Load the game again (from memory - it should be cached) to restore the cpc state.
	const std::string snapFolder = GetGlobalConfig().SnapshotFolder;
	const std::string gameFile = snapFolder + pGameConfig->SnapshotFile;
	GamesList.LoadGame(gameFile.c_str());

	// Start in break mode so the memory will be in it's initial state. 
	// Otherwise, if we export an asm file once the game is running the memory could be in an arbitrary state.
	Break();
}

bool FCpcEmu::StartGame(const char* pGameName)
{
	for (const auto& pGameConfig : GetGameConfigs())
	{
		if (pGameConfig->Name == pGameName)
		{
			const std::string snapFolder = CpcEmuState.type == CPC_TYPE_6128 ? GetGlobalConfig().SnapshotFolder128 : GetGlobalConfig().SnapshotFolder;
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
void FCpcEmu::SaveCurrentGameData()
{
	if (pActiveGame != nullptr)
	{
		FGameConfig* pGameConfig = pActiveGame->pConfig;
		if (pGameConfig->Name.empty())
		{

		}
		else
		{
			const std::string root = GetGlobalConfig().WorkspaceRoot;
			const std::string configFName = root + "Configs/" + pGameConfig->Name + ".json";
			const std::string dataFName = root + "GameData/" + pGameConfig->Name + ".bin";
			const std::string analysisJsonFName = root + "AnalysisJson/" + pGameConfig->Name + ".json";
			const std::string analysisStateFName = root + "AnalysisState/" + pGameConfig->Name + ".astate";
			const std::string saveStateFName = root + "SaveStates/" + pGameConfig->Name + ".state";
			EnsureDirectoryExists(std::string(root + "Configs").c_str());
			EnsureDirectoryExists(std::string(root + "GameData").c_str());
			EnsureDirectoryExists(std::string(root + "AnalysisJson").c_str());
			EnsureDirectoryExists(std::string(root + "AnalysisState").c_str());
			EnsureDirectoryExists(std::string(root + "SaveStates").c_str());

			// set config values
			for (int i = 0; i < FCodeAnalysisState::kNoViewStates; i++)
			{
				const FCodeAnalysisViewState& viewState = CodeAnalysis.ViewState[i];
				FCodeAnalysisViewConfig& viewConfig = pGameConfig->ViewConfigs[i];

				viewConfig.bEnabled = viewState.Enabled;
				viewConfig.ViewAddress = viewState.GetCursorItem().IsValid() ? viewState.GetCursorItem().AddressRef : FAddressRef();
			}

			SaveGameConfigToFile(*pGameConfig, configFName.c_str());
#if SPECCY			
			// The Future
			SaveGameState(this, saveStateFName.c_str());
#endif // #if SPECCY
			ExportAnalysisJson(CodeAnalysis, analysisJsonFName.c_str());
			ExportAnalysisState(CodeAnalysis, analysisStateFName.c_str());
			//ExportGameJson(this, analysisJsonFName.c_str());
		}
	}

	// TODO: get this working?
#if	SAVE_ROM_JSON
	const std::string romJsonFName = root + kRomInfoJsonFile;
	ExportROMJson(CodeAnalysis, romJsonFName.c_str());
#endif
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
						FGameConfig *pNewConfig = CreateNewGameConfigFromSnapshot(game);
						if(pNewConfig != nullptr)
							StartGame(pNewConfig);
					}
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Open Game"))
		{
			for (const auto& pGameConfig : GetGameConfigs())
			{
				if (ImGui::MenuItem(pGameConfig->Name.c_str()))
				{
					const std::string snapFolder = CpcEmuState.type == CPC_TYPE_6128 ? GetGlobalConfig().SnapshotFolder128 : GetGlobalConfig().SnapshotFolder;
					const std::string gameFile = snapFolder + pGameConfig->SnapshotFile;

					if(GamesList.LoadGame(gameFile.c_str()))
					{
						StartGame(pGameConfig);
					}
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Save Game Data"))
		{
			SaveCurrentGameData();
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
			cpc_reset(&CpcEmuState);
			ui_dbg_reset(&UICpc.dbg);
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
		ImGui::MenuItem("MC6845 (CRTC)", 0, &UICpc.vdc.open);
		ImGui::MenuItem("AM40010 (Gate Array)", 0, &UICpc.ga.open);
		ImGui::EndMenu();
	}
}

void FCpcEmu::DrawOptionsMenu()
{
	if (ImGui::BeginMenu("Options"))
	{
		FGlobalConfig& config = GetGlobalConfig();

		if (ImGui::BeginMenu("Number Mode"))
		{
			bool bClearCode = false;
			if (ImGui::MenuItem("Decimal", 0, GetNumberDisplayMode() == ENumberDisplayMode::Decimal))
			{
				SetNumberDisplayMode(ENumberDisplayMode::Decimal);
				CodeAnalysis.SetAllBanksDirty();
				bClearCode = true;
			}
			if (ImGui::MenuItem("Hex - FEh", 0, GetNumberDisplayMode() == ENumberDisplayMode::HexAitch))
			{
				SetNumberDisplayMode(ENumberDisplayMode::HexAitch);
				CodeAnalysis.SetAllBanksDirty();
				bClearCode = true;
			}
			if (ImGui::MenuItem("Hex - $FE", 0, GetNumberDisplayMode() == ENumberDisplayMode::HexDollar))
			{
				SetNumberDisplayMode(ENumberDisplayMode::HexDollar);
				CodeAnalysis.SetAllBanksDirty();
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

		ImGui::MenuItem("Enable Audio", 0, &config.bEnableAudio);
#if SPECCY
		ImGui::MenuItem("Scan Line Indicator", 0, &config.bShowScanLineIndicator);
		ImGui::MenuItem("Edit Mode", 0, &CodeAnalysis.bAllowEditing);
		if(pActiveGame!=nullptr)
			ImGui::MenuItem("Save Snapshot with game", 0, &pActiveGame->pConfig->WriteSnapshot);
#endif
		ImGui::MenuItem("Show Opcode Values", 0, &CodeAnalysis.Config.bShowOpcodeValues);

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
		//ImGui::MenuItem("DebugLog", 0, &bShowDebugLog);
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
	CpcViewer.Tick();

	ExecThisFrame = ui_cpc_before_exec(&UICpc);

	if (ExecThisFrame)
	{
		const float frameTime = std::min(1000000.0f / ImGui::GetIO().Framerate, 32000.0f) * ExecSpeedScale;
		const uint32_t microSeconds = std::max(static_cast<uint32_t>(frameTime), uint32_t(1));

		// TODO: Start frame method in analyser
		CodeAnalysis.FrameTrace.clear();
		
		StoreRegisters_Z80(CodeAnalysis);

		cpc_exec(&CpcEmuState, microSeconds);
		
		ImGui_UpdateTextureRGBA(Texture, FrameBuffer);

#if SPECCY
		FrameTraceViewer.CaptureFrame();
#endif
		FrameScreenPixWrites.clear();

		if (bStepToNextFrame)
		{
			_ui_dbg_break(&UICpc.dbg);
			CodeAnalysis.GetFocussedViewState().GoToAddress({ CodeAnalysis.GetBankFromAddress(GetPC()), GetPC() });
			bStepToNextFrame = false;
		}


		// on debug break send code analyser to address
		else if (UICpc.dbg.dbg.z80->trap_id >= UI_DBG_STEP_TRAPID)
		{
			CodeAnalysis.GetFocussedViewState().GoToAddress({ CodeAnalysis.GetBankFromAddress(GetPC()), GetPC() });
		}
	}
	
#if SPECCY
	UpdateCharacterSets(CodeAnalysis);
#endif
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

	if (pCPCUI->memmap.open)
	{
		// sam todo work out why SpectrumEmu.cpp has it's own version of UpdateMemmap()
		// why didn't Mark call _ui_zx_update_memmap()?
		//UpdateMemmap(pCPCUI);
	}

	if (pCPCUI->memmap.open) 
	{
		_ui_cpc_update_memmap(pCPCUI);
	}

	// call the Chips UI functions
	ui_audio_draw(&pCPCUI->audio, pCPCUI->cpc->sample_pos);
	ui_z80_draw(&pCPCUI->cpu);
	ui_ay38910_draw(&pCPCUI->psg);
	ui_kbd_draw(&pCPCUI->kbd);
	ui_memmap_draw(&pCPCUI->memmap);
	ui_am40010_draw(&pCPCUI->ga);
	ui_mc6845_draw(&pCPCUI->vdc);

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

	if (ImGui::Begin("CPC View"))
	{
		CpcViewer.Draw();
	}
	ImGui::End();

	if (ImGui::Begin("Call Stack"))
	{
#if SPECCY
		DrawStackInfo(CodeAnalysis);
#endif
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
	/*if (ImGui::Begin("Frame Trace"))
	{
		FrameTraceViewer.Draw();
	}
	ImGui::End();*/

	DrawGraphicsViewer(GraphicsViewer);

	// Code analysis views
	for (int codeAnalysisNo = 0; codeAnalysisNo < FCodeAnalysisState::kNoViewStates; codeAnalysisNo++)
	{
		char name[32];
		sprintf(name, "Code Analysis %d", codeAnalysisNo + 1);
		if (CodeAnalysis.ViewState[codeAnalysisNo].Enabled)
		{
			if (ImGui::Begin(name, &CodeAnalysis.ViewState[codeAnalysisNo].Enabled))
			{
				DrawCodeAnalysisData(CodeAnalysis, codeAnalysisNo);
			}
			ImGui::End();
		}

	}
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

void FCpcConfig::ParseCommandline(int argc, char** argv)
{
	std::vector<std::string> argList;
	for (int arg = 0; arg < argc; arg++)
	{
		argList.emplace_back(argv[arg]);
	}

	auto argIt = argList.begin();
	argIt++;	// skip exe name
	while (argIt != argList.end())
	{
		if (*argIt == std::string("-128"))
		{
			Model = ECpcModel::CPC_6128;
		}
		else if (*argIt == std::string("-game"))
		{
			if (++argIt == argList.end())
			{
				LOGERROR("-game : No game specified");
				break;
			}
			SpecificGame = *++argIt;
		}

		++argIt;
	}
}

// https://gist.github.com/neuro-sys/eeb7a323b27a9d8ad891b41144916946#registers
uint16_t FCpcEmu::GetScreenAddrStart() const
{
	const uint16_t dispStart = (CpcEmuState.crtc.start_addr_hi << 8) | CpcEmuState.crtc.start_addr_lo;
	const uint16_t pageIndex = (dispStart >> 12) & 0x3; // bits 12 & 13 hold the page index
	const uint16_t baseAddr = pageIndex * 0x4000;
	const uint16_t offset = (dispStart & 0x3ff) << 1; // 1024 positions. bits 0..9
	return baseAddr + offset;
}

uint16_t FCpcEmu::GetScreenAddrEnd() const
{
	return GetScreenAddrStart() + GetScreenMemSize() - 1;
}

// Usually screen mem size is 16k but it's possible to be set to 32k if both bits are set.
uint16_t FCpcEmu::GetScreenMemSize() const
{
	const uint16_t dispSize = (CpcEmuState.crtc.start_addr_hi >> 2) & 0x3;
	return dispSize == 0x3 ? 0x8000 : 0x4000;
}

bool FCpcEmu::GetScreenMemoryAddress(int x, int y, uint16_t& addr) const
{
	// sam todo. return false if out of range
	//if (x < 0 || x>255 || y < 0 || y> 191)
	//	return false;

	//screenaddr = screenbase + (y AND 7)*&800 + int(y/8)*2*R1 + int(x/M)

	const uint8_t charHeight = CpcEmuState.crtc.max_scanline_addr + 1; 
	const uint8_t w = CpcEmuState.crtc.h_displayed * 2;
	addr = GetScreenAddrStart() + ((y / charHeight) * w) + ((y % charHeight) * 2048) + (x / 4);
	return true;
}

uint8_t GetPixelsPerByte(int screenMode)
{
	uint8_t bpp = 0;
	switch (screenMode)
	{
	case 0:
		return 2;
	case 1:
		return 4;
	case 2:
		return 8;
	case 3: // unsupported
		return 4;
	}
	return 0;
}

uint8_t GetBitsPerPixel(int screenMode)
{
	uint8_t bpp = 0;
	switch (screenMode)
	{
	case 0:
		return 4;
	case 1:
		return 2;
	case 2:
		return 1;
	case 3: // unsupported
		return 4;
	}
	return 0;
}