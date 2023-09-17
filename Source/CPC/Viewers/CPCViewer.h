#pragma once

#include <cstdint>

#include "imgui.h"
#include "Misc/InputEventHandler.h"

class FCpcEmu;
class FCodeAnalysisState;
struct FCodeAnalysisViewState;
struct FPalette;

class FCpcViewer
{
public:
	FCpcViewer() {}

	void	Init(FCpcEmu* pEmu);
	void	Draw();
	void	Tick(void);

	const uint32_t* GetFrameBuffer() const { return FrameBuffer; }
	const ImTextureID GetScreenTexture() const { return ScreenTexture; }

private:
	bool	OnHovered(const ImVec2& pos);
	float	DrawScreenCharacter(int xChar, int yChar, float x, float y, float pixelHeight) const;
	void	CalculateScreenProperties();
	int		GetScreenModeForPixelLine(int yPos) const;
	const FPalette& GetPaletteForPixelLine(int yPos) const;

#ifndef NDEBUG
	void DrawTestScreen();
#endif
private:

	FCpcEmu* pCpcEmu = nullptr;

	uint32_t* FrameBuffer = 0;	// pixel buffer to store emu output
	ImTextureID	ScreenTexture = 0;		// texture

	bool bWindowFocused = false;
	float TextureWidth = 0;
	float TextureHeight = 0;

	// The x position of where the screen starts. Effectively this is the width of the left border.
	int ScreenEdgeL = 0;
	// The y position of where the screen starts. Effectively this is the height of the top border.
	int ScreenTop = 0;
	// The width of the screen in pixels. Note: this is currently using mode 1 coordinates. When in mode 0, you will need to divide this by 2.
	int ScreenWidth = 0;
	// The height of the screen in pixels.
	int ScreenHeight = 0;
	// The height in pixels of a cpc character "square". CPC can have non-square characters - even 1 pixel high if you wish.
	int CharacterHeight = 0;
	// Number of horizontal characters that can be displayed. Note: in mode 0 you will need to divide this by 2. This is possibly a bit poo.
	int HorizCharCount = 0;

#ifndef NDEBUG
	bool	bClickWritesToScreen = false;
#endif
	// screen inspector
	/*bool		bScreenCharSelected = false;
	uint16_t	SelectPixAddr = 0;
	uint16_t	SelectAttrAddr = 0;
	int			SelectedCharX = 0;
	int			SelectedCharY = 0;
	bool		CharDataFound = false;
	uint16_t	FoundCharDataAddress = 0;
	uint8_t		CharData[8] = {0};
	bool		bCharSearchWrap = true;*/
};
