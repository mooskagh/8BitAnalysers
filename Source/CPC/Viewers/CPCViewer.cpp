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
	static bool bDrawScreenExtents = true;
	ImGui::Checkbox("Draw screen extents", &bDrawScreenExtents);
	ImGui::SameLine();
	static uint8_t scrRectAlpha = 0xff;
	ImGui::PushItemWidth(40);
	ImGui::InputScalar("Alpha", ImGuiDataType_U8, &scrRectAlpha, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal);
	ImGui::PopItemWidth();

	static bool bClickWritesToScreen = true;
	ImGui::Checkbox("Write to screen on click", &bClickWritesToScreen);
	static bool bShowScreenmodeChanges = true;
	ImGui::Checkbox("Show screenmode changes", &bShowScreenmodeChanges);
#endif

	// draw the cpc display
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const int textureWidth = AM40010_DISPLAY_WIDTH / 2;
	const int textureHeight = AM40010_DISPLAY_HEIGHT;
	ImGui::Image(pCpcEmu->Texture, ImVec2(textureWidth, textureHeight));
	
	// work out the position and size of the logical cpc screen based on the crtc registers.
	// note: these calculations will be wrong if the game sets crtc registers dynamically during the frame.
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint8_t charHeight = crtc.max_scanline_addr + 1;			// crtc register 9 defines how many scanlines in a character square
	const int scrWidth = crtc.h_displayed * 8;
	const int scrHeight = crtc.v_displayed * charHeight;
	const int scanLinesPerCharOffset = 37 - (8 - (crtc.max_scanline_addr + 1)) * 9; 
	const int hTotOffset = (crtc.h_total - 63) * 8;					// offset based on the default horiz total size (63 chars)
	const int vTotalOffset = (crtc.v_total - 38) * charHeight;		// offset based on the default vertical total size (38 chars)
	const int hSyncOffset = (crtc.h_sync_pos - 46) * 8;				// offset based on the default horiz sync position (46 chars)
	const int vSyncOffset = (crtc.v_sync_pos - 30) * charHeight;	// offset based on the default vert sync position (30 chars)
	const int scrEdgeL = crtc.h_displayed * 8 - hSyncOffset + 32 - scrWidth + hTotOffset;
	const int scrTop = crtc.v_displayed * charHeight - vSyncOffset + scanLinesPerCharOffset - scrHeight + crtc.v_total_adjust + vTotalOffset;
	
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// registers not hooked up: r3
#ifndef NDEBUG
	// draw screen area.
	if (bDrawScreenExtents)
	{
		const int top = std::max(scrTop, 0);
		const int bot = std::min(scrTop + scrHeight, textureHeight);
		const int r = std::min(scrEdgeL + scrWidth, textureWidth);
		const int l = std::max(scrEdgeL, 0);
		dl->AddRect(ImVec2(pos.x + l, pos.y + top), ImVec2(pos.x + r, pos.y + bot), scrRectAlpha << 24 | 0xffffff);
	}

	// colourize scanlines depending on the screen mode
	if (bShowScreenmodeChanges)
	{
		for (int s=0; s< AM40010_DISPLAY_HEIGHT; s++)
		{
			uint8_t scrMode = pCpcEmu->ScreenModePerScanline[s];
			dl->AddLine(ImVec2(pos.x, pos.y + s), ImVec2(pos.x + textureWidth, pos.y + s), scrMode == 0 ? 0x40ffff00 : 0x4000ffff);
		}
	}
#endif

	if (ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();
		// get mouse cursor coords clamped to logical screen area
		const int xp = std::min(std::max((int)(io.MousePos.x - pos.x - scrEdgeL), 0), scrWidth-1);
		const int yp = std::min(std::max((int)(io.MousePos.y - pos.y - scrTop), 0), scrHeight-1);
		
		// get the screen mode for this raster line
		const int scanline = std::min(std::max((int)(io.MousePos.y - pos.y), 0), scrHeight - 1);
		const uint8_t scrMode = pCpcEmu->ScreenModePerScanline[scanline];
		const int charWidth = scrMode == 0 ? 16 : 8; // todo: screen mode 2

		uint16_t scrAddress = 0;
		if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
		{
			const int charX = xp & ~(charWidth-1);
			const int charY = (yp / charHeight) * charHeight;
			const int rx = static_cast<int>(pos.x) + scrEdgeL + charX;
			const int ry = static_cast<int>(pos.y) + scrTop + charY;
			
			// highlight the current character "square" (could actually be a rectangle if the char height is not 8)
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
						if (pCpcEmu->GetScreenMemoryAddress(charX, charY + y, plotAddress))
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
			ImGui::Text("Screen Mode: %d", scrMode);
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

