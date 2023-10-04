#include "CPCScreen.h"

#include "CPCEmu.h"
#include "Debug/DebugLog.h"

void FCPCScreen::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
}

int FCPCScreen::GetScreenModeForScanline(int scanline) const
{
	if (scanline < 0 || scanline >= AM40010_DISPLAY_HEIGHT)
		return -1;

	return ScreenModePerScanline[scanline];
}

int FCPCScreen::GetScreenModeForYPos(int yPos) const
{
	return GetScreenModeForScanline(ScreenTopScanline + yPos);
}

const FPalette& FCPCScreen::GetPaletteForScanline(int scanline) const
{
	if (scanline < 0 || scanline >= AM40010_DISPLAY_HEIGHT)
		return GetCurrentPalette_Const();

	return PalettePerScanline[scanline];
}

const FPalette& FCPCScreen::GetPaletteForYPos(int yPos) const
{
	return GetPaletteForScanline(ScreenTopScanline + yPos);
}

void FCPCScreen::Tick()
{
	const am40010_crt_t& crt = pCpcEmu->CpcEmuState.ga.crt;
	const uint16_t scanlinePos = crt.v_pos;

	// todo reset these bools when loading games

	if (!bInVblank)
	{
		/*LOGINFO("VBLANK v_pos %d pos_y %d h_pos %d pos_x %d vblank %d hblank %d",
			crt.v_pos,
			crt.pos_y,
			crt.h_pos,
			crt.pos_x,
			crt.v_blank,
			crt.h_blank
		);*/

		if (crt.v_blank)
		{
			// (crt.h_pos == 0)
			{
				//if (scanlinePos == 0)	// clear scanline info on new frame
				{
					LOGINFO("VBLANK v_pos %d pos_y %d h_pos %d pos_x %d vblank %d hblank %d",
						crt.v_pos,
						crt.pos_y,
						crt.h_pos,
						crt.pos_x,
						crt.v_blank,
						crt.h_blank
					);
					bInVblank = true;
					bDrawingPixels = false;
				}
			}
		}

		if (!bDrawingPixels)
		{
			if (crt.visible)
			{
				if (pCpcEmu->CpcEmuState.ga.crtc_pins & AM40010_DE)
				{
					LOGINFO("STARTED DRAWING. v_pos %d pos_y %d h_pos %d pos_x %d vblank %d hblank %d",
						crt.v_pos,
						crt.pos_y,
						crt.h_pos,
						crt.pos_x,
						crt.v_blank,
						crt.h_blank
					);
					LOGINFO("pos_x * 16 %d pos_x * 8 %d pos_x * 4 %d",
						crt.pos_x * 16,
						crt.pos_x * 8,
						crt.pos_x * 4
					);
					bDrawingPixels = true;
					ScreenTopScanline = crt.pos_y;
					ScreenLeftEdgeOffset = crt.pos_x * 8;
				}
			}
		}
		else
		{
#if 0
			if (CpcEmuState.ga.crt.visible)
			{
				if (!(CpcEmuState.ga.crtc_pins & AM40010_DE))
				{
					LOGINFO("STOPPED DRAWING. v_pos %d pos_y %d h_pos %d pos_x %d vblank %d hblank %d",
						CpcEmuState.ga.crt.v_pos,
						CpcEmuState.ga.crt.pos_y,
						CpcEmuState.ga.crt.h_pos,
						CpcEmuState.ga.crt.pos_x,
						CpcEmuState.ga.crt.v_blank,
						CpcEmuState.ga.crt.h_blank
					);
					//bDrawingPixels = true;
				}
			}
#endif
		}
	}
	else
	{
		if (!crt.v_blank)
		{
			bInVblank = false;
		}
	}

	
	// Store the screen mode per scanline.
	// Shame to do this here. Would be nice to have a horizontal blank callback
	const am40010_t ga = pCpcEmu->CpcEmuState.ga;
	const int curScanline = ga.crt.pos_y;
	if (LastScanline != curScanline)
	{
		const uint8_t screenMode = ga.video.mode;
		ScreenModePerScanline[curScanline] = screenMode;

		FPalette& palette = PalettePerScanline[curScanline];
		for (int i = 0; i < palette.GetColourCount(); i++)
			palette.SetColour(i, ga.hw_colors[ga.regs.ink[i]]);

		LastScanline = ga.crt.pos_y;
	}
}

// https://gist.github.com/neuro-sys/eeb7a323b27a9d8ad891b41144916946#registers
uint16_t FCPCScreen::GetScreenAddrStart() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint16_t dispStart = (crtc.start_addr_hi << 8) | crtc.start_addr_lo;
	// Bits 12 & 13 hold the page/bank index. It can be one of the 4 16k physical memory banks.
	const uint16_t pageIndex = (dispStart >> 12) & 0x3; 
	const uint16_t baseAddr = pageIndex * 0x4000;
	const uint16_t offset = (dispStart & 0x3ff) << 1; // 1024 positions. bits 0..9
	return baseAddr + offset;
}

bool FCPCScreen::IsScrolled() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint16_t dispStart = (crtc.start_addr_hi << 8) | crtc.start_addr_lo;
	// Bits 0 - 9 hold the scroll offset.
	return (dispStart & 0x3ff);
}

uint16_t FCPCScreen::GetScreenAddrEnd() const
{
	// todo: I don't think this is correct.
	// it doesnt deal with the fact screen memory wraps around.
	// for example, if screen start is $c200 it will wrap back around to $c000 and 
	// not to the start of memory: $0 - $20.
	// does this function even make sense when the screen is scrolled?
	return GetScreenAddrStart() + GetScreenMemSize() - 1;
}

// Usually screen mem size is 16k but it's possible to be set to 32k if both bits are set.
uint16_t FCPCScreen::GetScreenMemSize() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint16_t dispSize = (crtc.start_addr_hi >> 2) & 0x3;
	return dispSize == 0x3 ? 0x8000 : 0x4000;
}

uint16_t FCPCScreen::GetNextScreenAddr(uint16_t addr) const
{
	return 0;
#if 0
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const bool bIsScreenScrolled = crtc.start_addr_lo > 0 || crtc.start_addr_hi != 48;

	// Get the offset from the start of the "bank". 
	// The bank being one of the 4 16k physical memory locations : [$0, $4000, $8000, $c000]
	const uint16_t offsetFromBankStart = addr & 0x3fff;

	if (bIsScreenScrolled)
	{
		// Deal with the fact that, when the screen is HW scrolled,
		// the bytes on a single pixel line may not be contiguous.
		// For example the next byte after $c7ff is not $c800 but $c000 (2048 bytes back).
		// I didn't fully get my head around this logic so this code was written
		// with some educated guesses and trial and error. It may not be correct.

		uint16_t scrAddrOffsetFromBankStart = scrAddrStart & 0x3fff;
		if (offsetFromBankStart % 2048 < scrAddrOffsetFromBankStart)
			addr -= 2048;
	}
#endif
}

// values are in screen mode 1 coordinate system
// should we pass in screen mode?
bool FCPCScreen::GetScreenMemoryAddress(int x, int y, uint16_t& addr) const
{
	// sam todo. return false if out of range
	//if (x < 0 || x>255 || y < 0 || y> 191)
	//	return false;
	 
	// todo: deal with all r12 values. some don't work, like 47. 
	
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	//const bool bIsScreenScrolled = crtc.start_addr_lo > 0 || crtc.start_addr_hi != 48;
	const uint8_t charHeight = crtc.max_scanline_addr + 1;
	const uint8_t bytesPerScrLine = crtc.h_displayed * 2;
	const uint16_t yCharIndex = y / charHeight; // which character row are we in?
	const uint16_t charLine = (y % charHeight); // which line are we in of the character square [0-charHeight]?
	const uint16_t scrLineStartOffset = (yCharIndex * bytesPerScrLine) + (charLine * 2048);
	const uint16_t offset = scrLineStartOffset + (x / 4);
	const uint16_t scrAddrStart = GetScreenAddrStart();
	
	addr = scrAddrStart + offset;

	// Get the offset from the start of the "bank". 
	// The bank being one of the 4 16k physical memory locations : [$0, $4000, $8000, $c000]
	const uint16_t offsetFromBankStart = addr & 0x3fff;
	
 	if (IsScrolled())
	{
		// Deal with the fact that, when the screen is HW scrolled,
		// the bytes on a single pixel line may not be contiguous.
		// For example the next byte after $c7ff is not $c800 but $c000 (2048 bytes back).
		// I didn't fully get my head around this logic so this code was written
		// with some educated guesses and trial and error. It may not be correct.

		uint16_t scrAddrOffsetFromBankStart = scrAddrStart & 0x3fff;
		if (offsetFromBankStart % 2048 < scrAddrOffsetFromBankStart)
			addr -= 2048; 
	}

	return true;
}

/*
	Screen ram is 16k.
	It is split into 8 sections, 2048 bytes apart.

	[Screen eighth 0. 2048 bytes]
	[Screen eighth 1. 2048 bytes]
	...
	[Screen eighth 7. 2048 bytes]

	Each of those sections hold a single screen line for each character row of the screen.
	First section holds the first line of each character. Second section holds the second line of each character. Etc..

	Each screen eigth holds all the contiguous bytes for a character row line followed by the next character row line.
	The bytes for row 0 will be followed by row 1, 2.. etc.

	[Pixel line bytes for character row 0]
	[Pixel line bytes for character row 1]
	...
	[Pixel line bytes for character row n]

	*n depends on how many character rows are set in the CRTC register
*/

// should we pass in screen mode?
// currently, it returns in mode 1 coords. 
bool FCPCScreen::GetScreenAddressCoords(uint16_t addr, int& x, int& y) const
{
	const uint16_t startAddr = GetScreenAddrStart();
	if (addr < startAddr || addr >= GetScreenAddrEnd()) // todo: fix this logic if the screen is scrolled
		return false;

	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	if (crtc.h_displayed == 0)
		return false;

	const uint16_t totOffset = addr - startAddr; // Get byte offset into screen ram
	const uint8_t charLine = totOffset / 2048; // Which line of the character are we [0-charHeight]
	const uint8_t bytesPerLine = crtc.h_displayed * 2; // How many bytes in a single screen line?
	const uint16_t charLineOffset = totOffset % 2048; // What is our offset into the char line area? 
	const uint16_t charRowIndex = charLineOffset / bytesPerLine; // Which row are we on?

	x = (charLineOffset % bytesPerLine) * 4;

	const uint8_t charHeight = crtc.max_scanline_addr + 1;
	y = (charRowIndex * charHeight) + charLine;

	return true;
}

// Given a byte containing multiple pixels, decode the colour index for a specified pixel.
// For screen mode 0 pixel index can be [0-1]. For mode 1 pixel index can be [0-3]
// This returns a colour index into the 32 hardware colours. It does not return an RGB value.
int GetHWColourIndexForPixel(uint8_t val, int pixelIndex, int scrMode)
{
	int colourIndex = -1;

	if (scrMode == 0)
	{
		if (pixelIndex == 0)
			colourIndex = (val & 0x80 ? 1 : 0) | (val & 0x8 ? 2 : 0) | (val & 0x20 ? 4 : 0) | (val & 0x2 ? 8 : 0);
		else if (pixelIndex == 1)
			colourIndex = (val & 0x40 ? 1 : 0) | (val & 0x4 ? 2 : 0) | (val & 0x10 ? 4 : 0) | (val & 0x1 ? 8 : 0);
	}
	else
	{
		switch (pixelIndex)
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
	}
	return colourIndex;
}