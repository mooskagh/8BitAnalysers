#pragma once

#include "ViewerBase.h"

class FCrtcViewer : public FViewerBase
{
public:
			FCrtcViewer(FCpcEmu* pEmu) :FViewerBase(pEmu) { Name = "CRTC"; }
	bool	Init(void) override { return true; }
	void	DrawUI() override;
private:
};