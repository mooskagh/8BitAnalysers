#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <CodeAnalyser/CodeAnalyserTypes.h>

#include <CodeAnalyser/UI/GraphicsViewer.h>
//#include "SpriteViewer.h"

class FCpcEmu;
struct FGame;
class FCpcEmu;

class FCPCGraphicsViewer : public FGraphicsViewer
{
public:
	FCPCGraphicsViewer()
	{
		ScreenWidth = 320;
		ScreenHeight = 200;
	}

	void	DrawScreenViewer(void) override;
	void	Init(FCodeAnalysisState* pCodeAnalysis, FCpcEmu* pEmu);

private:
	void		UpdateScreenPixelImage(void);
	uint16_t	GetPixelLineAddress(int yPos);
	uint16_t	GetPixelLineOffset(int yPos);

	FCpcEmu*	pCpcEmu = nullptr;
	int			CharacterHeight = 8;

public:
	//std::map<std::string, FUISpriteList>	SpriteLists;
	//std::string				SelectedSpriteList;

};

