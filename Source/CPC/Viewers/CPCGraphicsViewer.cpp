#include "CPCGraphicsViewer.h"


#include <Util/GraphicsView.h>
#include <CodeAnalyser/CodeAnalyser.h>

#include "../CPCEmu.h"
//#include "../SpectrumConstants.h"

void FCPCGraphicsViewer::Init(FCodeAnalysisState* pCodeAnalysis, FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
	FGraphicsViewer::Init(pCodeAnalysis);
}

// Amstrad CPC specific implementation
void FCPCGraphicsViewer::DrawScreenViewer()
{
	UpdateScreenPixelImage();
	pScreenView->Draw();
}

// should this return an offset from the start of screen ram?
uint16_t FCPCGraphicsViewer::GetPixelLineAddress(int yPos)
{
	const uint16_t start = pCpcEmu->GetScreenAddrStart();
	return start + GetPixelLineOffset(yPos);
}

// should this return an offset from the start of screen ram?

// get offset into screen ram for a given horizontal pixel line (scan line)
uint16_t FCPCGraphicsViewer::GetPixelLineOffset(int yPos)
{
	// todo: couldn't we use FCpcEmu::GetScreenMemoryAddress() instead?

	return ((yPos / CharacterHeight) * (ScreenWidth / 4)) + ((yPos % CharacterHeight) * 2048);
}

void FCPCGraphicsViewer::UpdateScreenPixelImage(void)
{
	const FCodeAnalysisState& state = GetCodeAnalysis();

	// todo: deal with Bank being set
	const int16_t startBankId = Bank == -1 ? state.GetBankFromAddress(pCpcEmu->GetScreenAddrStart()) : Bank;
	const int16_t endBankId = Bank == -1 ? state.GetBankFromAddress(pCpcEmu->GetScreenAddrEnd()) : Bank;

	if (startBankId != endBankId)
	{
		ImGui::Text("SCREEN MEMORY SPANS MULTIPLE BANKS. TODO");
		ImGui::Text("");
		ImGui::Text("");

		// if the default screen start address is used we will use one bank for the screen memory
		// however, it could be possible the screen ram will be spread across 2 banks.
		// todo: deal with this
		return;
	}

	const FCodeAnalysisBank* pBank = state.GetBank(startBankId);

	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	CharacterHeight = crtc.max_scanline_addr + 1;
	const int lastScreenWidth = ScreenWidth;
	const int lastScreenHeight = ScreenHeight;
	ScreenWidth = crtc.h_displayed * 8;
	ScreenHeight = crtc.v_displayed * CharacterHeight;
	
	ScreenHeight = std::min(ScreenHeight, AM40010_DISPLAY_HEIGHT);
	ScreenWidth = std::min(ScreenWidth, AM40010_DISPLAY_WIDTH >> 1);

	if (ScreenWidth != lastScreenWidth || ScreenHeight != lastScreenHeight)
	{
		// ScreenWidth can be huge: 1216 for AtomSmasher
		// temp. recreate the screen view
		delete pScreenView;
		pScreenView = new FGraphicsView(ScreenWidth, ScreenHeight);
	}

	// todo: mixed screen modes
	const int scrMode = pCpcEmu->CpcEmuState.ga.video.mode;

	for (int y = 0; y < ScreenHeight; y++)
	{
		uint16_t bankOffset = GetPixelLineOffset(y);

		uint32_t* pLineAddr = pScreenView->GetPixelBuffer() + (y * ScreenWidth);

		int numChars = ScreenWidth / 8;
		if (scrMode == 0)
			numChars = numChars / 2;

		for (int x = 0; x < numChars; x++)
		{
			// draw 8x1 pixel line
			if (scrMode == 0)
			{
				for (int byte = 0; byte < 4; byte++)
				{
					const uint8_t val = pBank->Memory[bankOffset];

					const FCodeAnalysisPage& page = pBank->Pages[bankOffset >> 10];
					const uint32_t heatMapCol = GetHeatmapColourForMemoryAddress(page, bankOffset, state.CurrentFrameNo, HeatmapThreshold);

					// todo: put this in a function? we seem to be using it a lot
					for (int pixel = 0; pixel < 2; pixel++)
					{
						int colourIndex = 0;

						switch (pixel)
						{
						case 0:
							colourIndex = (val & 0x80 ? 1 : 0) | (val & 0x8 ? 2 : 0) | (val & 0x20 ? 4 : 0) | (val & 0x2 ? 8 : 0);
							break;
						case 1:
							colourIndex = (val & 0x40 ? 1 : 0) | (val & 0x4 ? 2 : 0) | (val & 0x10 ? 4 : 0) | (val & 0x1 ? 8 : 0);
							break;
						}

						// todo: correct palette for scanline
						const ImColor colour = GetCurrentPalette_Const().GetColour(colourIndex);
						// Grayscale value is R * 0.299 + G * 0.587 + B * 0.114
						const float grayScaleValue = colour.Value.x * 0.299f + colour.Value.y * 0.587f + colour.Value.z * 0.114f;
						const ImColor grayScaleColour = ImColor(grayScaleValue, grayScaleValue, grayScaleValue);
						ImU32 finalColour = grayScaleColour;
						if (heatMapCol != 0xffffffff)
						{
							finalColour |= heatMapCol;
						}
						// double width pixel
						*(pLineAddr + (x * 8) + (byte * 2 /*2 pixels per byte*/) + pixel) = finalColour;
						//*(pLineAddr + (x * 8) + (byte * 2 /*2 pixels per byte*/) + pixel + 1) = finalColour;
					}
					bankOffset++;
					const int bankSize = pBank->GetSizeBytes();
					if (bankOffset >= bankSize)
						return;
					assert(bankOffset < bankSize);
				}
			}
			else if (scrMode == 1)
			{
				for (int byte = 0; byte < 2; byte++)
				{
					const uint8_t val = pBank->Memory[bankOffset];

					const FCodeAnalysisPage& page = pBank->Pages[bankOffset >> 10];
					const uint32_t heatMapCol = GetHeatmapColourForMemoryAddress(page, bankOffset, state.CurrentFrameNo, HeatmapThreshold);

					// todo: put this in a function? we seem to be using it a lot
					for (int pixel = 0; pixel < 4; pixel++)
					{
						int colourIndex = 0;

						switch (pixel)
						{
						case 0:
							colourIndex = (val & 0x8 ? 2 : 0) | (val & 0x80 ? 1 : 0);
							break;
						case 1:
							colourIndex = (val & 0x4 ? 2 : 0) | (val & 0x40 ? 1 : 0);
							break;
						case 2:
							colourIndex = (val & 0x2 ? 2 : 0) | (val & 0x20 ? 1 : 0);
							break;
						case 3:
							colourIndex = (val & 0x1 ? 2 : 0) | (val & 0x10 ? 1 : 0);
							break;
						}

						// todo: correct palette for scanline
						const ImColor colour = GetCurrentPalette_Const().GetColour(colourIndex);
						// Grayscale value is R * 0.299 + G * 0.587 + B * 0.114
						const float grayScaleValue = colour.Value.x * 0.299f + colour.Value.y * 0.587f + colour.Value.z * 0.114f;
						const ImColor grayScaleColour = ImColor(grayScaleValue, grayScaleValue, grayScaleValue);
						ImU32 finalColour = grayScaleColour;
						if (heatMapCol != 0xffffffff)
						{
							finalColour |= heatMapCol;
						}
						*(pLineAddr + (x * 8) + (byte * 4 /* 4 pixels per byte */) + pixel) = finalColour;
					}
					bankOffset++;
					const int bankSize = pBank->GetSizeBytes();
					// todo: this fires when loading dtc.sna
					assert(bankOffset < bankSize);
				}
			}
		}
	}
}