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

#include <sokol_audio.h>
#include "cpc-roms.h"

#include "CodeAnalyser/UI/CodeAnalyserUI.h"
#include "App.h"

#include <ImGuiSupport/ImGuiTexture.h>
const std::string kAppTitle = "CPC Analyser";

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

const uint8_t* FCpcEmu::GetMemPtr(uint16_t address) const 
{
	// this logic is from the Speccy so might not work on the cpc
	// todo: deal with 464/6128 differences

	const int bank = address >> 14;
	const int bankAddr = address & 0x3fff;

	return &CpcEmuState.ram[bank][bankAddr];
}

uint8_t	FCpcEmu::ReadByte(uint16_t address) const
{
	return MemReadFunc(CurrentLayer, address, const_cast<cpc_t *>(&CpcEmuState));

}
uint16_t FCpcEmu::ReadWord(uint16_t address) const 
{
	return ReadByte(address) | (ReadByte(address + 1) << 8);
}

void* FCpcEmu::GetCPUEmulator(void)
{
	return &CpcEmuState.cpu;
}

uint16_t FCpcEmu::GetPC(void) 
{
	return z80_pc(&CpcEmuState.cpu);
} 

uint16_t FCpcEmu::GetSP(void)
{
	return z80_sp(&CpcEmuState.cpu);
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

void FCpcEmu::WriteByte(uint16_t address, uint8_t value)
{
	MemWriteFunc(CurrentLayer, address, value, &CpcEmuState);
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
				RegisterDataWrite(state, pc, addr);

			state.SetLastWriterForAddress(addr, pc);

			// Log screen pixel writes
			if (addr >= GetScreenAddrStart() && addr <= GetScreenAddrEnd())
			{
				FrameScreenPixWrites.push_back({ addr,value, pc });
			}
			
			FCodeInfo* pCodeWrittenTo = state.GetCodeInfoForAddress(addr);
			if (pCodeWrittenTo != nullptr && pCodeWrittenTo->bSelfModifyingCode == false)
			{
				// TODO: record some info such as what byte was written
				pCodeWrittenTo->bSelfModifyingCode = true;
			}
		}
		else if (pins & Z80_IORQ)
		{
			// todo
		}
	}

	pins =  OldTickCB(num, pins, OldTickUserData);
	return pins;
}

static uint64_t Z80TickThunk(int num, uint64_t pins, void* user_data)
{
	FCpcEmu* pEmu = (FCpcEmu*)user_data;
	return pEmu->Z80Tick(num, pins);
}

void CheckAddressSpaceItems(const FCodeAnalysisState& state)
{
	for (int addr = 0; addr < 1 << 16; addr++)
	{
		const FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(addr);
		assert(pDataInfo->Address == addr);
	}
}

// Bank is ROM bank 0 or 1
// this is always slot 0
void FCpcEmu::SetROMBank(int bankNo)
{
	if (ROMBank == bankNo)
		return;

	const uint16_t firstBankPage = bankNo * kNoSlotPages;

	for (int pageNo = 0; pageNo < kNoSlotPages; pageNo++)
	{
		ROMPages[firstBankPage + pageNo].ChangeAddress((pageNo * FCodeAnalysisPage::kPageSize));
		CodeAnalysis.SetCodeAnalysisRWPage(pageNo, &ROMPages[firstBankPage + pageNo], &ROMPages[firstBankPage + pageNo]);	// Read/Write
	}

	CodeAnalysis.SetMemoryRemapped();
#ifdef _DEBUG
	if (bInitialised)
		CheckAddressSpaceItems(CodeAnalysis);
#endif
}

// Slot is physical 16K memory region (0-3) 
// Bank is a 16K Spectrum RAM bank (0-7)
void FCpcEmu::SetRAMBank(int slot, int bankNo)
{
	if (RAMBanks[slot] == bankNo)
		return;
	RAMBanks[slot] = bankNo;

	const uint16_t firstSlotPage = slot * kNoSlotPages;
	const uint16_t firstBankPage = bankNo * kNoBankPages;
	uint16_t slotAddress = firstSlotPage * FCodeAnalysisPage::kPageSize;

	for (int pageNo = 0; pageNo < kNoSlotPages; pageNo++)
	{
		const int slotPageNo = firstSlotPage + pageNo;
		FCodeAnalysisPage& bankPage = RAMPages[firstBankPage + pageNo];
		bankPage.ChangeAddress(slotAddress);
		CodeAnalysis.SetCodeAnalysisRWPage(slotPageNo, &bankPage, &bankPage);	// Read/Write
		slotAddress += FCodeAnalysisPage::kPageSize;
	}

	CodeAnalysis.SetMemoryRemapped();

#ifdef _DEBUG
	if (bInitialised)
		CheckAddressSpaceItems(CodeAnalysis);
#endif

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

	//cpc_type_t type = CPC_TYPE_464;
	cpc_type_t type = CPC_TYPE_6128;
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

	GraphicsViewer.pEmu = this;
	InitGraphicsViewer(GraphicsViewer);

	CpcViewer.Init(this);

	CodeAnalysis.ViewState[0].Enabled = true;	// always have first view enabled



	// Set up code analysis
	// initialise code analysis pages
//#if SPECCY

	// todo look at _ui_cpc_update_memmap
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

	// Setup initial machine memory config
	//if (config.Model == ESpectrumModel::Spectrum48K)
	if (1)
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
//#endif

	// load the command line game if none specified then load the last game
	bool bLoadedGame = false;

#if SPECCY
	if (config.SpecificGame.empty() == false)
	{
		bLoadedGame = StartGame(config.SpecificGame.c_str());
	}
	else if (globalConfig.LastGame.empty() == false)
	{
		bLoadedGame = StartGame(globalConfig.LastGame.c_str());
	}
#endif
	// Start ROM if no game has been loaded
	if (bLoadedGame == false)
	{
#if SPECCY
		std::string romJsonFName = kRomInfo48JsonFile;

		if (config.Model == ESpectrumModel::Spectrum128K)
			romJsonFName = kRomInfo128JsonFile;
#endif
		InitialiseCodeAnalysis(CodeAnalysis, this);

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

		/*for (auto Viewer : Viewers)
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
	CpcViewer.Tick();

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

#if SPECCY
		FrameTraceViewer.CaptureFrame();
#endif
		FrameScreenPixWrites.clear();

		if (bStepToNextFrame)
		{
			_ui_dbg_break(&UICpc.dbg);
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
			bStepToNextFrame = false;
		}


		// on debug break send code analyser to address
		else if (UICpc.dbg.dbg.z80->trap_id >= UI_DBG_STEP_TRAPID)
		{
			CodeAnalyserGoToAddress(CodeAnalysis.GetFocussedViewState(), GetPC());
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

	/*if (pCPCUI->memmap.open)
	{
		UpdateMemmap(pCPCUI);
	}*/

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

	if (ImGui::Begin("CPC View"))
	{
		CpcViewer.Draw();
	}
	ImGui::End();

	if (ImGui::Begin("Trace"))
	{
		DrawTrace(CodeAnalysis);
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
	// todo. return false if out of range
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