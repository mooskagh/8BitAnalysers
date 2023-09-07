#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>

#include "Util/Misc.h"
#include "Util/GraphicsView.h"

#include <ImGuiSupport/ImGuiTexture.h>
#include <ImGuiSupport/ImGuiScaling.h>
#include <algorithm>

int CpcKeyFromImGuiKey(ImGuiKey key);

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
	int w = dispInfo.frame.dim.width; // 1024
	int h = dispInfo.frame.dim.height; // 312

	const size_t pixelBufferSize = w * h;
	FrameBuffer = new uint32_t[pixelBufferSize * 2];
	ScreenTexture = ImGui_CreateTextureRGBA(FrameBuffer, w, h);

	TextureWidth = AM40010_DISPLAY_WIDTH / 2;
	TextureHeight = AM40010_DISPLAY_HEIGHT;
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

	CalculateScreenProperties();

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
	const int multiplier[4] = {4, 8, 16, 4};
	if(scrMode == -1)
	{
		ImGui::Text("Screen mode: mixed", ScreenWidth, ScreenHeight);
		ImGui::Text("Resolution: mixed (%d) x %d", ScreenWidth, ScreenHeight);
	}
	else
	{
		ImGui::Text("Screen mode: %d", scrMode);
		ImGui::Text("Resolution: %d x %d", crtc.h_displayed * multiplier[scrMode], ScreenHeight);
	}

	// draw the cpc display
	chips_display_info_t disp = cpc_display_info(&pCpcEmu->CpcEmuState);

	// convert texture to RGBA
	const uint8_t* pix = (const uint8_t*)disp.frame.buffer.ptr;
	const uint32_t* pal = (const uint32_t*)disp.palette.ptr;
	for (int i = 0; i < disp.frame.buffer.size; i++)
		FrameBuffer[i] = pal[pix[i]];

	ImGui_UpdateTextureRGBA(ScreenTexture, FrameBuffer);

	const static float uv0w = 0.0f;
	const static float uv0h = 0.0f;
	const static float uv1w = (float)AM40010_DISPLAY_WIDTH / (float)AM40010_FRAMEBUFFER_WIDTH;
	const static float uv1h = (float)AM40010_DISPLAY_HEIGHT / (float)AM40010_FRAMEBUFFER_HEIGHT;
#if 0
	ImGui::InputScalar("width", ImGuiDataType_Float, &textureWidth, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("height", ImGuiDataType_Float, textureHeight, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal);// ImGui::SameLine();
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
	ImGui::Image(ScreenTexture, ImVec2(TextureWidth, TextureHeight), uv0, uv1);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	// draw screen area.
	if (bDrawScreenExtents)
	{
		const float y_min = Clamp(pos.y + ScreenTop, pos.y, pos.y + TextureHeight);
		const float y_max = Clamp(pos.y + ScreenTop + ScreenHeight, pos.y, pos.y + TextureHeight);
		const float x_min = Clamp(pos.x + ScreenEdgeL, pos.x, pos.x + TextureWidth);
		const float x_max = Clamp(pos.x + ScreenEdgeL + ScreenWidth, pos.x, pos.x + TextureWidth);

		dl->AddRect(ImVec2(x_min, y_min), ImVec2(x_max, y_max), scrRectAlpha << 24 | 0xffffff);
	}

	// colourize scanlines depending on the screen mode
	if (bShowScreenmodeChanges)
	{
		for (int s=0; s< AM40010_DISPLAY_HEIGHT; s++)
		{
			const uint8_t scrMode = pCpcEmu->ScreenModePerScanline[s];
			dl->AddLine(ImVec2(pos.x, pos.y + s), ImVec2(pos.x + TextureWidth, pos.y + s), scrMode == 0 ? 0x40ffff00 : 0x4000ffff);
		}
	}

	// todo highlight hovered address in code analyser view

	bool bJustSelectedChar = false;
	if (ImGui::IsItemHovered())
	{
		bJustSelectedChar = OnHovered(pos, ScreenEdgeL, ScreenTop);
	}

	// draw an entire screen out of characters
	//ImGui::BeginChild("dogdirt");
	{
		ImVec2 curPos = ImGui::GetCursorScreenPos();
		const float xStart = curPos.x;

		for (int y = 0; y < crtc.v_displayed; y++)
		{
			for (int x = 0; x < HorizCharCount; x++) // why the -1. because we're treating mode 0 as double width?
			//for (int x = 0; x < crtc.h_displayed; x++) // why the -1. because we're treating mode 0 as double width?
			//for (int x = crtc.h_displayed - 1; x < crtc.h_displayed; x++) // why the -1. because we're treating mode 0 as double width?
			//for (int x = 0; x < 1; x++) // why the -1. because we're treating mode 0 as double width?
			{
				curPos.x += DrawScreenCharacter(x, y, scrMode, curPos.x, curPos.y, ScreenTop + y * 8);
				//curPos.x += 40.0f;
			}
			curPos.x = xStart;
			curPos.y += 40.0f;
		}
		ImGui::SetCursorScreenPos(ImVec2(curPos.x, curPos.y));
	}
	//ImGui::EndChild();
	//ImGui::SliderFloat("Speed Scale", &pCpcEmu->ExecSpeedScale, 0.0f, 2.0f);
	//ImGui::SameLine();
	//if (ImGui::Button("Reset"))
	//	pCpcEmu->ExecSpeedScale = 1.0f;
	//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	bWindowFocused = ImGui::IsWindowFocused();
}

bool FCpcViewer::OnHovered(const ImVec2& pos, int scrEdgeL, int scrTop)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();

	// get mouse cursor coords in logical screen area space
	const int xp = Clamp((int)(io.MousePos.x - pos.x - scrEdgeL), 0, ScreenWidth - 1);
	const int yp = Clamp((int)(io.MousePos.y - pos.y - scrTop), 0, ScreenHeight - 1);

	// get the screen mode for the raster line the mouse is pointed at
	// note: the logic doesn't always work and we sometimes end up with a negative scanline.
	// for example, this can happen for demos that set the scanlines-per-character to 1.
	const int scanline = scrTop + yp;
	// get the screen mode for this scan line
	const int scrMode = scanline > 0 && scanline < AM40010_DISPLAY_HEIGHT ? pCpcEmu->ScreenModePerScanline[scanline] : -1;
	const int charWidth = scrMode == 0 ? 16 : 8;

	// note: for screen mode 0 this will be in coord space of 320 x 200.
	// not sure that is right?
	uint16_t scrAddress = 0;
	if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
	{
		const int charX = xp & ~(charWidth - 1);
		const int charY = (yp / CharacterHeight) * CharacterHeight;
		const float rx = Clamp(pos.x + scrEdgeL + charX, pos.x, pos.x + TextureWidth);
		const float ry = Clamp(pos.y + scrTop + charY, pos.y, pos.y + TextureHeight);

		// highlight the current character "square" (could actually be a rectangle if the char height is not 8)
		dl->AddRect(ImVec2(rx, ry), ImVec2((float)rx + charWidth, (float)ry + CharacterHeight), 0xffffffff);

		FCodeAnalysisState& codeAnalysis = pCpcEmu->CodeAnalysis;
		const FAddressRef lastPixWriter = codeAnalysis.GetLastWriterForAddress(scrAddress);

#ifndef NDEBUG
		if (ImGui::IsMouseClicked(0))
		{
			if (bClickWritesToScreen)
			{
				const uint8_t numBytes = GetBitsPerPixel(scrMode);
				uint16_t plotAddress = 0;
				for (int y = 0; y < CharacterHeight; y++)
				{
					if (pCpcEmu->GetScreenMemoryAddress(charX, charY + y, plotAddress))
					{
						for (int b = 0; b < numBytes; b++)
						{
							pCpcEmu->WriteByte(plotAddress + b, 0xff);
						}
					}
				}
			}
		}
#endif			
		ImGui::BeginTooltip();

		// adjust the x position based on the screen mode for the scanline
		const int divisor[4] = { 4, 2, 1, 2 };
		const int x_adj = (xp * 2) / (scrMode == -1 ? 2 : divisor[scrMode]);

		ImGui::Text("raw coords (%d,%d)", xp, yp);

		ImGui::Text("Screen Pos (%d,%d)", x_adj, yp);
		ImGui::Text("Addr: %s", NumStr(scrAddress));

		FCodeAnalysisViewState& viewState = codeAnalysis.GetFocussedViewState();
		if (lastPixWriter.IsValid())
		{
			ImGui::Text("Pixel Writer: ");
			ImGui::SameLine();
			DrawCodeAddress(codeAnalysis, viewState, lastPixWriter);
		}

		if (scrMode == -1)
			ImGui::Text("Screen Mode: unknown", scrMode);
		else
			ImGui::Text("Screen Mode: %d", scrMode);
		
		//DrawScreenCharacter(xp, yp, scrMode, 413, 517);
		//DrawScreenCharacter(xp+8, yp, scrMode, 413+80, 517);
		//DrawScreenCharacter(xp+16, yp, scrMode, 413+160, 517);

		//ImGui::Text("");
		//ImGui::Text("");
		//ImGui::Text("");
		//ImGui::Text("");

		ImGui::EndTooltip();
		//}

		if (ImGui::IsMouseDoubleClicked(0))
			viewState.GoToAddress(lastPixWriter);
	}

	return false;
}

// todo: 
// - get palette from the palette for that scanline
// - problematic mode 0 games:
//		canard
//		amsbrique

// coords need to be in character squares?
// returns how much space it took
float FCpcViewer::DrawScreenCharacter(int xChar, int yChar, int screenMode, float x, float y, int scanlineTEMP) const 
{
	// todo move this comment out of this function
	// the x coord will be in mode 1 coordinates. [320 pixels]
	// mode 0 will effectively use 2 mode 1 pixels.

	const FCodeAnalysisState& codeAnalysis = pCpcEmu->CodeAnalysis;
	const int xMult = screenMode == 0 ? 2 : 1;
	ImVec2 pixelSize = ImVec2(5.0f * (float)xMult, 5.0f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 pos = ImVec2((float)x, (float)y);
	const float startPos = pos.x;
	for (int pixline = 0; pixline < CharacterHeight; pixline++) 
	{
		uint16_t pixLineAddress = 0;
		// todo check return from this
		pCpcEmu->GetScreenMemoryAddress(xChar * xMult * 8, yChar * CharacterHeight + pixline, pixLineAddress); // this wont work

		switch (screenMode)
		{
		case 0:
			// double width
			for (int byte = 0; byte < 4; byte++)
			{
				const uint8_t val = codeAnalysis.ReadByte(pixLineAddress + byte);

				for (int pixel = 0; pixel < 2; pixel++)
				{
					const ImVec2 rectMin(pos.x, pos.y);
					const ImVec2 rectMax(pos.x + pixelSize.x, pos.y + pixelSize.y);

					int colourIndex = 0;

					switch (pixel)
					{
					case 0:
						colourIndex = (val & 0x80 ? 1 : 0) | (val & 0x8 ? 2 : 0) | (val & 0x20 ? 4 : 0) | (val & 0x2 ? 8 : 0);
						break;
					case 1:
						colourIndex = (val & 0x40 ? 1 : 0) | (val & 0x4 ? 2 : 0) | (val & 0x10 ? 4 : 0) | (val & 0x1 ? 8 : 0);
						break;
					}

					const ImU32 colour = pCpcEmu->PalettePerScanline[scanlineTEMP].GetColour(colourIndex);
					//const ImU32 colour = GetCurrentPalette_Const().GetColour(colourIndex);
					dl->AddRectFilled(rectMin, rectMax, colour);
					//dl->AddRect(rectMin, rectMax, 0xffffffff);

					pos.x += pixelSize.x;
				}
			}
			break;
		case 1:
			for (int byte = 0; byte < 2; byte++)
			{
				const uint8_t val = codeAnalysis.ReadByte(pixLineAddress + byte);

				for (int pixel = 0; pixel < 4; pixel++)
				{
					const ImVec2 rectMin(pos.x, pos.y);
					const ImVec2 rectMax(pos.x + pixelSize.x, pos.y + pixelSize.y);

					int colourIndex = 0;

					switch (pixel)
					{
					case 0:
						colourIndex = (val & 0x8 ? 2 : 0) | (val & 0x80 ? 1 : 0);
						break;
					case 1:
						colourIndex = (val & 0x4 ? 2 : 0) | (val & 0x40 ? 1 : 0);
						break;
					case 2:
						colourIndex = (val & 0x2 ? 2 : 0) | (val & 0x20 ? 1 : 0);
						break;
					case 3:
						colourIndex = (val & 0x1 ? 2 : 0) | (val & 0x10 ? 1 : 0);
						break;
					}

					const ImU32 colour = pCpcEmu->PalettePerScanline[scanlineTEMP].GetColour(colourIndex);
					//const ImU32 colour = GetCurrentPalette_Const().GetColour(colourIndex);
					//const ImU32 colour = GetCurrentPalette_Const().GetColour(colourIndex);
					dl->AddRectFilled(rectMin, rectMax, colour);
					//dl->AddRect(rectMin, rectMax, 0xffffffff);

					pos.x += pixelSize.x;
				}
			}
			break;
		}

		pos.x = startPos;
		pos.y += pixelSize.y;
	}
	float rectWidth = pixelSize.x * 8.0f;
	dl->AddRect(ImVec2(x,y), ImVec2(x + rectWidth, y + pixelSize.y * CharacterHeight), 0xffffffff);
	return rectWidth;
}

void FCpcViewer::CalculateScreenProperties()
{
	// work out the position and size of the logical cpc screen based on the crtc registers.
	// note: these calculations will be wrong if the game sets crtc registers dynamically during the frame.
	// registers not hooked up: R3
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;

	CharacterHeight = crtc.max_scanline_addr + 1;			// crtc register 9 defines how many scanlines in a character square
	
	ScreenWidth = crtc.h_displayed * 8; // this is always in mode 1 coords. fix this?
	ScreenHeight = crtc.v_displayed * CharacterHeight;

	const int _hTotOffset = (crtc.h_total - 63) * 8;					// offset based on the default horiz total size (63 chars)
	const int _hSyncOffset = (crtc.h_sync_pos - 46) * 8;				// offset based on the default horiz sync position (46 chars)
	ScreenEdgeL = crtc.h_displayed * 8 - _hSyncOffset + 32 - ScreenWidth + _hTotOffset;

	const int _scanLinesPerCharOffset = 37 - (8 - (crtc.max_scanline_addr + 1)) * 9;
	const int _vTotalOffset = (crtc.v_total - 38) * CharacterHeight;		// offset based on the default vertical total size (38 chars)
	const int _vSyncOffset = (crtc.v_sync_pos - 30) * CharacterHeight;		// offset based on the default vert sync position (30 chars)
	ScreenTop = crtc.v_displayed * CharacterHeight - _vSyncOffset + _scanLinesPerCharOffset - ScreenHeight + crtc.v_total_adjust + _vTotalOffset;

	HorizCharCount = pCpcEmu->CpcEmuState.ga.video.mode == 0 ? crtc.h_displayed / 2 : crtc.h_displayed;
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