#pragma once

//#include "imgui.h"
#include <cstdint>
#include <map>
#include <string>

#include <CodeAnalyser/CodeAnalyserTypes.h>

#include <CodeAnalyser/UI/GraphicsViewer.h>
//#include "SpriteViewer.h"

class FCpcEmu;
struct FGame;



class FCPCGraphicsViewer : public FGraphicsViewer
{
public:
	FCPCGraphicsViewer()
	{
		ScreenWidth = 256;
		ScreenHeight = 192;
	}

	void	DrawScreenViewer(void) override;
private:
	void	UpdateScreenPixelImage(void);
public:
	//std::map<std::string, FUISpriteList>	SpriteLists;
	//std::string				SelectedSpriteList;
};

