#pragma once

#include <cstdint>

#include "imgui.h"
#include "Misc/InputEventHandler.h"

class FCpcEmu;
class FCodeAnalysisState;
struct FCodeAnalysisViewState;

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
	bool	OnHovered(const ImVec2& pos, int scrEdgeL, int scrTop);
	float	DrawScreenCharacter(int xChar, int yChar, int screenMode, float x, float y, int scanlineTEMP) const;

	void	CalculateScreenProperties();
private:

	FCpcEmu* pCpcEmu = nullptr;

	uint32_t* FrameBuffer;	// pixel buffer to store emu output
	ImTextureID		ScreenTexture;		// texture

	bool bWindowFocused = false;
	float TextureWidth = 0;
	float TextureHeight = 0;

	int ScreenEdgeL = 0;
	int ScreenTop = 0;
	int ScreenWidth = 0;
	int ScreenHeight = 0;
	int CharacterHeight = 0;
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

