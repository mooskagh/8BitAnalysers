#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>

#include <Util/Misc.h>
#include <ImGuiSupport/ImGuiTexture.h>
#include <algorithm>

template<typename T> static inline T Clamp(T v, T mn, T mx)
{ 
	return (v < mn) ? mn : (v > mx) ? mx : v; 
}

void FCpcViewer::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;

	// setup texture
	chips_display_info_t dispInfo = cpc_display_info(&pEmu->CpcEmuState);

	// setup pixel buffer
	size_t w = dispInfo.frame.dim.width; // 1024
	size_t h = dispInfo.frame.dim.height; // 312

	const size_t pixelBufferSize = w * h;
	FrameBuffer = new uint32_t[pixelBufferSize * 2];
	ScreenTexture = ImGui_CreateTextureRGBA(FrameBuffer, w, h);
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

	static bool bDrawScreenExtents = true;
	ImGui::Checkbox("Draw screen extents", &bDrawScreenExtents);
	ImGui::SameLine();
	static uint8_t scrRectAlpha = 0xff;
	ImGui::PushItemWidth(40);
	ImGui::InputScalar("Alpha", ImGuiDataType_U8, &scrRectAlpha, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal);
	ImGui::PopItemWidth();
	static bool bShowScreenmodeChanges = false;
	ImGui::Checkbox("Show screenmode changes", &bShowScreenmodeChanges);
#ifndef NDEBUG
	static bool bClickWritesToScreen = false;
	ImGui::Checkbox("Write to screen on click", &bClickWritesToScreen);
#endif

	// see if mixed screen modes are used
	int scrMode = pCpcEmu->CpcEmuState.ga.video.mode;
	for (int s=0; s< AM40010_DISPLAY_HEIGHT; s++)
	{
		if (pCpcEmu->ScreenModePerScanline[s] != scrMode)
		{
			scrMode = -1;
			break;
		}
	}

	// display screen mode and resolution
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint8_t charHeight = crtc.max_scanline_addr + 1;			// crtc register 9 defines how many scanlines in a character square
	const int scrWidth = crtc.h_displayed * 8;
	const int scrHeight = crtc.v_displayed * charHeight;
	int multiplier[4] = {4, 8, 16, 4};
	if(scrMode == -1)
	{
		ImGui::Text("Screen mode: mixed", scrWidth, scrHeight);
		ImGui::Text("Resolution: mixed (%d) x %d", scrWidth, scrHeight);
	}
	else
	{
		ImGui::Text("Screen mode: %d", scrMode);
		ImGui::Text("Resolution: %d x %d", crtc.h_displayed * multiplier[scrMode], scrHeight);
	}

	// draw the cpc display
	chips_display_info_t disp = cpc_display_info(&pCpcEmu->CpcEmuState);

	// convert texture to RGBA
	const uint8_t* pix = (const uint8_t*)disp.frame.buffer.ptr;
	const uint32_t* pal = (const uint32_t*)disp.palette.ptr;
	for (int i = 0; i < disp.frame.buffer.size; i++)
		FrameBuffer[i] = pal[pix[i]];

	ImGui_UpdateTextureRGBA(ScreenTexture, FrameBuffer);

	static float uv0w = 0.0f;
	static float uv0h = 0.0f;
	static float uv1w = (float)AM40010_DISPLAY_WIDTH / (float)AM40010_FRAMEBUFFER_WIDTH;
	static float uv1h = (float)AM40010_DISPLAY_HEIGHT / (float)AM40010_FRAMEBUFFER_HEIGHT;
	static uint16_t dispw = 384;
	static uint16_t disph = 272;
#if 0
	ImGui::InputScalar("width", ImGuiDataType_U16, &dispw, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("height", ImGuiDataType_U16, &disph, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal);// ImGui::SameLine();
	ImGui::InputScalar("uv1w", ImGuiDataType_Float, &uv1w, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv1h", ImGuiDataType_Float, &uv1h, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv0w", ImGuiDataType_Float, &uv0w, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv0h", ImGuiDataType_Float, &uv0h, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal);
	
	ImGui::SliderFloat("uv1w", &uv1w, 0.0f, 1.0f);
	ImGui::SliderFloat("uv1h", &uv1h, 0.0f, 1.0f);
	ImGui::SliderFloat("uv0w", &uv0w, 0.0f, 1.0f);
	ImGui::SliderFloat("uv0h", &uv0h, 0.0f, 1.0f);
#endif

	const ImVec2 pos = ImGui::GetCursorScreenPos(); // get the position of the texture
	ImVec2 uv0(uv0w, uv0h);
	ImVec2 uv1(uv1w, uv1h);
	ImGui::Image(ScreenTexture, ImVec2(dispw, disph), uv0, uv1);

	// work out the position and size of the logical cpc screen based on the crtc registers.
	// note: these calculations will be wrong if the game sets crtc registers dynamically during the frame.
	// registers not hooked up: R3
	const int scanLinesPerCharOffset = 37 - (8 - (crtc.max_scanline_addr + 1)) * 9; 
	const int hTotOffset = (crtc.h_total - 63) * 8;					// offset based on the default horiz total size (63 chars)
	const int vTotalOffset = (crtc.v_total - 38) * charHeight;		// offset based on the default vertical total size (38 chars)
	const int hSyncOffset = (crtc.h_sync_pos - 46) * 8;				// offset based on the default horiz sync position (46 chars)
	const int vSyncOffset = (crtc.v_sync_pos - 30) * charHeight;	// offset based on the default vert sync position (30 chars)
	const int scrEdgeL = crtc.h_displayed * 8 - hSyncOffset + 32 - scrWidth + hTotOffset;
	const int scrTop = crtc.v_displayed * charHeight - vSyncOffset + scanLinesPerCharOffset - scrHeight + crtc.v_total_adjust + vTotalOffset;
	
	ImDrawList* dl = ImGui::GetWindowDrawList();

	
	// draw screen area.
	if (bDrawScreenExtents)
	{
		const float y_min = Clamp(pos.y + scrTop, pos.y, pos.y + textureHeight);
		const float y_max = Clamp(pos.y + scrTop + scrHeight, pos.y, pos.y + textureHeight);
		const float x_min = Clamp(pos.x + scrEdgeL, pos.x, pos.x + textureWidth);
		const float x_max = Clamp(pos.x + scrEdgeL + scrWidth, pos.x, pos.x + textureWidth);

		dl->AddRect(ImVec2(x_min, y_min), ImVec2(x_max, y_max), scrRectAlpha << 24 | 0xffffff);
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

	// todo highlight hovered address in code analyser view

	if (ImGui::IsItemHovered())
	{
		ImGuiIO& io = ImGui::GetIO();

		// get mouse cursor coords in logical screen area space
		const int xp = Clamp((int)(io.MousePos.x - pos.x - scrEdgeL), 0, scrWidth-1);
		const int yp = Clamp((int)(io.MousePos.y - pos.y - scrTop), 0, scrHeight-1);
		
		// get the screen mode for the raster line the mouse is pointed at
		// note: the logic doesn't always work and we sometimes end up with a negative scanline.
		// for example, this can happen for demos that set the scanlines-per-character to 1.
		const int scanline = scrTop + yp;
		const int scrMode = scanline > 0 && scanline < AM40010_DISPLAY_HEIGHT ? pCpcEmu->ScreenModePerScanline[scanline] : -1;
		const int charWidth = scrMode == 0 ? 16 : 8; // todo: screen mode 2
		
		uint16_t scrAddress = 0;
		// note: for screen mode 0 this will be in coord space of 320 x 200.
		// not sure that is right?
		if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
		{
			const int charX = xp & ~(charWidth-1);
			const int charY = (yp / charHeight) * charHeight;
			const float rx = Clamp(pos.x + scrEdgeL + charX, pos.x, pos.x + textureWidth);
			const float ry = Clamp(pos.y + scrTop + charY, pos.y, pos.y + textureHeight);

			// highlight the current character "square" (could actually be a rectangle if the char height is not 8)
			dl->AddRect(ImVec2(rx, ry), ImVec2((float)rx + charWidth, (float)ry + charHeight), 0xffffffff);

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

			// adjust the x position based on the screen mode for the scanline
			const int divisor[4] = { 4, 2, 1, 2};
			const int x_adj = (xp * 2) / (scrMode == -1 ? 2 : divisor[scrMode]);
			
			ImGui::Text("Screen Pos (%d,%d)", x_adj, yp);
			ImGui::Text("Addr: %s", NumStr(scrAddress));
			if (scrMode == -1)
				ImGui::Text("Screen Mode: unknown", scrMode);
			else
				ImGui::Text("Screen Mode: %d", scrMode);
			ImGui::EndTooltip();
		}
	}

	ImGui::SliderFloat("Speed Scale", &pCpcEmu->ExecSpeedScale, 0.0f, 2.0f);
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
		pCpcEmu->ExecSpeedScale = 1.0f;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	bWindowFocused = ImGui::IsWindowFocused();
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
			if (bWindowFocused)
			{
				int cpcKey = CpcKeyFromImGuiKey(key);
				if (cpcKey != 0)
					cpc_key_down(&pCpcEmu->CpcEmuState, cpcKey);
			}
		}
		else if (ImGui::IsKeyReleased(key))
		{
			const int cpcKey = CpcKeyFromImGuiKey(key);
			if (cpcKey != 0)
				cpc_key_up(&pCpcEmu->CpcEmuState, cpcKey);
		}
	}
}

