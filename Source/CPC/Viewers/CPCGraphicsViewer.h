#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <CodeAnalyser/CodeAnalyserTypes.h>
#include <CodeAnalyser/UI/GraphicsViewer.h>

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
	uint32_t	GetRGBValueForPixel(int yPos, int colourIndex, uint32_t heatMapCol) const;
	void		UpdateScreenPixelImage(void);
	uint16_t	GetPixelLineAddress(int yPos);
	uint16_t	GetPixelLineOffset(int yPos);

	FCpcEmu*	pCpcEmu = nullptr;
	int			CharacterHeight = 8;
};
