#pragma once

#include <cstdint>

#include "imgui.h"
#include "Misc/InputEventHandler.h"

class FCpcEmu;

class FCpcViewer
{
public:
	FCpcViewer() {}

	void	Init(FCpcEmu* pEmu);
	void	Draw();
	void	Tick(void);

private:
	FCpcEmu* pCpcEmu = nullptr;

	bool	bWindowFocused = false;
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