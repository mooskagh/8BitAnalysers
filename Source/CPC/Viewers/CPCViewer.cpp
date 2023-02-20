#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
//#include "../CPCConstants.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>
//#include "GraphicsViewer.h"
//#include "../GlobalConfig.h"

#include <Util/Misc.h>


void FCPCViewer::Init(FCPCEmu* pEmu)
{
	pCPCEmu = pEmu;
}

void FCPCViewer::Draw()
{
	ImGui::Image(pCPCEmu->Texture, ImVec2(1024, 312));		

	ImGui::SliderFloat("Speed Scale", &pCPCEmu->ExecSpeedScale, 0.0f, 2.0f);
	//ImGui::SameLine();
	if (ImGui::Button("Reset"))
		pCPCEmu->ExecSpeedScale = 1.0f;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
}

void FCPCViewer::Tick(void)
{
}

