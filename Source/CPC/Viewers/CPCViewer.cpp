#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
//#include "../CPCConstants.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>
//#include "GraphicsViewer.h"
//#include "../GlobalConfig.h"

#include <Util/Misc.h>


void FCpcViewer::Init(FCpcEmu* pEmu)
{
	pCPCEmu = pEmu;
}

void FCpcViewer::Draw()
{
	ImGui::Image(pCPCEmu->Texture, ImVec2(AM40010_DISPLAY_WIDTH/2, AM40010_DISPLAY_HEIGHT));		

	ImGui::SliderFloat("Speed Scale", &pCPCEmu->ExecSpeedScale, 0.0f, 2.0f);
	//ImGui::SameLine();
	if (ImGui::Button("Reset"))
		pCPCEmu->ExecSpeedScale = 1.0f;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
}

void FCpcViewer::Tick(void)
{
}

