#include "CPCGraphicsViewer.h"


#include <Util/GraphicsView.h>
#include <CodeAnalyser/CodeAnalyser.h>

#include "../CPCEmu.h"

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
	const uint16_t start = pCpcEmu->Screen.GetScreenAddrStart();
	return start + GetPixelLineOffset(yPos);
}

// should this return an offset from the start of screen ram?

// get offset into screen ram for a given horizontal pixel line (scan line)
uint16_t FCPCGraphicsViewer::GetPixelLineOffset(int yPos)
{
	// todo: couldn't we use FCpcEmu::GetScreenMemoryAddress() instead?

	return ((yPos / CharacterHeight) * (ScreenWidth / 4)) + ((yPos % CharacterHeight) * 2048);
}

uint32_t FCPCGraphicsViewer::GetRGBValueForPixel(int yPos, int colourIndex, uint32_t heatMapCol) const
{
	//const ImColor colour = GetCurrentPalette_Const().GetColour(colourIndex);
	const ImColor colour = pCpcEmu->Screen.GetPaletteForYPos(yPos).GetColour(colourIndex);
	//return colour;

	// Grayscale value is R * 0.299 + G * 0.587 + B * 0.114
	const float grayScaleValue = colour.Value.x * 0.299f + colour.Value.y * 0.587f + colour.Value.z * 0.114f;
	const ImColor grayScaleColour = ImColor(grayScaleValue, grayScaleValue, grayScaleValue);
	ImU32 finalColour = grayScaleColour;
	if (heatMapCol != 0xffffffff)
	{
		finalColour |= heatMapCol;
	}
	return finalColour;
}

// todo: make sure this can display a full screen overscan image
void FCPCGraphicsViewer::UpdateScreenPixelImage(void)
{
	const FCodeAnalysisState& state = GetCodeAnalysis();

	// todo: deal with Bank being set
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const int16_t startBankId = Bank == -1 ? state.GetBankFromAddress(pCpcEmu->Screen.GetScreenAddrStart()) : Bank;
	const int16_t endBankId = Bank == -1 ? state.GetBankFromAddress(pCpcEmu->Screen.GetScreenAddrEnd()) : Bank;

	if (startBankId != endBankId)
	{
		ImGui::Text("SCREEN MEMORY SPANS MULTIPLE BANKS. CURRENTLY UNSUPORTED");
		ImGui::Text("");
		ImGui::Text("");

		// if the default screen start address is used we will use one bank for the screen memory
		// however, it could be possible the screen ram will be spread across 2 banks.
		// todo: deal with this
		return;
	}

	const FCodeAnalysisBank* pBank = state.GetBank(startBankId);

	CharacterHeight = crtc.max_scanline_addr + 1;
	
	// Not sure character height of >8 is supported?
	// I'm clamping it to 8 because it can cause out of bounds crashes.
	CharacterHeight = std::min(CharacterHeight, 8);

	const int lastScreenWidth = ScreenWidth;
	const int lastScreenHeight = ScreenHeight;
	ScreenWidth = crtc.h_displayed * 8;
	ScreenHeight = crtc.v_displayed * CharacterHeight;
	
	ScreenHeight = std::min(ScreenHeight, AM40010_DISPLAY_HEIGHT);
	ScreenWidth = std::min(ScreenWidth, AM40010_DISPLAY_WIDTH >> 1);

	if (ScreenWidth != lastScreenWidth || ScreenHeight != lastScreenHeight)
	{
		// temp. recreate the screen view
		// todo: need to create this up front. 
		delete pScreenView;
		pScreenView = new FGraphicsView(ScreenWidth, ScreenHeight);
	}

	for (int y = 0; y < ScreenHeight; y++)
	{
		const int scrMode = pCpcEmu->Screen.GetScreenModeForYPos(y);

		// I think this needs to return an address instead of an offset
		// then we can lookup a bank per screen line?
		// can a screen line cross bank boundaries?
		uint16_t bankOffset = GetPixelLineOffset(y); 
		
		// todo: check here if we have enough bytes in the bank for a pixel line

		const int bankSize = pBank->GetSizeBytes();
		assert(bankOffset < bankSize);
		if (bankOffset >= bankSize)
			return;

		// get pointer to start of line in pixel buffer
		uint32_t* pPixBufAddr = pScreenView->GetPixelBuffer() + (y * ScreenWidth);

		const int pixelsPerByte = scrMode == 0 ? 2 : 4;
		const int bytesPerLine = crtc.h_displayed * 2;
		for (int b = 0; b < bytesPerLine; b++)
		{
			const uint8_t val = pBank->Memory[bankOffset];
			const FCodeAnalysisPage& page = pBank->Pages[bankOffset >> 10];
			const uint32_t heatMapCol = GetHeatmapColourForMemoryAddress(page, bankOffset, state.CurrentFrameNo, HeatmapThreshold);

			for (int p = 0; p < pixelsPerByte; p++)
			{
				int colourIndex = GetHWColourIndexForPixel(val, p, scrMode);

				// todo: correct palette for scanline
				const ImU32 pixelColour = GetRGBValueForPixel(y, colourIndex, heatMapCol);

				*pPixBufAddr = pixelColour;
				pPixBufAddr++;
				
				if (scrMode == 0)
				{
					*pPixBufAddr = pixelColour;
					pPixBufAddr++;
				}
			}
			bankOffset++;
			assert(bankOffset < bankSize); // todo: work out why this sometimes fires
			if (bankOffset >= bankSize)
				return;
		}
	}
}
