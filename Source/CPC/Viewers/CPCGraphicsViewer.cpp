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

void FCPCGraphicsViewer::UpdateScreenPixelImage(void)
{
	FGraphicsView* pGraphicsView = pScreenView;
	const FCodeAnalysisState& state = GetCodeAnalysis();
	const int16_t bankId = Bank == -1 ? state.GetBankFromAddress(pCpcEmu->GetScreenAddrStart()) : Bank;
	const FCodeAnalysisBank* pBank = state.GetBank(bankId);

	// should we set these somewhere else?
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const int CharacterHeight = crtc.max_scanline_addr + 1;
	ScreenWidth = crtc.h_displayed * 8;
	ScreenHeight = crtc.v_displayed * CharacterHeight;

	uint16_t bankAddr = 0;
	for (int y = 0; y < ScreenHeight; y++)
	{
		//const int y0to2 = ((bankAddr >> 8) & 7);
		//const int y3to5 = ((bankAddr >> 5) & 7) << 3;
		//const int y6to7 = ((bankAddr >> 11) & 3) << 6;
		//const int yDestPos = y0to2 | y3to5 | y6to7;	// or offsets together
		const int yDestPos = 0;

		// determine dest pointer for scanline
		uint32_t* pLineAddr = pGraphicsView->GetPixelBuffer() + (yDestPos * ScreenWidth);

		// pixel line
		for (int x = 0; x < ScreenWidth / 8; x++)
		{
			const uint8_t charLine = pBank->Memory[bankAddr];
			const FCodeAnalysisPage& page = pBank->Pages[bankAddr >> 10];
			const uint32_t col = GetHeatmapColourForMemoryAddress(page, bankAddr, state.CurrentFrameNo, HeatmapThreshold);

			for (int xpix = 0; xpix < 8; xpix++)
			{
				const bool bSet = (charLine & (1 << (7 - xpix))) != 0;
				*(pLineAddr + xpix + (x * 8)) = bSet ? col : 0xff000000;
			}

			bankAddr++;
		}
	}
#if SPECCY

	uint16_t bankAddr = 0;
	for (int y = 0; y < ScreenHeight; y++)
	{
		const int y0to2 = ((bankAddr >> 8) & 7);
		const int y3to5 = ((bankAddr >> 5) & 7) << 3;
		const int y6to7 = ((bankAddr >> 11) & 3) << 6;
		const int yDestPos = y0to2 | y3to5 | y6to7;	// or offsets together

		// determine dest pointer for scanline
		uint32_t* pLineAddr = pGraphicsView->GetPixelBuffer() + (yDestPos * ScreenWidth);

		// pixel line
		for (int x = 0; x < ScreenWidth / 8; x++)
		{
			const uint8_t charLine = pBank->Memory[bankAddr];
			const FCodeAnalysisPage& page = pBank->Pages[bankAddr >> 10];
			const uint32_t col = GetHeatmapColourForMemoryAddress(page, bankAddr, state.CurrentFrameNo, HeatmapThreshold);

			for (int xpix = 0; xpix < 8; xpix++)
			{
				const bool bSet = (charLine & (1 << (7 - xpix))) != 0;
				*(pLineAddr + xpix + (x * 8)) = bSet ? col : 0xff000000;
			}

			bankAddr++;
		}
	}
#endif
}