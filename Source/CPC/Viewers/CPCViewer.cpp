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

int CpcKeyFromImGuiKey(ImGuiKey key)
{
	int cpcKey = 0;

	if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
	{
		cpcKey = '0' + (key - ImGuiKey_0);
	}
	else if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
	{
		cpcKey = 'A' + (key - ImGuiKey_A) + 0x20;
	}
	else if (key >= ImGuiKey_Keypad1 && key <= ImGuiKey_Keypad9)
	{
		cpcKey = 0xf1 + (key - ImGuiKey_Keypad1);
	}
	else if (key == ImGuiKey_Keypad0)
	{
		cpcKey = 0xfa;
	}
	else if (key == ImGuiKey_Space)
	{
		cpcKey = ' ';
	}
	else if (key == ImGuiKey_Enter)
	{
		cpcKey = 0xd;
	}
	else if (key == ImGuiKey_Backspace)
	{	
		cpcKey = 0x1;
	}
	else if (key == ImGuiKey_Comma)
	{
		cpcKey = 0x2c;
	}
	else if (key == ImGuiKey_Tab)
	{
		cpcKey = 0x6;
	}
	else if (key == ImGuiKey_Period)
	{
		cpcKey = 0x2e;
	}
	else if (key == ImGuiKey_Semicolon)
	{
		cpcKey = 0x3a;
	}
	else if (key == ImGuiKey_CapsLock)
	{
		cpcKey = 0x7;
	}
	else if (key == ImGuiKey_Escape)
	{
		cpcKey = 0x3;
	}
	else if (key == ImGuiKey_Minus)
	{
		cpcKey = 0x2d;
	}
	else if (key == ImGuiKey_Apostrophe)
	{
		// ; semicolon
		cpcKey = 0x3b;
	}
	else if (key == ImGuiKey_Equal)
	{
		// up arrow with pound sign
		cpcKey = 0x5e;
	}
	else if (key == ImGuiKey_Delete)
	{
		// CLR
		cpcKey = 0xc;
	}
	else if (key == ImGuiKey_Insert)
	{
		// Copy
		cpcKey = 0x5;
	}
	else if (key == ImGuiKey_Slash)
	{
		// forward slash /
		cpcKey = 0x2f;
	}
	else if (key == ImGuiKey_LeftBracket)
	{
		// [
		cpcKey = 0x5b;
	}
	else if (key == ImGuiKey_RightBracket)
	{
		// ]
		cpcKey = 0x5d;
	}
	else if (key == ImGuiKey_Backslash)
	{
		// backslash '\'
		cpcKey = 0x5c;
	}
	else if (key == ImGuiKey_GraveAccent) // `
	{
		// @
		cpcKey = 0x40;
	}
	else if (key == ImGuiKey_LeftArrow)
	{
		cpcKey = 0x8;
	}
	else if (key == ImGuiKey_RightArrow)
	{
		cpcKey = 0x9;
	}
	else if (key == ImGuiKey_UpArrow)
	{
		cpcKey = 0xb;
	}
	else if (key == ImGuiKey_DownArrow)
	{
		cpcKey = 0xa;
	}
	else if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift)
	{
		cpcKey = 0xe;
	}
	else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl)
	{
		cpcKey = 0xf;
	}

	
	/*
	no way to press the following keys:
	shift 
	ctrl */

	return cpcKey;
}

void FCpcViewer::Tick(void)
{
	// Check keys - not event driven, hopefully perf isn't too bad
	for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_COUNT; key++)
	{
		if (ImGui::IsKeyPressed(key,false))
		{ 
			int cpcKey = CpcKeyFromImGuiKey(key);
			if (cpcKey != 0)
				cpc_key_down(&pCPCEmu->CpcEmuState, cpcKey);
		}
		else if (ImGui::IsKeyReleased(key))
		{
			const int cpcKey = CpcKeyFromImGuiKey(key);
			if (cpcKey != 0)
				cpc_key_up(&pCPCEmu->CpcEmuState, cpcKey);
		}
	}
}

