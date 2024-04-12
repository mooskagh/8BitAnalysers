#include "CharacterMapViewer.h"
#include "../CodeAnalyser.h"
#include "MemoryAccessGrid.h"

#include <imgui.h>
#include "CodeAnalyserUI.h"

#include <cmath>
#include <ImGuiSupport/ImGuiScaling.h>

static const char* g_MaskInfoTxt[] =
{
	"None",
	"InterleavedBytesPM",
	"InterleavedBytesMP",
};

static const char* g_ColourInfoTxt[] =
{
	"None",
	"InterleavedPost",
	"MemoryLUT",
	"InterleavedPre",
};

void RunEnumTests()
{
	assert(IM_ARRAYSIZE(g_MaskInfoTxt) == (int)EMaskInfo::Max);	// if this asserts then you need to look at how EColourInfo maps to g_ColourInfoTxt
	assert(IM_ARRAYSIZE(g_ColourInfoTxt) == (int)EColourInfo::Max);	// if this asserts then you need to look at how EColourInfo maps to g_ColourInfoTxt
}

void DrawMaskInfoComboBox(EMaskInfo* pValue)
{
	if (ImGui::BeginCombo("Mask Info", g_MaskInfoTxt[(int)*pValue]))
	{
		for (int i = 0; i < IM_ARRAYSIZE(g_MaskInfoTxt); i++)
		{
			if (ImGui::Selectable(g_MaskInfoTxt[i], (int)*pValue == i))
			{
				*pValue = (EMaskInfo)i;
			}
		}
		ImGui::EndCombo();
	}
}

void DrawColourInfoComboBox(EColourInfo* pValue)
{
	if (ImGui::BeginCombo("Colour Info", g_ColourInfoTxt[(int)*pValue]))
	{
		for (int i = 0; i < IM_ARRAYSIZE(g_ColourInfoTxt); i++)
		{
			if (ImGui::Selectable(g_ColourInfoTxt[i], (int)*pValue == i))
			{
				*pValue = (EColourInfo)i;
			}
		}
		ImGui::EndCombo();
	}
}

void DrawCharacterSetComboBox(FCodeAnalysisState& state, FAddressRef& addr)
{
	const FCharacterSet* pCharSet = addr.IsValid() ? GetCharacterSetFromAddress(addr) : nullptr;
	const FLabelInfo* pLabel = pCharSet != nullptr ? state.GetLabelForAddress(addr) : nullptr;

	const char* pCharSetName = pLabel != nullptr ? pLabel->GetName() : "None";

	if (ImGui::BeginCombo("CharacterSet", pCharSetName))
	{
		if (ImGui::Selectable("None", addr.IsValid() == false))
		{
			addr = FAddressRef();
		}

		for (int i=0;i< GetNoCharacterSets();i++)
		{
			const FCharacterSet* pCharSet = GetCharacterSetFromIndex(i);
			const FLabelInfo* pSetLabel = state.GetLabelForAddress(pCharSet->Params.Address);
			if (pSetLabel == nullptr)
				continue;
			if (ImGui::Selectable(pSetLabel->GetName(), addr == pCharSet->Params.Address))
			{
				addr = pCharSet->Params.Address;
			}
		}

		ImGui::EndCombo();
	}
}

void FCharacterMapViewer::DrawCharacterSetViewer()
{
	FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();
	FCodeAnalysisViewState& viewState = state.GetFocussedViewState();

	if (ImGui::BeginChild("##charsetselect", ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, 0), true))
	{
		int deleteIndex = -1;
		for (int i = 0; i < GetNoCharacterSets(); i++)
		{
			const FCharacterSet* pCharSet = GetCharacterSetFromIndex(i);
			const FLabelInfo* pSetLabel = state.GetLabelForAddress(pCharSet->Params.Address);
			const bool bSelected = CharSetParams.Address == pCharSet->Params.Address;

			if (pSetLabel == nullptr)
				continue;

			ImGui::PushID(i);

			if (ImGui::Selectable(pSetLabel->GetName(), bSelected))
			{
				SelectedCharSetAddr = pCharSet->Params.Address;
				if (CharSetParams.Address != pCharSet->Params.Address)
				{
					CharSetParams = pCharSet->Params;
				}
			}

			if (ImGui::BeginPopupContextItem("char set context menu"))
			{
				if (ImGui::Selectable("Delete"))
				{
					deleteIndex = i;
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		if(deleteIndex != -1)
			DeleteCharacterSet(deleteIndex);
	}

	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("##charsetdetails", ImVec2(0, 0), true))
	{
		FCharacterSet* pCharSet = GetCharacterSetFromAddress(SelectedCharSetAddr);
		if (pCharSet)
		{
			if (DrawAddressInput(state, "Address", CharSetParams.Address))
			{
				//UpdateCharacterSet(state, *pCharSet, params);
			}
			DrawAddressLabel(state, viewState, CharSetParams.Address);
			DrawMaskInfoComboBox(&CharSetParams.MaskInfo);
			DrawBitmapFormatCombo(CharSetParams.BitmapFormat, state);
			if (CharSetParams.BitmapFormat == EBitmapFormat::Bitmap_1Bpp)
			{
				DrawColourInfoComboBox(&CharSetParams.ColourInfo);
				if (CharSetParams.ColourInfo == EColourInfo::MemoryLUT)
				{
					DrawAddressInput(state, "Attribs Address", CharSetParams.AttribsAddress);
				}
			}
			else
			{
				DrawPaletteCombo("Palette", "None", CharSetParams.PaletteNo, GetNumColoursForBitmapFormat(CharSetParams.BitmapFormat));
			}

			ImGui::Checkbox("Dynamic", &CharSetParams.bDynamic);
			if (ImGui::Button("Update Character Set"))
			{
				SelectedCharSetAddr = CharSetParams.Address;
				UpdateCharacterSet(state, *pCharSet, CharSetParams);
			}
			pCharSet->Image->Draw();
		}
	}
	ImGui::EndChild();
}

struct FCharacterMapViewerUIState
{
	FAddressRef				SelectedCharMapAddr;
	FCharMapCreateParams	Params;

	FAddressRef				SelectedCharAddress;
	int						SelectedCharX = -1;
	int						SelectedCharY = -1;
};

// this assumes the character map is in address space
void DrawCharacterMap(FCharacterMapViewerUIState& uiState, FCodeAnalysisState& state, FCodeAnalysisViewState& viewState)
{
	FCharacterMap* pCharMap = GetCharacterMapFromAddress(uiState.SelectedCharMapAddr);

	if (pCharMap == nullptr)
		return;
	
	FCharMapCreateParams& params = uiState.Params;
	
	DrawAddressLabel(state, viewState, uiState.SelectedCharMapAddr);

	// Display and edit params
	DrawAddressInput(state, "Address", params.Address);
	DrawCharacterSetComboBox(state, params.CharacterSet);
	int sz[2] = { params.Width, params.Height };
	if (ImGui::InputInt2("Size (X,Y)", sz))
	{
		params.Width = sz[0];
		params.Height = sz[1];
	}
	DrawU8Input("Null Character", &params.IgnoreCharacter);

	if (ImGui::Button("Apply"))
	{
		pCharMap->Params = params;

		// Reformat Memory
		FDataFormattingOptions formattingOptions;
		formattingOptions.DataType = EDataType::CharacterMap;
		formattingOptions.StartAddress = params.Address;	
		formattingOptions.ItemSize = params.Width;
		formattingOptions.NoItems = params.Height;
		formattingOptions.CharacterSet = params.CharacterSet;
		formattingOptions.EmptyCharNo = params.IgnoreCharacter;
		formattingOptions.AddLabelAtStart = true;
		FormatData(state, formattingOptions);
		state.SetCodeAnalysisDirty(params.Address);
	}

	// Display Character Map
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	uint16_t byte = 0;
	const FCharacterSet* pCharSet = GetCharacterSetFromAddress(params.CharacterSet);
	static bool bShowReadWrites = true;
	const uint16_t physAddress = params.Address.Address;
	float scale = ImGui_GetScaling();
	const float rectSize = 12.0f * scale;

	for (int y = 0; y < params.Height; y++)
	{
		for (int x = 0; x < params.Width; x++)
		{
			const uint8_t val = state.ReadByte(physAddress + byte);
			FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(physAddress + byte);
			const int framesSinceWritten = pDataInfo->LastFrameWritten == -1 ? 255 : state.CurrentFrameNo - pDataInfo->LastFrameWritten;
			const int framesSinceRead = pDataInfo->LastFrameRead == -1 ? 255 : state.CurrentFrameNo - pDataInfo->LastFrameRead;
			const int wBrightVal = (255 - std::min(framesSinceWritten << 3, 255)) & 0xff;
			const int rBrightVal = (255 - std::min(framesSinceRead << 3, 255)) & 0xff;

			if (val != params.IgnoreCharacter || wBrightVal > 0 || rBrightVal > 0)	// skip empty chars
			{
				const float xp = pos.x + (x * rectSize);
				const float yp = pos.y + (y * rectSize);
				ImVec2 rectMin(xp, yp);
				ImVec2 rectMax(xp + rectSize, yp + rectSize);

				if (val != params.IgnoreCharacter)
				{
					if (pCharSet)
					{
						const FCharUVS UVS = pCharSet->GetCharacterUVS(val);
						dl->AddImage((ImTextureID)pCharSet->Image->GetTexture(), rectMin, rectMax, ImVec2(UVS.U0, UVS.V0), ImVec2(UVS.U1, UVS.V1));
					}
					else
					{
						char valTxt[8];
						snprintf(valTxt,8, "%02x", val);
						dl->AddRect(rectMin, rectMax, 0xffffffff);
						dl->AddText(ImVec2(xp + 1, yp + 1), 0xffffffff, valTxt);
						//dl->AddText(NULL, 8.0f, ImVec2(xp + 1, yp + 1), 0xffffffff, valTxt, NULL);
					}
				}

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

			byte++;	// go to next byte
		}
	}

	// draw highlight rect
	const float mousePosX = io.MousePos.x - pos.x;
	const float mousePosY = io.MousePos.y - pos.y;
	if (mousePosX >= 0 && mousePosY >= 0 && mousePosX < (params.Width * rectSize) && mousePosY < (params.Height * rectSize))
	{
		const int xChar = (int)floor(mousePosX / rectSize);
		const int yChar = (int)floor(mousePosY / rectSize);
		const uint16_t charAddress = pCharMap->Params.Address.Address + (xChar + (yChar * pCharMap->Params.Width));
		const uint8_t charVal = state.ReadByte(charAddress);

		const float xp = pos.x + (xChar * rectSize);
		const float yp = pos.y + (yChar * rectSize);
		const ImVec2 rectMin(xp, yp);
		const ImVec2 rectMax(xp + rectSize, yp + rectSize);
		dl->AddRect(rectMin, rectMax, 0xffffffff);

		if (ImGui::IsMouseClicked(0))
		{
			uiState.SelectedCharAddress = FAddressRef(pCharMap->Params.Address.BankId,charAddress);
			uiState.SelectedCharX = xChar;
			uiState.SelectedCharY = yChar;
		}

		// Tool Tip
		ImGui::BeginTooltip();
		ImGui::Text("Char Pos (%d,%d)", xChar, yChar);
		ImGui::Text("Value: %s", NumStr(charVal));
		ImGui::EndTooltip();
	}

	if (uiState.SelectedCharX != -1 && uiState.SelectedCharY != -1)
	{
		const float xp = pos.x + (uiState.SelectedCharX * rectSize);
		const float yp = pos.y + (uiState.SelectedCharY * rectSize);
		const ImVec2 rectMin(xp, yp);
		const ImVec2 rectMax(xp + rectSize, yp + rectSize);
		dl->AddRect(rectMin, rectMax, 0xffffffff);
	}

	// draw hovered address
	if (viewState.HighlightAddress.IsValid())
	{
		//const uint16_t charMapStartAddr = params.Address;
		const uint16_t charMapEndAddr = params.Address.Address + (params.Width * params.Height) - 1;
		// TODO: this needs to use banks
		if (viewState.HighlightAddress.Address >= params.Address.Address && viewState.HighlightAddress.Address <= charMapEndAddr)	// pixel
		{
			const uint16_t addrOffset = viewState.HighlightAddress.Address - params.Address.Address;
			const int charX = addrOffset % params.Width;
			const int charY = addrOffset / params.Width;
			const float xp = pos.x + (charX * rectSize);
			const float yp = pos.y + (charY * rectSize);
			const ImVec2 rectMin(xp, yp);
			const ImVec2 rectMax(xp + rectSize, yp + rectSize);
			dl->AddRect(rectMin, rectMax, 0xffff00ff);
		}
	}

	pos.y += params.Height * rectSize;
	ImGui::SetCursorScreenPos(pos);

	ImGui::Checkbox("Show Reads & Writes", &bShowReadWrites);
	if (uiState.SelectedCharAddress.IsValid())
	{
		// Show data reads & writes
		// 
		FDataInfo* pDataInfo = state.GetDataInfoForAddress(uiState.SelectedCharAddress);
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

void DrawCharacterMaps(FCodeAnalysisState& state, FCodeAnalysisViewState& viewState)
{
	static FCharacterMapViewerUIState uiState;

	if (ImGui::BeginChild("##charmapselect", ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, 0), true))
	{
		int deleteIndex = -1;

		// List character maps
		for (int i = 0; i < GetNoCharacterMaps(); i++)
		{
			const FCharacterMap* pCharMap = GetCharacterMapFromIndex(i);
			const FLabelInfo* pSetLabel = state.GetLabelForAddress(pCharMap->Params.Address);
			const bool bSelected = uiState.SelectedCharMapAddr == pCharMap->Params.Address;

			if (pSetLabel == nullptr)
				continue;

			ImGui::PushID(i);

			if (ImGui::Selectable(pSetLabel->GetName(), bSelected))
			{
				uiState.SelectedCharMapAddr = pCharMap->Params.Address;
				if (uiState.SelectedCharMapAddr != uiState.Params.Address)
				{
					uiState.Params = pCharMap->Params;	// copy params
					uiState.SelectedCharAddress.SetInvalid();
					uiState.SelectedCharX = -1;
					uiState.SelectedCharY = -1;
				}
			}			

			if (ImGui::BeginPopupContextItem("char map context menu"))
			{
				if (ImGui::Selectable("Delete"))
				{
					deleteIndex = i;
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		if(deleteIndex != -1)
			DeleteCharacterMap(deleteIndex);

		
	}
	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("##charmapdetails", ImVec2(0, 0), true))
	{
		if (uiState.SelectedCharMapAddr.IsValid())
		{
			FCodeAnalysisBank* pBank = state.GetBank(uiState.SelectedCharMapAddr.BankId);
			assert(pBank != nullptr);
			state.MapBankForAnalysis(*pBank);	// map bank in so it can be read ok
			DrawCharacterMap(uiState, state, viewState);
			state.UnMapAnalysisBanks();	// map bank out
		}
	}
	ImGui::EndChild();
}


class FCharacterMapGrid : public FMemoryAccessGrid
{
public:
	FCharacterMapGrid(FCodeAnalysisState* pCodeAnalysis, int xGridSize, int yGridSize):FMemoryAccessGrid(pCodeAnalysis,xGridSize,yGridSize)
	{
		bShowValues = true;
		bShowReadWrites = true;
		bOutlineAllSquares = true;	
	}

	FAddressRef GetGridSquareAddress(int x, int y) override
	{
		const int offset = x + (y * GridSizeX);
		FAddressRef squareAddress = Address;
		if(CodeAnalysis->AdvanceAddressRef(squareAddress,offset))
			return squareAddress;
		else
		return FAddressRef();
	}

	void OnDraw() override
	{
		const float scale = ImGui_GetScaling();
		const float kNumSize = 80.0f * scale;	// size for number GUI widget
		ImGui::Text("Grid Size");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(kNumSize);
		ImGui::InputInt("##GridSizeX", &GridSizeX);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(kNumSize);
		ImGui::InputInt("##GridSizeY", &GridSizeY);
		
		ImGui::SetNextItemWidth(120.0f * scale);
		bool bUpdated = false;
		if (GetNumberDisplayMode() == ENumberDisplayMode::Decimal)
			bUpdated |= ImGui::InputInt("##Address", &PhysicalAddress, 1, 8, ImGuiInputTextFlags_CharsDecimal);
		else
			bUpdated |= ImGui::InputInt("##Address", &PhysicalAddress, 1, 8, ImGuiInputTextFlags_CharsHexadecimal);
		if(bUpdated)
			Address = CodeAnalysis->AddressRefFromPhysicalAddress(PhysicalAddress);
		if(Address.IsValid())
			DrawAddressLabel(*CodeAnalysis, CodeAnalysis->GetFocussedViewState(), Address);
		ImGui::SameLine();
		ImGui::Checkbox("##UseIgnoreValue", &bUseIgnoreValue);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(16.0f * scale);
		ImGui::InputScalar("Ignore Value", ImGuiDataType_U8,&IgnoreValue);

		// create and format a character map
		if (ImGui::Button("Create"))
		{
			FDataFormattingOptions formattingOptions;
			formattingOptions.StartAddress = Address;
			formattingOptions.DataType = EDataType::CharacterMap;
			formattingOptions.ItemSize = GridSizeX;
			formattingOptions.NoItems = GridSizeY;
			formattingOptions.ClearCodeInfo = true;
			formattingOptions.ClearLabels = true;
			formattingOptions.AddLabelAtStart = true;
			FormatData(*CodeAnalysis, formattingOptions);
			CodeAnalysis->SetCodeAnalysisDirty(formattingOptions.StartAddress);
		}

		GridSquareSize = 14.0f * scale;	// to fit an 8x8 square on a scaling screen image
	}

	void SetAddress(FAddressRef addr) 
	{ 
		Address = addr; 
		PhysicalAddress = addr.Address;
	}

	void FixupAddressRefs() override
	{
		FMemoryAccessGrid::FixupAddressRefs();
		FixupAddressRef(*CodeAnalysis, Address);
		SetAddress(Address);
	}

	FAddressRef Address;
	int PhysicalAddress = 0;
};

bool	FCharacterMapViewer::Init(void) 
{ 
	ViewerGrid = new FCharacterMapGrid(&pEmulator->GetCodeAnalysis(), 16, 16);

	return true; 
}

void	FCharacterMapViewer::Shutdown() 
{

}

void FCharacterMapViewer::DrawCharacterMapViewer(void)
{
	FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();
	
	ViewerGrid->Draw();
}

void FCharacterMapViewer::GoToAddress(FAddressRef addr)
{
	ViewerGrid->SetAddress(addr);
}

void FCharacterMapViewer::SetGridSize(int x, int y)
{
	ViewerGrid->SetGridSize(x,y);
}


void FCharacterMapViewer::DrawUI(void)
{
	FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();
	FCodeAnalysisViewState& viewState = state.GetFocussedViewState();

	if (ImGui::BeginTabBar("CharacterMapTabs"))
	{
		if (ImGui::BeginTabItem("Character Map Viewer"))
		{
			DrawCharacterMapViewer();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Character Sets"))
		{
			DrawCharacterSetViewer();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Character Maps"))
		{
			DrawCharacterMaps(state, viewState);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void FCharacterMapViewer::FixupAddressRefs()
{
	ViewerGrid->FixupAddressRefs();
	FixupAddressRef(pEmulator->GetCodeAnalysis(), SelectedCharSetAddr);
	FixupAddressRef(pEmulator->GetCodeAnalysis(), CharSetParams.Address);
	FixupAddressRef(pEmulator->GetCodeAnalysis(), CharSetParams.AttribsAddress);

	// todo FCharacterMapViewerUIState
}