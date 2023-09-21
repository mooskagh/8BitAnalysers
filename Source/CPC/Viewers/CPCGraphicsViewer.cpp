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
	// todo character height
	// todo replace 80 with screen width
	return start + GetPixelLineOffset(yPos);
}

// should this return an offset from the start of screen ram?

// get offset into screen ram for a given horizontal pixel line (scan line)
uint16_t FCPCGraphicsViewer::GetPixelLineOffset(int yPos)
{
	// todo character height
	// todo replace 80 with screen width
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
		// if the default screen start address is used we will use one bank for the screen memory
		// however, it could be possible the screen ram will be spread across 2 banks.
		// todo: deal with this
		return;
	}

	const FCodeAnalysisBank* pBank = state.GetBank(startBankId);

	/*
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	CharacterHeight = crtc.max_scanline_addr + 1;
	const int lastScreenWidth = ScreenWidth;
	const int lastScreenHeight = ScreenHeight;
	ScreenWidth = crtc.h_displayed * 8;
	ScreenHeight = crtc.v_displayed * CharacterHeight;
	if (ScreenWidth != lastScreenWidth || ScreenHeight != lastScreenHeight)
	{
		// ScreenWidth can be huge: 1216 for AtomSmasher
		// temp. recreate the screen view
		delete pScreenView;
		pScreenView = new FGraphicsView(ScreenWidth, ScreenHeight);
	}*/

	for (int y = 0; y < ScreenHeight; y++)
	{
		uint16_t bankOffset = GetPixelLineOffset(y);

		uint32_t* pLineAddr = pScreenView->GetPixelBuffer() + (y * ScreenWidth);

		for (int x = 0; x < ScreenWidth / 8; x++)
		{
			// draw 8 pixel line
			for (int byte = 0; byte < 2; byte++)
			{
				const uint8_t val = pBank->Memory[bankOffset];

				const FCodeAnalysisPage& page = pBank->Pages[bankOffset >> 10];
				const uint32_t heatMapCol = GetHeatmapColourForMemoryAddress(page, bankOffset, state.CurrentFrameNo, HeatmapThreshold);

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
					
					const ImColor colour = GetCurrentPalette_Const().GetColour(colourIndex);
					const float r = colour.Value.x;
					const float g = colour.Value.y;
					const float b = colour.Value.z;
					const float grayScaleValue = r * 0.299f + g * 0.587f + b * 0.114f;
					const ImColor grayScaleColour = ImColor(grayScaleValue, grayScaleValue, grayScaleValue);
					ImU32 finalColour = grayScaleColour;
					if (heatMapCol != 0xffffffff)
					{
						finalColour |= heatMapCol;
					}
					*(pLineAddr + (x * 8) + (byte * 4) + pixel) = finalColour;
				}
				bankOffset++;
			}
		}

		/*for (int x = 0; x < ScreenWidth / 8; x++)
		{
			// 4 pixels in mode 1
			const uint8_t pixels = pBank->Memory[bankOffset];
			const FCodeAnalysisPage& page = pBank->Pages[bankOffset >> 10];
			const uint32_t col = GetHeatmapColourForMemoryAddress(page, bankOffset, state.CurrentFrameNo, HeatmapThreshold);

			for (int xpix = 0; xpix < 8; xpix++)
			{
				const bool bSet = (charLine & (1 << (7 - xpix))) != 0;
				*(pLineAddr + xpix + (x * 8)) = bSet ? col : 0xff000000;
			}

			// todo: check we're not going off the bounds of the bank
			bankOffset++;
		}*/
	}
}