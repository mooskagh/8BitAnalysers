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
#ifndef NDEBUG
	// debug code to manually iterate through all snaps in a directory
	if (pCpcEmu->GetGamesList().GetNoGames())
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
			if (gGameIndex < pCpcEmu->GetGamesList().GetNoGames()-1) 
				gGameIndex++;
			bLoadSnap = true;
		}
		ImGui::SameLine();
		const FGameSnapshot& game = pCpcEmu->GetGamesList().GetGame(gGameIndex);
		ImGui::Text("(%d/%d) %s", gGameIndex+1, pCpcEmu->GetGamesList().GetNoGames(), game.DisplayName.c_str());
		if (bLoadSnap)
			pCpcEmu->GetGamesList().LoadGame(gGameIndex);
	}
#endif

#ifndef NDEBUG
	static bool bDrawScreenExtentsHoriz = true;
	static bool bDrawScreenExtentsVert = true;
	ImGui::Checkbox("Draw screen extents Horiz", &bDrawScreenExtentsHoriz);
	ImGui::Checkbox("Draw screen extents Vert", &bDrawScreenExtentsVert);
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

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const uint16_t textureWidth = AM40010_DISPLAY_WIDTH / 2;
	const uint16_t textureHeight = AM40010_DISPLAY_HEIGHT;
	ImGui::Image(pCpcEmu->Texture, ImVec2(textureWidth, textureHeight));

	// crtc register 9 defines how many scanlines in a character square
	const uint8_t charHeight = crtc.max_scanline_addr + 1; 
	const uint16_t screenWidth = crtc.h_displayed * 8;
	const uint16_t screenHeight = crtc.v_displayed * charHeight;
	ImGuiIO& io = ImGui::GetIO();

	const int offset = 37 - (8 - (crtc.max_scanline_addr+1)) * 9; 
	//ImGui::InputScalar("offset", ImGuiDataType_U8, &offset, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal);

	const int hSyncOffset = (crtc.h_sync_pos - 46) * 8; // offset based on the default horiz sync position (46)
	const int vSyncOffset = (crtc.v_sync_pos - 30) * charHeight; // offset based on the default vert sync position (30)
	const int vTotalOffset = (crtc.v_total - 38) * charHeight;
	const int screenEdgeL = crtc.h_displayed*8 - hSyncOffset + 32 - screenWidth;
	const int screenTop = crtc.v_displayed*charHeight - vSyncOffset + offset - screenHeight + crtc.v_total_adjust + vTotalOffset;
	
	ImDrawList* dl = ImGui::GetWindowDrawList();

#ifndef NDEBUG
	// draw screen area. todo clip when off screen
	// registers not hooked up: r0,r3
	if (bDrawScreenExtentsVert)
	{
		dl->AddLine(ImVec2(pos.x, pos.y+screenTop), ImVec2(pos.x+textureWidth, pos.y+screenTop), 0xffffffff);
		dl->AddLine(ImVec2(pos.x, pos.y+screenTop+screenHeight-1), ImVec2(pos.x+textureWidth, pos.y+screenTop+screenHeight-1), 0xffffffff);
	}
	if (bDrawScreenExtentsHoriz)
	{
		dl->AddLine(ImVec2(pos.x+screenEdgeL, pos.y), ImVec2(pos.x+screenEdgeL, pos.y+textureHeight), 0xffffffff);
		dl->AddLine(ImVec2(pos.x+screenEdgeL+screenWidth-1, pos.y), ImVec2(pos.x+screenEdgeL+screenWidth-1, pos.y+textureHeight), 0xffffffff);
		
		//dl->AddRect(ImVec2(pos.x+screenEdgeL, pos.y+screenTop), ImVec2(pos.x+screenEdgeL+screenWidth, pos.y+screenTop+screenHeight), 0xffffffff);
	}
#endif

	if (ImGui::IsItemHovered())
	{
		const int charWidth = scrMode == 0 ? 16 : 8; // todo: screen mode 2
		const int xp = std::min(std::max((int)(io.MousePos.x - pos.x - screenEdgeL), 0), screenWidth-1);
		const int yp = std::min(std::max((int)(io.MousePos.y - pos.y - screenTop), 0), screenHeight-1);

		uint16_t scrAddress = 0;
		if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
		{
			const int charX = xp & ~(charWidth-1);
			const int charY = (yp / charHeight) * charHeight;
			const int rx = static_cast<int>(pos.x) + screenEdgeL + charX;
			const int ry = static_cast<int>(pos.y) + screenTop + charY;
			
			// highlight the current character "square" (could be a rectangle)
			dl->AddRect(ImVec2((float)rx, (float)ry), ImVec2((float)rx + charWidth, (float)ry + charHeight), 0xffffffff);

			if (ImGui::IsMouseClicked(0))
			{
#ifndef NDEBUG
				if (bClickWritesToScreen)
				{
					const uint8_t numBytes = GetBitsPerPixel(scrMode);
					uint16_t plotAddress = 0;
					for (int y = 0; y < charHeight; y++)
					{
						if (pCpcEmu->GetScreenMemoryAddress(charX, charY+y, plotAddress))
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
			ImGui::Text("Screen Pos (%d,%d)", xp, yp);
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

