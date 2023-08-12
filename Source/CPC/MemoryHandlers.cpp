#include "MemoryHandlers.h"

#include "CpcEmu.h"
#include "CodeAnalyser/UI/CodeAnalyserUI.h"
#include <Util/Misc.h>

#include "misc/cpp/imgui_stdlib.h"
#include <cstdint>

int MemoryHandlerTrapFunction(uint16_t pc, int ticks, uint64_t pins, FCpcEmu* pEmu)
{
	const uint16_t addr = Z80_GET_ADDR(pins);
	const bool bRead = (pins & Z80_CTRL_PIN_MASK) == (Z80_MREQ | Z80_RD);
	const bool bWrite = (pins & Z80_CTRL_PIN_MASK) == (Z80_MREQ | Z80_WR);
	
	FCodeInfo* pCodeInfo = pEmu->CodeAnalysis.GetCodeInfoForAddress(pc);
	const FAddressRef PCaddrRef = pEmu->CodeAnalysis.AddressRefFromPhysicalAddress(pc);

	// increment counters
	//pEmu->MemStats.ExecCount[pc]++;
	//const int op_len = DisasmLen(pEmu, pc);
	for (int i = 0; i < pCodeInfo->ByteSize; i++) 
		pEmu->MemStats.ExecCount[(pc + i) & 0xFFFF]++;

	if (bRead)
		pEmu->MemStats.ReadCount[addr]++;
	if (bWrite)
		pEmu->MemStats.WriteCount[addr]++;

	// See if we can find a handler
	for (auto& handler : pEmu->MemoryAccessHandlers)
	{
		if (handler.bEnabled == false)
			continue;

		bool bCallHandler = false;

		if (handler.Type == MemoryAccessType::Execute)	// Execution
		{
			if (pc >= handler.MemStart && pc <= handler.MemEnd)
				bCallHandler = true;
		}
		else // Memory access
		{
			if (addr >= handler.MemStart && addr <= handler.MemEnd)
			{
				bool bExecute = false;

				if (handler.Type == MemoryAccessType::Read && bRead)
					bCallHandler = true;
				else if (handler.Type == MemoryAccessType::Write && bWrite)
					bCallHandler = true;
			}
		}

		if (bCallHandler)
		{
			// update handler stats
			handler.TotalCount++;
			handler.Callers.RegisterAccess(PCaddrRef);
#if SPECCY
			if (handler.pHandlerFunction != nullptr)
				handler.pHandlerFunction(handler, pEmu->pActiveGame, pc, pins);
#endif
			if (handler.bBreak)
				return UI_DBG_STEP_TRAPID;
		}
	}
	
	assert(!(bRead == true && bWrite == true));

	return 0;
}

#if 0
MemoryUse DetermineAddressMemoryUse(const FMemoryStats &memStats, uint16_t addr, bool &smc)
{
	const bool bCode = memStats.ExecCount[addr] > 0;
	const bool bData = memStats.ReadCount[addr] > 0 || memStats.WriteCount[addr] > 0;

	if (bCode && memStats.WriteCount[addr] > 0)
	{
		smc = true;
	}

	if (bCode)
		return MemoryUse::Code;
	if (bData)
		return MemoryUse::Data;

	return MemoryUse::Unknown;
}

void AnalyseMemory(FMemoryStats &memStats)
{
	FMemoryBlock currentBlock;
	bool bSelfModifiedCode = false;

	memStats.MemoryBlockInfo.clear();	// Clear old list
	memStats.CodeAndDataList.clear();

	currentBlock.StartAddress = 0;
	currentBlock.Use = DetermineAddressMemoryUse(memStats, 0, bSelfModifiedCode);
	if (bSelfModifiedCode)
		memStats.CodeAndDataList.push_back(0);

	for (int addr = 1; addr < (1<<16); addr++)
	{
		bSelfModifiedCode = false;
		const MemoryUse addrUse = DetermineAddressMemoryUse(memStats, addr, bSelfModifiedCode);
		if (bSelfModifiedCode)
			memStats.CodeAndDataList.push_back(addr);
		if (addrUse != currentBlock.Use)
		{
			currentBlock.EndAddress = addr - 1;
			memStats.MemoryBlockInfo.push_back(currentBlock);

			// start new block
			currentBlock.StartAddress = addr;
			currentBlock.Use = addrUse;
		}
	}

	// finish off last block
	currentBlock.EndAddress = 0xffff;
	memStats.MemoryBlockInfo.push_back(currentBlock);
}
#endif

void ResetMemoryStats(FMemoryStats &memStats)
{
	memStats.MemoryBlockInfo.clear();	// Clear list
	// 
	// reset counters
	memset(memStats.ExecCount, 0, sizeof(memStats.ExecCount));
	memset(memStats.ReadCount, 0, sizeof(memStats.ReadCount));
	memset(memStats.WriteCount, 0, sizeof(memStats.WriteCount));
}

#if 0
// UI
void DrawMemoryHandlers(FCpcEmu* pCpcEmu)
{
	FCodeAnalysisViewState& viewState = pCpcEmu->CodeAnalysis.GetFocussedViewState();
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginChild("DrawMemoryHandlersGUIChild1", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.25f, 0), false, window_flags);
	FMemoryAccessHandler *pSelectedHandler = nullptr;

	for (auto &handler : pCpcEmu->MemoryAccessHandlers)
	{
		const bool bSelected = pCpcEmu->SelectedMemoryHandler == handler.Name;
		if (bSelected)
		{
			pSelectedHandler = &handler;
		}

		if (ImGui::Selectable(handler.Name.c_str(), bSelected))
		{
			pCpcEmu->SelectedMemoryHandler = handler.Name;
		}

	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Handler details
	ImGui::BeginChild("DrawMemoryHandlersGUIChild2", ImVec2(0, 0), false, window_flags);
	if (pSelectedHandler != nullptr)
	{
		ImGui::Checkbox("Enabled", &pSelectedHandler->bEnabled);
		ImGui::Checkbox("Break", &pSelectedHandler->bBreak);
		ImGui::Text(pSelectedHandler->Name.c_str());

		ImGui::Text("Start: %s", NumStr(pSelectedHandler->MemStart));
		ImGui::SameLine();
		DrawAddressLabel(pCpcEmu->CodeAnalysis, viewState, pSelectedHandler->MemStart);

		ImGui::Text("End: %s", NumStr(pSelectedHandler->MemEnd));
		ImGui::SameLine();
		DrawAddressLabel(pCpcEmu->CodeAnalysis, viewState, pSelectedHandler->MemEnd);

		ImGui::Text("Total Accesses %d", pSelectedHandler->TotalCount);

		ImGui::Text("Callers");
		for (const auto& accessPC : pSelectedHandler->Callers.GetReferences())
		{
			ImGui::PushID(accessPC.Val);
			DrawCodeAddress(pCpcEmu->CodeAnalysis, viewState, accessPC);
			//ImGui::SameLine();
			//ImGui::Text(" - %d accesses",accessPC.second);
			ImGui::PopID();
		}
	}

	ImGui::EndChild();
}

void DrawMemoryAnalysis(FCpcEmu* pUI)
{
	ImGui::Text("Memory Analysis");
	if (ImGui::Button("Analyse"))
	{
		AnalyseMemory(pUI->MemStats);	// non-const on purpose
	}
	const FMemoryStats& memStats = pUI->MemStats;
	ImGui::Text("%d self modified code points", (int)memStats.CodeAndDataList.size());
	ImGui::Text("%d blocks", (int)memStats.MemoryBlockInfo.size());
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	if (ImGui::BeginChild("DrawMemoryAnalysisChild1", ImVec2(0, 0), false, window_flags))
	{
		for (const auto &memblock : memStats.MemoryBlockInfo)
		{
			const char *pTypeStr = "Unknown";
			if (memblock.Use == MemoryUse::Code)
				pTypeStr = "Code";
			else if (memblock.Use == MemoryUse::Data)
				pTypeStr = "Data";
			ImGui::Text("%s", pTypeStr);
			ImGui::SameLine(100);
			ImGui::Text("0x%x - 0x%x", memblock.StartAddress, memblock.EndAddress);
		}
	}
	ImGui::EndChild();

}
#endif

void FDiffTool::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
}


void FDiffTool::DrawUI()
{
	FCodeAnalysisViewState& viewState = pCpcEmu->CodeAnalysis.GetFocussedViewState();
	
	if(ImGui::Button("SnapShot"))
	{
		for (int addr = 0; addr < (1 << 16); addr++)
		{
		  if (addr < pCpcEmu->GetScreenAddrStart() || addr > pCpcEmu->GetScreenAddrEnd())
		  {
			 DiffSnapShotMemory[addr] = pCpcEmu->ReadWritableByte(addr);
			 SnapShotGfxMemStart = pCpcEmu->GetScreenAddrStart();
			 SnapShotGfxMemEnd = pCpcEmu->GetScreenAddrEnd();
		  }
		}
		bSnapshotAvailable = true;
		DiffChangedLocations.clear();
	}
	
	if (bSnapshotAvailable)
	{
		ImGui::SameLine();

		if (ImGui::Button("Diff"))
		{
			DiffChangedLocations.clear();
			for (int addr = DiffStartAddr; addr < DiffEndAddr; addr++)
			{
				bool bIsOrWasGfxMem = false;

				if (!bDiffVideoMem)
				{
					if (addr >= pCpcEmu->GetScreenAddrStart() && addr <= pCpcEmu->GetScreenAddrEnd())
						bIsOrWasGfxMem = true;

					if (addr >= SnapShotGfxMemStart && addr <= SnapShotGfxMemEnd)
						bIsOrWasGfxMem = true;
				}

				if (!bIsOrWasGfxMem)
				{
					if (pCpcEmu->ReadWritableByte(addr) != DiffSnapShotMemory[addr])
						DiffChangedLocations.push_back(addr);
				}
			}
		}
	}

	ImGui::SameLine();
	ImGui::Checkbox("Include Graphics Memory", &bDiffVideoMem);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
		
	ImGui::Text("Address range");
	bool bHex = GetNumberDisplayMode() != ENumberDisplayMode::Decimal;
	const char* formatStr = bHex ? "%x" : "%u";
	ImGuiInputTextFlags flags = bHex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal;
	ImGui::SetNextItemWidth(100);
	ImGui::InputScalar("Start", ImGuiDataType_U16, &DiffStartAddr, NULL, NULL, formatStr, flags);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::InputScalar("End", ImGuiDataType_U16, &DiffEndAddr, NULL, NULL, formatStr, flags);
	
	if(ImGui::BeginChild("DiffedMemory", ImVec2(0, 0), true, window_flags))
	{
		if (bSnapshotAvailable)
		{
			ImGui::Text("Differences: %d", DiffChangedLocations.size());
			if (DiffChangedLocations.size())
			{
				if (ImGui::Button("Comment All"))
				{
					for (const uint16_t changedAddr : DiffChangedLocations)
					{
						if (FDataInfo* pWriteDataInfo = pCpcEmu->CodeAnalysis.GetWriteDataInfoForAddress(changedAddr))
						{
							if (bReplaceExistingComment || pWriteDataInfo->Comment.empty())
								pWriteDataInfo->Comment = CommentText;
							else
							{
								if (!CommentText.empty())
								{
									if (!pWriteDataInfo->Comment.empty())
										pWriteDataInfo->Comment += "; ";
									pWriteDataInfo->Comment += CommentText;
								}
							}
						}
					}
				}
				
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
				{
					ImGui::SetTooltip("Add comment to disassembly for each modified memory location");
				}

				ImGui::SameLine();
				ImGui::SetNextItemWidth(250);
				ImGui::InputText("##comment", &CommentText, ImGuiInputTextFlags_AutoSelectAll);
				
				ImGui::SameLine();
				ImGui::Checkbox("Override existing", &bReplaceExistingComment);

				ImGui::Separator();
			}
		}
		else
			ImGui::Text("Take snapshot to begin...");

		for(const uint16_t changedAddr : DiffChangedLocations)
		{
			ImGui::PushID(changedAddr);
			if(ImGui::Selectable("##diffaddr",DiffSelectedAddr == changedAddr))
			{
				DiffSelectedAddr = changedAddr;
			}
		
			ImGui::SetItemAllowOverlap();	// allow buttons
			ImGui::SameLine();
			ImGui::Text("%s\t%s\t%s", NumStr(changedAddr), NumStr(DiffSnapShotMemory[changedAddr]), NumStr(pCpcEmu->ReadWritableByte(changedAddr)));
			ImGui::SameLine();
			DrawAddressLabel(pCpcEmu->CodeAnalysis, viewState, changedAddr);
			
			if (const FDataInfo* pWriteDataInfo = pCpcEmu->CodeAnalysis.GetWriteDataInfoForAddress(changedAddr))
			{
				DrawComment(pWriteDataInfo);
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}

void FDataFindTool::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;
	pCurFinder = &ByteFinder;
	ByteFinder.Init(pEmu);
	WordFinder.Init(pEmu);
	TextFinder.Init(pEmu);
}

void FDataFindTool::Reset()
{
	ByteFinder.Reset();
	WordFinder.Reset();
	TextFinder.Reset();
}

// todo:
//   search all memory banks - not just address range
//	 investigate getwritedata when displaying accessor indicator
//   unacessed memory tooltip incorrect
void FDataFindTool::DrawUI()
{
	FCodeAnalysisViewState& viewState = pCpcEmu->CodeAnalysis.GetFocussedViewState();
	const ESearchType lastSearchType = SearchType;
	const ESearchDataType lastDataSize = DataSize;

	if (ImGui::RadioButton("Decimal Value", SearchType == ESearchType::Value && bDecimal == true))
	{
		SearchType = ESearchType::Value;
		bDecimal = true;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Hexadecimal Value", SearchType == ESearchType::Value && bDecimal == false))
	{
		SearchType = ESearchType::Value;
		bDecimal = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Text", SearchType == ESearchType::Text))
	{
		SearchType = ESearchType::Text;
	}
	if (ImGui::RadioButton("Byte", DataSize == ESearchDataType::Byte))
	{
		DataSize = ESearchDataType::Byte;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Word", DataSize == ESearchDataType::Word))
	{
		DataSize = ESearchDataType::Word;
	}
 
	if (lastSearchType != SearchType || lastDataSize != DataSize)
	{
		if (SearchType == ESearchType::Text)
			pCurFinder = &TextFinder;
		else
			pCurFinder = DataSize == ESearchDataType::Byte ? (FFinder*)&ByteFinder : &WordFinder;
	}

#if 0
	if (SearchType == ESearchType::Value)
	{
		const char* dataTypes[] = { "Byte", "Word" };
		const char* combo_preview_value = dataTypes[DataSize];  // Pass in the preview value visible before opening the combo (it could be anything)
		if (ImGui::BeginCombo("Data Type", combo_preview_value, 0))
		{
			// todo: clear results if change drop down?

			for (int n = 0; n < IM_ARRAYSIZE(dataTypes); n++)
			{
				const bool isSelected = (DataSize == n);
				if (ImGui::Selectable(dataTypes[n], isSelected))
					DataSize = (ESearchDataType)n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
#endif

	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Search Options"))
	{
		if (SearchType == ESearchType::Value)
		{
			if (ImGui::RadioButton("Data", Options.MemoryType == ESearchMemoryType::Data))
			{
				Options.MemoryType = ESearchMemoryType::Data;
			}
			ImGui::SameLine();
			
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			{
				ImGui::SetTooltip("Search only memory locations marked as data.");
			}
			
			if (ImGui::RadioButton("Code", Options.MemoryType == ESearchMemoryType::Code))
			{
				Options.MemoryType = ESearchMemoryType::Code;
			}
			ImGui::SameLine();
			
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			{
				ImGui::SetTooltip("Search only memory locations marked as code.");
			}
			
			if (ImGui::RadioButton("Code & Data", Options.MemoryType == ESearchMemoryType::CodeAndData))
			{
				Options.MemoryType = ESearchMemoryType::CodeAndData;
			}
			
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			{
				ImGui::SetTooltip("Search all memory locations - both code and data.");
			}

			ImGui::Checkbox("Search Graphics Memory", &Options.bSearchGraphicsMem);
		}

		ImGui::Checkbox("Search Unaccessed Memory", &Options.bSearchUnaccessed);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		{
			// todo this can return results when emulator not run. does it load accesses from disk?
			ImGui::SetTooltip("Include search results from memory locations that have not been accessed this session.");
		}

		ImGui::Checkbox("Search Memory With No References", &Options.bSearchUnreferenced);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		{
			ImGui::SetTooltip("Include search results from memory locations that have no references.");
		}
		ImGui::TreePop();
	}
	
	bool bPressedEnter = false;
	if (SearchType == ESearchType::Value)
	{
		const char* formatStr = bDecimal ? "%u" : "%x";
		ImGuiInputTextFlags flags = bDecimal ? ImGuiInputTextFlags_CharsDecimal : ImGuiInputTextFlags_CharsHexadecimal;

		ImGui::Text("Search Value");
		ImGui::SameLine();

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
		if (DataSize == ESearchDataType::Byte)
		{ 
			ImGui::InputScalar("##bytevalue", ImGuiDataType_U8, &ByteFinder.SearchValue, NULL, NULL, formatStr, flags);
		}
		else
		{
			ImGui::InputScalar("##wordvalue", ImGuiDataType_U16, &WordFinder.SearchValue, NULL, NULL, formatStr, flags);
		}
	}
	else if (SearchType == ESearchType::Text)
	{
		ImGui::Text("Search Text");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
		ImGui::InputText("##searchtext", &TextFinder.SearchText);
	}

	// sam. I wanted to use ImGuiInputTextFlags_EnterReturnsTrue here to do the search when enter is pressed but
	// I ran into a bug where when clicking away from the input box would revert the value. 
	// https://github.com/ocornut/imgui/issues/6284
	// Doing this as a workaround:
	if (ImGui::IsItemFocused())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
		{
			bPressedEnter = true;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Find") || bPressedEnter)
	{
		pCurFinder->Find(Options);
	}

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	const ImVec2 childSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * (SearchType == ESearchType::Value ? 2.0f : 1.0f)); // Leave room for 1 or 2 lines below us
	if (ImGui::BeginChild("SearchResults", childSize, true, window_flags)) 
	{
		if (pCurFinder->SearchResults.size())
		{
			static const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
			static const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

			static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
			const int numColumns = SearchType == ESearchType::Text ? 2 : 3;
			if (ImGui::BeginTable("Events", numColumns, flags))
			{
				ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
				if (SearchType == ESearchType::Value)
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 5);
				ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				//const float lineHeight = ImGui::GetTextLineHeight(); // this breaks the clipper and makes it impossible to see the last few rows.
				ImGuiListClipper clipper((int)pCurFinder->SearchResults.size()/*, lineHeight*/);

				bool bUserPrefersHexAitch = GetNumberDisplayMode() == ENumberDisplayMode::HexAitch;
				const ENumberDisplayMode numberMode = bDecimal ? ENumberDisplayMode::Decimal : bUserPrefersHexAitch ? ENumberDisplayMode::HexAitch : ENumberDisplayMode::HexDollar;
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
					{
						const uint16_t resultAddr = pCurFinder->SearchResults[i];
						ImGui::PushID(i);
						ImGui::TableNextRow();

						ImGui::TableNextColumn();

						bool bValueChanged = pCurFinder->HasValueChanged(resultAddr);
						if (bValueChanged)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
						}

						// Address
						if (const FDataInfo* pWriteDataInfo = pCpcEmu->CodeAnalysis.GetWriteDataInfoForAddress(resultAddr))
						{
							ShowDataItemActivity(pCpcEmu->CodeAnalysis, pCpcEmu->CodeAnalysis.AddressRefFromPhysicalAddress(resultAddr));
						}

						ImGui::Text("    %s", NumStr(resultAddr));
						ImGui::SameLine();
						DrawAddressLabel(pCpcEmu->CodeAnalysis, viewState, resultAddr);

						// Value
						if (SearchType == ESearchType::Value)
						{
							ImGui::TableNextColumn();
							if (SearchType == ESearchType::Value)
							{
								ImGui::Text ("%s", pCurFinder->GetValueString(resultAddr, numberMode));
							}
						}

						// Comment
						ImGui::TableNextColumn();
						if (const FDataInfo* pWriteDataInfo = pCpcEmu->CodeAnalysis.GetWriteDataInfoForAddress(resultAddr))
						{
							// what if this is code?
							DrawComment(pWriteDataInfo);
						}
						
						if (bValueChanged)
							ImGui::PopStyleColor();

						ImGui::PopID();
					}
				}
				ImGui::EndTable();
			}
		}
	} 
	ImGui::EndChild();
	ImGui::Text("%d results found", pCurFinder->SearchResults.size());

	if (SearchType == ESearchType::Value)
	{
		if (ImGui::Button("Remove Unchanged Results"))
		{
			pCurFinder->RemoveUnchangedResults();
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		{
			ImGui::SetTooltip("Remove unchanged results todo.");
		}
	}
}

void FFinder::Reset()
{
	SearchResults.clear();
}

bool FFinder::HasValueChanged(uint16_t addr) const
{
	return false;
}

void FFinder::Find(const FSearchOptions& opt)
{
	SearchResults.clear();

	uint16_t curSearchAddr = 0;

	while (FindNextMatch(curSearchAddr, curSearchAddr))
	{
		bool bAddResult = true;
		const FDataInfo* pAnyDataInfo = nullptr;
		const FCodeInfo* pCodeInfo = pCpcEmu->CodeAnalysis.GetCodeInfoForAddress(curSearchAddr);
		{
			const FDataInfo* pWriteDataInfo = pCpcEmu->CodeAnalysis.GetWriteDataInfoForAddress(curSearchAddr);
			const FDataInfo* pReadDataInfo = pCpcEmu->CodeAnalysis.GetReadDataInfoForAddress(curSearchAddr);
			pAnyDataInfo = pWriteDataInfo ? pWriteDataInfo : pReadDataInfo;
		}
		const bool bIsCode = pCodeInfo || (pAnyDataInfo && pAnyDataInfo->DataType == EDataType::InstructionOperand);
			
		if (opt.MemoryType == ESearchMemoryType::Code)
		{
			bAddResult = bIsCode;
		}
		else if (opt.MemoryType == ESearchMemoryType::Data)
		{
			bAddResult = !bIsCode;
		}

		if (bAddResult)
		{
			if (pAnyDataInfo)
			{
				if (!opt.bSearchUnaccessed)
				{
					if (pAnyDataInfo->LastFrameRead == -1 && pAnyDataInfo->LastFrameWritten == -1)
						bAddResult = false;
				}

				if (!opt.bSearchUnreferenced)
				{
					if (pAnyDataInfo->Reads.IsEmpty() && pAnyDataInfo->Writes.IsEmpty())
					{
						bAddResult = false;
					}
				}
			}
		}

		if (bAddResult)
		{
			if (!opt.bSearchGraphicsMem)
			{
				if (curSearchAddr >= pCpcEmu->GetScreenAddrStart() && curSearchAddr <= pCpcEmu->GetScreenAddrEnd())
					bAddResult = false;
			}
		}

		if (bAddResult)
		{
			SearchResults.push_back(curSearchAddr);
		}
		curSearchAddr++;
		if (curSearchAddr == 0)
			break;
	}
}

bool FByteFinder::FindNextMatch(uint16_t offset, uint16_t& outAddr)
{
	return pCpcEmu->CodeAnalysis.FindMemoryPatternInPhysicalMemory(&SearchValue, 1, offset, outAddr);
}

bool FByteFinder::HasValueChanged(uint16_t addr) const
{
	const uint8_t curValue = pCpcEmu->ReadWritableByte(addr);
	return curValue != LastValue;
}

const char* FByteFinder::GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const
{
	const uint8_t value = pCpcEmu->ReadWritableByte(addr);
	return NumStr(value, numberMode);
}

void FByteFinder::RemoveUnchangedResults()
{
	for (auto it = SearchResults.begin(); it != SearchResults.end(); )
	{
		const uint8_t value = pCpcEmu->ReadWritableByte(*it);
		if (value == LastValue)
			it = SearchResults.erase(it);
		else
			++it;
	}
}

bool FWordFinder::FindNextMatch(uint16_t offset, uint16_t& outAddr)
{
	return pCpcEmu->CodeAnalysis.FindMemoryPatternInPhysicalMemory(SearchBytes, 2, offset, outAddr);
}

bool FWordFinder::HasValueChanged(uint16_t addr) const
{
	const uint16_t curValue = pCpcEmu->ReadWritableWord(addr);
	return curValue != LastValue;
}

const char* FWordFinder::GetValueString(uint16_t addr, ENumberDisplayMode numberMode) const
{
	const uint16_t value = pCpcEmu->ReadWritableWord(addr);
	return NumStr(value, numberMode);
}

void FWordFinder::RemoveUnchangedResults()
{
	for (auto it = SearchResults.begin(); it != SearchResults.end(); )
	{
		const uint16_t value = pCpcEmu->ReadWritableWord(*it);
		if (value == LastValue)
			it = SearchResults.erase(it);
		else
			++it;
	}
}

bool FTextFinder::FindNextMatch(uint16_t offset, uint16_t& outAddr)
{
	return pCpcEmu->CodeAnalysis.FindMemoryPatternInPhysicalMemory((uint8_t*)SearchText.c_str(), SearchText.size(), offset, outAddr);
}

