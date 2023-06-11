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
	ImGui::Checkbox("Include video memory", &bDiffVideoMem);
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

