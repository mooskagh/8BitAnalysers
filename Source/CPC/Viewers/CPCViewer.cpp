#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
//#include "../CPCConstants.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>
//#include "GraphicsViewer.h"
//#include "../GlobalConfig.h"

#include <Util/Misc.h>
#include <algorithm>

void FCpcViewer::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
}

void FCpcViewer::Draw()
{
	// test code. remove me
	/*
	const uint8_t dispStartH = crtc.start_addr_hi; // R12
	const uint8_t dispStartL = crtc.start_addr_lo; // R13
	const uint16_t wordValue = (dispStartH << 8) | dispStartL;
	const uint16_t page = (wordValue >> 12) & 0x3; // bits 12 & 13
	const uint16_t baseAddr = page * 0x4000;
	const uint16_t offset = (wordValue & 0x3ff) << 1; // 1024 positions. bits 0..9
	ImGui::Text("screen base=%x, offset=%x, scr addr=%x", baseAddr, offset, pCpcEmu->GetScreenAddrStart());*/
	
#ifndef NDEBUG
	// debug code to manually iterate through all snaps in a directory
	if (pCpcEmu->GamesList.GetNoGames())
	{
		static int gGameIndex = 0;
		bool bLoadSnap = false;
		if (ImGui::Button("Prev snap") || ImGui::IsKeyPressed(ImGuiKey_F1))
		{
			if (gGameIndex > 0)
				gGameIndex--;
			bLoadSnap = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Next snap") || ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			if (gGameIndex < pCpcEmu->GamesList.GetNoGames()-1) 
				gGameIndex++;
			bLoadSnap = true;
		}
		ImGui::SameLine();
		const FGameSnapshot& game = pCpcEmu->GamesList.GetGame(gGameIndex);
		ImGui::Text("(%d/%d) %s", gGameIndex+1, pCpcEmu->GamesList.GetNoGames(), game.DisplayName.c_str());
		if (bLoadSnap)
			pCpcEmu->GamesList.LoadGame(gGameIndex);
	}
#endif

#ifndef NDEBUG
	static bool bDrawScreenExtents = true;
	ImGui::Checkbox("Draw screen extents", &bDrawScreenExtents);
	static bool bClickWritesToScreen = true;
	ImGui::Checkbox("Write to screen on click", &bClickWritesToScreen);
#endif

	// todo remove this or tidy up
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint8_t scrMode = pCpcEmu->CpcEmuState.ga.video.mode;
	// mode 0 x4
	// mode 1 x8
	// mode 2 
	if (scrMode == 0)
	{
		ImGui::Text("logical w=%d(%d), logical h=%d(%d)", crtc.h_displayed, crtc.h_displayed*4, crtc.v_displayed, crtc.v_displayed*8);
		ImGui::Text("v sync pos=%d(%d), h sync pos=%d(%d)", crtc.v_sync_pos, crtc.v_sync_pos*8, crtc.h_sync_pos, crtc.h_sync_pos*8);
	}
	else if (scrMode == 1)
	{
		ImGui::Text("logical w=%d(%d), logical h=%d(%d)", crtc.h_displayed, crtc.h_displayed*8, crtc.v_displayed, crtc.v_displayed*8);
		ImGui::Text("v sync pos=%d(%d), h sync pos=%d(%d)", crtc.v_sync_pos, crtc.v_sync_pos*8, crtc.h_sync_pos, crtc.h_sync_pos*8);
	}

	ImVec2 pos = ImGui::GetCursorScreenPos();
	uint16_t textureWidth = AM40010_DISPLAY_WIDTH / 2;
	uint16_t textureHeight = AM40010_DISPLAY_HEIGHT;
	ImGui::Image(pCpcEmu->Texture, ImVec2(textureWidth, textureHeight));

	uint16_t screenWidth = crtc.h_displayed * 8;
	uint16_t screenHeight = crtc.v_displayed * 8;
	ImGuiIO& io = ImGui::GetIO();

	const int hSyncOffset = (crtc.h_sync_pos - 46) * 8; // offset based on the default horiz sync position (46)
	const int vSyncOffset = (crtc.v_sync_pos - 30) * 8; // offset based on the default vert sync position (30)
	const int vTotalOffset = (crtc.v_total - 38) * 8;
	const int screenEdgeL = crtc.h_displayed*8 - hSyncOffset + 32 - screenWidth;
	const int screenTop = crtc.v_displayed*8 - vSyncOffset + 37 - screenHeight + crtc.v_total_adjust + vTotalOffset;
	
	ImDrawList* dl = ImGui::GetWindowDrawList();

#ifndef NDEBUG
	// draw screen area. todo clip when off screen
	// games not working: live and let die, backtro
	// registers not hooked up: r0,r3,r4,r5
	if (bDrawScreenExtents)
	{
		dl->AddRect(ImVec2(pos.x+screenEdgeL, pos.y+screenTop), ImVec2(pos.x+screenEdgeL+screenWidth, pos.y+screenTop+screenHeight), 0xffffffff);
	}
#endif

	if (ImGui::IsItemHovered())
	{
		int charWidth = scrMode == 0 ? 16 : 8; // todo: screen mode 2
		const int xp = std::min(std::max((int)(io.MousePos.x - pos.x - screenEdgeL), 0), screenWidth-1);
		const int yp = std::min(std::max((int)(io.MousePos.y - pos.y - screenTop), 0), screenHeight-1);

		uint16_t scrAddress = 0;
		if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
		{
			int rx = static_cast<int>(pos.x) + screenEdgeL + (xp & ~(charWidth-1));
			int ry = static_cast<int>(pos.y) + screenTop + (yp & ~0x7);
			
			// highlight the current 8x8 character
			dl->AddRect(ImVec2((float)rx, (float)ry), ImVec2((float)rx + charWidth, (float)ry + 8), 0xffffffff);
			
			int plotX = xp & ~(charWidth-1);
			int plotY = yp & ~0x7;

			if (ImGui::IsMouseClicked(0))
			{
#ifndef NDEBUG
				if (bClickWritesToScreen)
				{
					const uint8_t numBytes = pCpcEmu->GetBitsPerPixel();
					uint16_t plotAddress = 0;
					for (int y = 0; y < 8; y++)
					{
						if (pCpcEmu->GetScreenMemoryAddress(plotX, plotY+y, plotAddress))
						{
							for (int b=0; b<numBytes; b++)
							{
								pCpcEmu->WriteByte(plotAddress + b, 0xff);
							}
						}
					}
				}
#endif			
			}
			ImGui::BeginTooltip();
			// this screen pos is wrong in mode 0
			ImGui::Text("Screen Pos (%d,%d) (%d,%d)", xp, yp, plotX, plotY);
			ImGui::Text("Addr: %s", NumStr(scrAddress));
			ImGui::EndTooltip();
		}
	}

	ImGui::SliderFloat("Speed Scale", &pCpcEmu->ExecSpeedScale, 0.0f, 2.0f);
	//ImGui::SameLine();
	if (ImGui::Button("Reset"))
		pCpcEmu->ExecSpeedScale = 1.0f;
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
				cpc_key_down(&pCpcEmu->CpcEmuState, cpcKey);
		}
		else if (ImGui::IsKeyReleased(key))
		{
			const int cpcKey = CpcKeyFromImGuiKey(key);
			if (cpcKey != 0)
				cpc_key_up(&pCpcEmu->CpcEmuState, cpcKey);
		}
	}
}

