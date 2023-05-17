#include "IOAnalysis.h"

#include "CodeAnalyser/UI/CodeAnalyserUI.h"
#include "CpcEmu.h"

#include <chips/z80.h>
#include "chips/am40010.h"

#include "imgui.h"

std::map< CpcIODevice, const char*> g_DeviceNames = 
{
	{CpcIODevice::Keyboard, "Keyboard"},
	{CpcIODevice::BorderColour, "BorderColour"},
#if SPECCY
	{SpeccyIODevice::Ear, "Ear"},
	{SpeccyIODevice::Mic, "Mic"},
	{SpeccyIODevice::Beeper, "Beeper"},
	{SpeccyIODevice::KempstonJoystick, "KempstonJoystick"},
	{SpeccyIODevice::MemoryBank, "Memory Bank Switch"},
	{SpeccyIODevice::SoundChip, "Sound Chip (AY)"},
#endif
	{CpcIODevice::Unknown, "Unknown"},
};

void FIOAnalysis::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
}

// sam get rid of this hack
#define _AM40010_GET_DATA(p) ((uint8_t)((p&0xFF0000ULL)>>16))

void FIOAnalysis::IOHandler(uint16_t pc, uint64_t pins)
{
	 const FAddressRef PCaddrRef = pCpcEmu->CodeAnalysis.AddressRefFromPhysicalAddress(pc);

	 CpcIODevice writeDevice = CpcIODevice::None;

	 if ((pins & Z80_IORQ) && (pins & (Z80_RD | Z80_WR))) 
	 {
		 if ((pins & Z80_A11) == 0)
		 {
			 uint64_t ppi_pins = (pins & Z80_PIN_MASK) | I8255_CS;
			 if (pins & Z80_A9) { ppi_pins |= I8255_A1; }
			 if (pins & Z80_A8) { ppi_pins |= I8255_A0; }
			 if (pins & Z80_RD) { ppi_pins |= I8255_RD; }
			 if (pins & Z80_WR) { ppi_pins |= I8255_WR; }

			 //pins = i8255_iorq(&sys->ppi, ppi_pins) & Z80_PIN_MASK;
			 if (ppi_pins & I8255_CS)
			 {
				 //const uint8_t data = _i8255_read(ppi, pins);
				 if (ppi_pins & I8255_RD)
				 {
					 switch (ppi_pins & (I8255_A0 | I8255_A1))
					 {
					 case 0: /* read from port A */
					 {
						 //data = _i8255_in_a(ppi);
						 i8255_t & ppi = pCpcEmu->CpcEmuState.ppi;
						 if ((ppi.control & I8255_CTRL_A) == I8255_CTRL_A_OUTPUT)
						 {
							 //data = ppi->output[I8255_PORT_A];
						 }
						 else
						 {
							 //data = ppi->in_cb(I8255_PORT_A, ppi->user_data);

							 uint64_t ay_pins = 0;
							 uint8_t ay_ctrl = ppi.output[I8255_PORT_C];
							 if (ay_ctrl & (1 << 7)) ay_pins |= AY38910_BDIR;
							 if (ay_ctrl & (1 << 6)) ay_pins |= AY38910_BC1;
							 //uint8_t ay_data = sys->ppi.output[I8255_PORT_A];
							 //AY38910_SET_DATA(ay_pins, ay_data);
							 //ay_pins = ay38910_iorq(&sys->psg, ay_pins);*/

							 const ay38910_t* ay = &pCpcEmu->CpcEmuState.psg;

							 if (ay_pins & (AY38910_BDIR | AY38910_BC1))
							 {
								 if (ay_pins & AY38910_BDIR)
								 {
								 }
								 else
								 {
									 if (ay->addr < AY38910_NUM_REGISTERS)
									 {
										 if (ay->addr == AY38910_REG_IO_PORT_A)
										 {
											 if ((ay->enable & (1 << 6)) == 0)
											 {
												 if (ay->in_cb)
												 {
													 // here
													 writeDevice = CpcIODevice::Keyboard;
													 //ay->port_a = ay->in_cb(AY38910_PORT_A, ay->user_data);
												 }
												 else
												 {
													 //ay->port_a = 0xFF;
												 }
											 }
										 }
										 else if (ay->addr == AY38910_REG_IO_PORT_B)
										 {
										 }
									 }
								 }
							 }
						 }

						 break;
					 }
					 case I8255_A0: /* read from port B */
						 //data = _i8255_in_b(ppi);
						 break;
					 case I8255_A1: /* read from port C */
						 //data = _i8255_in_c(ppi);
						 break;
					 case (I8255_A0 | I8255_A1): /* read control word */
						 //data = ppi->control;
						 break;
					 }

					 /* read from PPI */
					 /*const uint8_t data = _i8255_read(ppi, ppi_pins);
					 I8255_SET_DATA(pins, data);*/
				 }
				 else if (ppi_pins & I8255_WR)
				 {
					 /* write to PPI */
					 const uint8_t data = I8255_GET_DATA(ppi_pins);
					 //pins = _i8255_write(ppi, pins, data);
					 switch (ppi_pins & (I8255_A0 | I8255_A1))
					 {
						 case 0: /* write to port A */
							 /*if ((ppi->control & I8255_CTRL_A) == I8255_CTRL_A_OUTPUT) 
							 {
								 pins = _i8255_out_a(ppi, pins, data);
							 }*/
							 break;
						 case I8255_A0: /* write to port B */
							 /*if ((ppi->control & I8255_CTRL_B) == I8255_CTRL_B_OUTPUT) 
							 {
								 pins = _i8255_out_b(ppi, pins, data);
							 }*/
							 break;
						 case I8255_A1: /* write to port C */
							 //ppi->output[I8255_PORT_C] = data;
							 //pins = _i8255_out_c(ppi, pins);
							 
							 const uint8_t ay_ctrl = pCpcEmu->CpcEmuState.ppi.output[I8255_PORT_C] & ((1 << 7) | (1 << 6));
							 if (ay_ctrl) 
							 {
								 uint64_t ay_pins = 0;
								 if (ay_ctrl & (1 << 7)) { ay_pins |= AY38910_BDIR; }
								 if (ay_ctrl & (1 << 6)) { ay_pins |= AY38910_BC1; }
								 //const uint8_t ay_data = sys->ppi.output[I8255_PORT_A];
								 //AY38910_SET_DATA(ay_pins, ay_data);
								 //ay38910_iorq(&sys->psg, ay_pins);
								 if (ay_pins & (AY38910_BDIR | AY38910_BC1))
								 {
									 const ay38910_t* ay = &pCpcEmu->CpcEmuState.psg;
									 if (ay_pins & AY38910_BDIR)
									 {

									 }
									 else
									 {
										 if (ay->addr < AY38910_NUM_REGISTERS)
										 {
											 if (ay->addr == AY38910_REG_IO_PORT_A)
											 {
												 if ((ay->enable & (1 << 6)) == 0)
												 {
													 if (ay->in_cb)
													 {
														 writeDevice = CpcIODevice::Keyboard;

														 //>>> ay->port_a = ay->in_cb(AY38910_PORT_A, ay->user_data);
													 }
													 else
													 {
														 //ay->port_a = 0xFF;
													 }
												 }
											 }
											 else if (ay->addr == AY38910_REG_IO_PORT_B)
											 {
												 /*if ((ay->enable & (1 << 7)) == 0)
												 {
													 if (ay->in_cb) {
														 ay->port_b = ay->in_cb(AY38910_PORT_B, ay->user_data);
													 }
													 else {
														 ay->port_b = 0xFF;
													 }
												 }*/
											 }
										 }
									 }
								 }
							 }
							 break;
					 }
				 }
			 }
		 }
	 }

	 if (writeDevice != CpcIODevice::None)
	 {
		 FIOAccess& ioDevice = IODeviceAcceses[(int)writeDevice];
		 ioDevice.Callers.RegisterAccess(PCaddrRef);
		 ioDevice.WriteCount++;
		 ioDevice.FrameReadCount++;
	 }
}

void FIOAnalysis::DrawUI()
{
	FCodeAnalysisState& state = pCpcEmu->CodeAnalysis;
	FCodeAnalysisViewState& viewState = state.GetFocussedViewState();
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginChild("DrawIOAnalysisGUIChild1", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.25f, 0), false, window_flags);
	FIOAccess *pSelectedIOAccess = nullptr;
	static CpcIODevice selectedDevice = CpcIODevice::None;

	for (int i = 0; i < (int)CpcIODevice::Count; i++)
	{
		FIOAccess &ioAccess = IODeviceAcceses[i];
		const CpcIODevice device = (CpcIODevice)i;

		const bool bSelected = (int)selectedDevice == i;

		if (ImGui::Selectable(g_DeviceNames[device], bSelected))
		{
			selectedDevice = device;
		}

		if(bSelected)
			pSelectedIOAccess = &ioAccess;

	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Handler details
	ImGui::BeginChild("DrawIOAnalysisGUIChild2", ImVec2(0, 0), false, window_flags);
	if (pSelectedIOAccess != nullptr)
	{
		FIOAccess &ioAccess = *pSelectedIOAccess;

		ImGui::Text("Reads %d (frame %d)", ioAccess.ReadCount, ioAccess.FrameReadCount);
		ImGui::Text("Writes %d (frame %d)", ioAccess.WriteCount, ioAccess.FrameWriteCount);

		ImGui::Text("Callers");
		for (const auto &accessPC : ioAccess.Callers.GetReferences())
		{
			ImGui::PushID(accessPC.Val);
			ShowCodeAccessorActivity(state, accessPC);
			ImGui::Text("   ");
			ImGui::SameLine();
			DrawCodeAddress(state, viewState, accessPC);
			ImGui::PopID();
		}
	}

	// reset for frame
	for (int i = 0; i < (int)CpcIODevice::Count; i++)
	{
		IODeviceAcceses[i].FrameReadCount = 0;
		IODeviceAcceses[i].FrameWriteCount = 0;
	}

	ImGui::EndChild();

}










