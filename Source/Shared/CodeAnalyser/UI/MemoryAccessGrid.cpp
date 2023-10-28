#include "MemoryAccessGrid.h"

#include "CodeAnalyser/CodeAnalyser.h"
#include <imgui.h>
#include "ImGuiSupport/ImGuiScaling.h"
#include "CodeAnalyserUI.h"

FMemoryAccessGrid::FMemoryAccessGrid(FCodeAnalysisState* pCodeAnalysis, int xGridSize, int yGridSize)
	: CodeAnalysis(pCodeAnalysis)
	, GridSizeX(xGridSize)
	, GridSizeY(yGridSize)
{

}

bool FMemoryAccessGrid::GetAddressGridPosition(FAddressRef address, int& outX, int& outY)
{
	// draw hovered address
	if (address.IsValid())
	{
		const FAddressRef startAddr = GetGridSquareAddress(0, 0);
		const FAddressRef endAddr = GetGridSquareAddress(GridSizeX - 1, GridSizeY - 1);

		const uint16_t charMapEndAddr = endAddr.Address;//charMapAddress + (noCharsX * noCharsY) - 1;

		// is checking bank ID enough?
		if (address.BankId == endAddr.BankId &&
			address.Address >= startAddr.Address &&
			address.Address <= endAddr.Address)
		{
			const uint16_t addrOffset = address.Address - startAddr.Address;
			outX = addrOffset % GridSizeX;
			outY = addrOffset / GridSizeX;
			return true;
		}
	}
	return false;
}

void FMemoryAccessGrid::DrawAt(float x, float y)
{
	OnDraw();

	// Display Character Map
	FCodeAnalysisState& state = *CodeAnalysis;
	FCodeAnalysisViewState& viewState = state.GetFocussedViewState();
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 pos(x, y);
	const float rectSize = GridSquareSize;

	for (int y = 0; y < GridSizeY; y++)
	{
		for (int x = 0; x < GridSizeX; x++)
		{
			FAddressRef curCharAddress = GetGridSquareAddress(x, y);
			FDataInfo* pDataInfo = state.GetDataInfoForAddress(curCharAddress);
			const int framesSinceWritten = pDataInfo->LastFrameWritten == -1 ? 255 : state.CurrentFrameNo - pDataInfo->LastFrameWritten;
			const int framesSinceRead = pDataInfo->LastFrameRead == -1 ? 255 : state.CurrentFrameNo - pDataInfo->LastFrameRead;
			const int wBrightVal = (255 - std::min(framesSinceWritten << 3, 255)) & 0xff;
			const int rBrightVal = (255 - std::min(framesSinceRead << 3, 255)) & 0xff;

			if (wBrightVal > 0 || rBrightVal > 0)	// skip empty chars
			{
				const float xp = pos.x + (x * rectSize);
				const float yp = pos.y + (y * rectSize);
				ImVec2 rectMin(xp, yp);
				ImVec2 rectMax(xp + rectSize, yp + rectSize);

				if (bShowReadWrites)
				{
					if (rBrightVal > 0)
					{
						const ImU32 col = 0xff000000 | (rBrightVal << 8);
						dl->AddRect(rectMin, rectMax, col);

						rectMin = ImVec2(rectMin.x + 1, rectMin.y + 1);
						rectMax = ImVec2(rectMax.x - 1, rectMax.y - 1);
					}
					if (wBrightVal > 0)
					{
						const ImU32 col = 0xff000000 | (wBrightVal << 0);
						dl->AddRect(rectMin, rectMax, col);
					}
				}
			}
		}
	}

	// draw highlight rect
	const float mousePosX = io.MousePos.x - pos.x;
	const float mousePosY = io.MousePos.y - pos.y;
	if (mousePosX >= 0 && mousePosY >= 0 && mousePosX < (GridSizeX * rectSize) && mousePosY < (GridSizeY * rectSize))
	{
		const int xChar = (int)floor(mousePosX / rectSize);
		const int yChar = (int)floor(mousePosY / rectSize);

		FAddressRef charAddress = GetGridSquareAddress(xChar, yChar);
		//const uint16_t charAddress = charMapAddress + (xChar + (yChar * noCharsX));
		const uint8_t charVal = state.ReadByte(charAddress);

		const float xp = pos.x + (xChar * rectSize);
		const float yp = pos.y + (yChar * rectSize);
		const ImVec2 rectMin(xp, yp);
		const ImVec2 rectMax(xp + rectSize, yp + rectSize);
		dl->AddRect(rectMin, rectMax, 0xffffffff);

		if (ImGui::IsMouseClicked(0))
		{
			SelectedCharAddress = charAddress;
			SelectedCharX = xChar;
			SelectedCharY = yChar;
		}

		// Tool Tip
		ImGui::BeginTooltip();
		ImGui::Text("Char Pos (%d,%d)", xChar, yChar);
		ImGui::Text("Value: %s", NumStr(charVal));
		ImGui::EndTooltip();
	}

	if (SelectedCharX != -1 && SelectedCharY != -1)
	{
		const float xp = pos.x + (SelectedCharX * rectSize);
		const float yp = pos.y + (SelectedCharY * rectSize);
		const ImVec2 rectMin(xp, yp);
		const ImVec2 rectMax(xp + rectSize, yp + rectSize);
		dl->AddRect(rectMin, rectMax, 0xffffffff);
	}

	// draw hovered address
	int hoverredX = -1;
	int hoverredY = -1;
	if (GetAddressGridPosition(viewState.HighlightAddress,hoverredX,hoverredY))
	{
		const float xp = pos.x + (hoverredX * rectSize);
		const float yp = pos.y + (hoverredY * rectSize);
		const ImVec2 rectMin(xp, yp);
		const ImVec2 rectMax(xp + rectSize, yp + rectSize);
		dl->AddRect(rectMin, rectMax, 0xffff00ff);
	}

	pos.y += GridSizeY * rectSize;
	ImGui::SetCursorScreenPos(pos);
	ImGui::Checkbox("Show Reads & Writes", &bShowReadWrites);
	if (SelectedCharAddress.IsValid())
	{
		ImGui::Text("Address: %s", NumStr(SelectedCharAddress.Address));
		DrawAddressLabel(state,state.GetFocussedViewState(),SelectedCharAddress);
		// Show data reads & writes
		FDataInfo* pDataInfo = state.GetDataInfoForAddress(SelectedCharAddress);
		// List Data accesses
		if (pDataInfo->Reads.IsEmpty() == false)
		{
			ImGui::Text("Reads:");
			for (const auto& reader : pDataInfo->Reads.GetReferences())
			{
				ShowCodeAccessorActivity(state, reader);

				ImGui::Text("   ");
				ImGui::SameLine();
				DrawCodeAddress(state, viewState, reader);
			}
		}

		if (pDataInfo->Writes.IsEmpty() == false)
		{
			ImGui::Text("Writes:");
			for (const auto& writer : pDataInfo->Writes.GetReferences())
			{
				ShowCodeAccessorActivity(state, writer);

				ImGui::Text("   ");
				ImGui::SameLine();
				DrawCodeAddress(state, viewState, writer);
			}
		}
	}
}
