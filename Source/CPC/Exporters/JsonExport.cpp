#include "JsonExport.h"
#include "CodeAnalyser/CodeAnalyser.h"

#if SPECCY
#include "../SpectrumConstants.h"
#endif

#include "../CpcEmu.h"

#include <json.hpp>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <Util/GraphicsView.h>
#include <Debug/DebugLog.h>

using json = nlohmann::json;

// We want to eventually move to using Json as it will allow merging 
void WriteAddressRangeToJson(FCodeAnalysisState& state, int startAddress, int endAddress, json& jsonDoc);
void WritePageToJson(const FCodeAnalysisPage& page, json& jsonDoc);
void ReadBankFromJson(FCpcEmu* pCpcEmu, int bankNo, const json& jsonDoc);

#if SPECCY
// todo this wont work on cpc
bool ExportROMJson(FCodeAnalysisState& state, const char* pJsonFileName)
{
	json jsonROMData;

	WriteAddressRangeToJson(state, 0, 0x3fff, jsonROMData);

	std::ofstream outFileStream(pJsonFileName);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonROMData << std::endl;
		return true;
	}

	return false;
}
#endif

#define WRITE_PAGES 1

bool ExportGameJson(FCpcEmu* pCpcEmu, const char* pJsonFileName)
{
	FCodeAnalysisState& state = pCpcEmu->CodeAnalysis;
	json jsonGameData;

	// Write watches
	for (const auto& watch : state.GetWatches())
	{
		jsonGameData["Watches"].push_back(watch);
	}

#if SPECCY
	// Write character sets
	for (int i = 0; i < GetNoCharacterSets(); i++)
	{
		const FCharacterSet* pCharSet = GetCharacterSetFromIndex(i);
		json jsonCharacterSet;

		jsonCharacterSet["Address"] = pCharSet->Params.Address;
		jsonCharacterSet["AttribsAddress"] = pCharSet->Params.AttribsAddress;
		jsonCharacterSet["MaskInfo"] = pCharSet->Params.MaskInfo;
		jsonCharacterSet["ColourInfo"] = pCharSet->Params.ColourInfo;
		jsonCharacterSet["Dynamic"] = pCharSet->Params.bDynamic;

		jsonGameData["CharacterSets"].push_back(jsonCharacterSet);
	}

	// Write character maps
	for (int i = 0; i < GetNoCharacterMaps(); i++)
	{
		const FCharacterMap* pCharMap = GetCharacterMapFromIndex(i);
		json jsonCharacterMap;

		jsonCharacterMap["Address"] = pCharMap->Params.Address;
		jsonCharacterMap["Width"] = pCharMap->Params.Width;
		jsonCharacterMap["Height"] = pCharMap->Params.Height;
		jsonCharacterMap["CharacterSet"] = pCharMap->Params.CharacterSet;
		jsonCharacterMap["IgnoreCharacter"] = pCharMap->Params.IgnoreCharacter;

		jsonGameData["CharacterMaps"].push_back(jsonCharacterMap);
	}
#endif

#if WRITE_PAGES
	int pagesWritten = 0;
	for (int pageNo = 0; pageNo < FCpcEmu::kNoRAMPages; pageNo++)
	{
		FCodeAnalysisPage& page = pCpcEmu->RAMPages[pageNo];
		if (page.bUsed)
		{
			json pageData;

			WritePageToJson(page, pageData);
			jsonGameData["Pages"].push_back(pageData);
			pagesWritten++;
		}
	}

	LOGINFO("%d pages written", pagesWritten);
#else
	// write out RAM
	const int startAddress = 0x4000;
	const int endAddress = 0xffff;

	WriteAddressRangeToJson(state, startAddress, endAddress, jsonGameData);
#endif

	// Write file out
	std::ofstream outFileStream(pJsonFileName);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonGameData << std::endl;
		return true;
	}

	return false;
}

bool WriteDataInfoToJson(const FDataInfo* pDataInfo, json& jsonDoc, int addressOverride = -1)
{
	json dataInfoJson;

	assert(pDataInfo != nullptr);
	if (pDataInfo->DataType != EDataType::Byte)
		dataInfoJson["DataType"] = (int)pDataInfo->DataType;
	if (pDataInfo->OperandType != EOperandType::Unknown)
		dataInfoJson["OperandType"] = pDataInfo->OperandType;
	if (pDataInfo->ByteSize != 1)
		dataInfoJson["ByteSize"] = pDataInfo->ByteSize;
	if (pDataInfo->Flags != 0)
		dataInfoJson["Flags"] = pDataInfo->Flags;
	if (pDataInfo->Comment.empty() == false)
		dataInfoJson["Comment"] = pDataInfo->Comment;

	for (const auto& read : pDataInfo->Reads)
		dataInfoJson["Reads"].push_back(read.first);

	for (const auto& write : pDataInfo->Writes)
		dataInfoJson["Writes"].push_back(write.first);

#if SPECCY
	// Charmap specific
	if (pDataInfo->DataType == EDataType::CharacterMap)
	{
		dataInfoJson["CharSetAddress"] = pDataInfo->CharSetAddress;
		dataInfoJson["EmptyCharNo"] = pDataInfo->EmptyCharNo;
	}
#endif

	if (dataInfoJson.size() != 0)	// only write it if it deviates from the normal
	{
		dataInfoJson["Address"] = addressOverride == -1 ? pDataInfo->Address : addressOverride;
		jsonDoc["DataInfo"].push_back(dataInfoJson);
		return true;
	}

	return false;
}

void WriteCodeInfoToJson(const FCodeInfo* pCodeInfoItem, json& jsonDoc, int addressOverride = -1)
{
	json codeInfoJson;
	codeInfoJson["Address"] = addressOverride == -1 ? pCodeInfoItem->Address : addressOverride;
	codeInfoJson["ByteSize"] = pCodeInfoItem->ByteSize;
	if (pCodeInfoItem->bSelfModifyingCode)
		codeInfoJson["SMC"] = true;
	if (pCodeInfoItem->OperandType != EOperandType::Unknown)
		codeInfoJson["OperandType"] = (int)pCodeInfoItem->OperandType;
	if (pCodeInfoItem->Flags != 0)
		codeInfoJson["Flags"] = pCodeInfoItem->Flags;
	if (pCodeInfoItem->Comment.empty() == false)
		codeInfoJson["Comment"] = pCodeInfoItem->Comment;

	jsonDoc["CodeInfo"].push_back(codeInfoJson);
}

void WriteLabelInfoToJson(uint16_t addr, const FLabelInfo* pLabelInfo, json& jsonDoc, int addressOverride = -1)
{
	json labelInfoJson;
	labelInfoJson["Address"] = addressOverride == -1 ? addr : addressOverride;
	labelInfoJson["Name"] = pLabelInfo->Name;
	if (pLabelInfo->Global)
		labelInfoJson["Global"] = pLabelInfo->Global;
	labelInfoJson["LabelType"] = pLabelInfo->LabelType;
	if (pLabelInfo->Comment.empty() == false)
		labelInfoJson["Comment"] = pLabelInfo->Comment;

	for (const auto& reference : pLabelInfo->References)
		labelInfoJson["References"].push_back(reference.first);

	jsonDoc["LabelInfo"].push_back(labelInfoJson);
}

void WriteCommentBlockToJson(uint16_t addr, const FCommentBlock* pCommentBlock, json& jsonDoc, int addressOverride = -1)
{
	if (pCommentBlock->Comment.empty())
		return;

	json commentBlockJson;
	commentBlockJson["Address"] = addressOverride == -1 ? addr : addressOverride;
	commentBlockJson["Comment"] = pCommentBlock->Comment;

	jsonDoc["CommentBlocks"].push_back(commentBlockJson);
}

#ifndef WRITE_PAGES
void WriteAddressRangeToJson(FCodeAnalysisState& state, int startAddress,int endAddress, json& jsonDoc)
{
	int address = startAddress;

	// info on last writer
	jsonDoc["LastWriterStart"] = startAddress;
	for (int addr = startAddress; addr <= endAddress; addr++)
		jsonDoc["LastWriter"].push_back(state.GetLastWriterForAddress(addr));
	
	while(address <= endAddress)
	{
		FCommentBlock* pCommentBlock = state.GetCommentBlockForAddress(address);
		if (pCommentBlock != nullptr)
			WriteCommentBlockToJson(pCommentBlock, jsonDoc);

		FLabelInfo* pLabelInfo = state.GetLabelForAddress(address);
		if (pLabelInfo)
			WriteLabelInfoToJson(pLabelInfo, jsonDoc);

		FCodeInfo* pCodeInfoItem = state.GetCodeInfoForAddress(address);
		if (pCodeInfoItem && pCodeInfoItem->Address == address)	// only write code items for first byte of the instruction
		{
			WriteCodeInfoToJson(pCodeInfoItem, jsonDoc);
			
			if (pCodeInfoItem->bSelfModifyingCode == false)	// this is so that we can write info on SMC accesses
				address += pCodeInfoItem->ByteSize;
		}
		
		if(pCodeInfoItem == nullptr || pCodeInfoItem->bSelfModifyingCode)
		{
			FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(address);
			WriteDataInfoToJson(pDataInfo, jsonDoc);
			address += pDataInfo->ByteSize;
		}
	}
}
#endif // #ifndef WRITE_PAGES


FCommentBlock* CreateCommentBlockFromJson(const json& commentBlockJson)
{
	FCommentBlock* pCommentBlock = FCommentBlock::Allocate();
	pCommentBlock->Address = commentBlockJson["Address"];
	pCommentBlock->Comment = commentBlockJson["Comment"];
	return pCommentBlock;
}

FCodeInfo* CreateCodeInfoFromJson(const json& codeInfoJson)
{
	FCodeInfo* pCodeInfo = FCodeInfo::Allocate();
	pCodeInfo->Address = codeInfoJson["Address"];
	pCodeInfo->ByteSize = codeInfoJson["ByteSize"];

	if (codeInfoJson.contains("SMC"))
		pCodeInfo->bSelfModifyingCode = codeInfoJson["SMC"];

	if (codeInfoJson.contains("OperandType"))
		pCodeInfo->OperandType = codeInfoJson["OperandType"];

	if (codeInfoJson.contains("Flags"))
		pCodeInfo->Flags = codeInfoJson["Flags"];

	if (codeInfoJson.contains("Comment"))
		pCodeInfo->Comment = codeInfoJson["Comment"];

	return pCodeInfo;
}

FLabelInfo* CreateLabelInfoFromJson(const json& labelInfoJson)
{
	FLabelInfo* pLabelInfo = FLabelInfo::Allocate();

	pLabelInfo->Address = labelInfoJson["Address"];
	pLabelInfo->Name = labelInfoJson["Name"];
	if (labelInfoJson.contains("Global"))
		pLabelInfo->Global = true;

	if (labelInfoJson.contains("LabelType"))
		pLabelInfo->LabelType = (ELabelType)(int)labelInfoJson["LabelType"];
	if (labelInfoJson.contains("Comment"))
		pLabelInfo->Comment = labelInfoJson["Comment"];

	if (labelInfoJson.contains("References"))
	{
		for (const auto& reference : labelInfoJson["References"])
		{
			pLabelInfo->References[reference] = 1;
		}
	}

	return pLabelInfo;
}

void LoadDataInfoFromJson(FDataInfo* pDataInfo, const json & dataInfoJson)
{
	pDataInfo->Address = dataInfoJson["Address"];

	if (dataInfoJson.contains("DataType"))
		pDataInfo->DataType = (EDataType)(int)dataInfoJson["DataType"];
	if (dataInfoJson.contains("OperandType"))
		pDataInfo->OperandType = (EOperandType)(int)dataInfoJson["OperandType"];
	if (dataInfoJson.contains("ByteSize"))
		pDataInfo->ByteSize = dataInfoJson["ByteSize"];
	if (dataInfoJson.contains("Flags"))
		pDataInfo->Flags = dataInfoJson["Flags"];
	if (dataInfoJson.contains("Comment"))
		pDataInfo->Comment = dataInfoJson["Comment"];
	if (dataInfoJson.contains("Reads"))
	{
		for (const auto& read : dataInfoJson["Reads"])
		{
			pDataInfo->Reads[read] = 1;
		}
	}
	if (dataInfoJson.contains("Writes"))
	{
		for (const auto& read : dataInfoJson["Writes"])
		{
			pDataInfo->Writes[read] = 1;
		}
	}

#if SPECCY
	// Charmap specific
	if (pDataInfo->DataType == EDataType::CharacterMap)
	{
		if(dataInfoJson.contains("CharSetAddress"))
			pDataInfo->CharSetAddress = dataInfoJson["CharSetAddress"];
		if (dataInfoJson.contains("EmptyCharNo"))
			pDataInfo->EmptyCharNo = dataInfoJson["EmptyCharNo"];
	}
#endif
}


bool ImportAnalysisJson(FCodeAnalysisState& state, const char* pJsonFileName)
{
	std::ifstream inFileStream(pJsonFileName);
	if (inFileStream.is_open() == false)
		return false;

	json jsonGameData;

	inFileStream >> jsonGameData;
	inFileStream.close();

	// info on last writer
	if (jsonGameData.contains("LastWriterStart"))
	{
		const int lwStart = jsonGameData["LastWriterStart"];

		const json& lastWriterArray = jsonGameData["LastWriter"];
		const int noWriters = (int)lastWriterArray.size();
		for (int i = 0;i<noWriters;i++)
			state.SetLastWriterForAddress(lwStart + i, lastWriterArray[i]);
	}

	if (jsonGameData.contains("CommentBlocks"))
	{
		for (const auto commentBlockJson : jsonGameData["CommentBlocks"])
		{
			FCommentBlock* pCommentBlock = CreateCommentBlockFromJson(commentBlockJson);
			state.SetCommentBlockForAddress(pCommentBlock->Address, pCommentBlock);
		}
	}

	if (jsonGameData.contains("CodeInfo"))
	{
		for (const auto codeInfoJson : jsonGameData["CodeInfo"])
		{
			FCodeInfo* pCodeInfo = CreateCodeInfoFromJson(codeInfoJson);

			for (int codeByte = 0; codeByte < pCodeInfo->ByteSize; codeByte++)	// set for whole instruction address range
				state.SetCodeInfoForAddress(pCodeInfo->Address + codeByte, pCodeInfo);
		}
	}

	if (jsonGameData.contains("LabelInfo"))
	{
		for (const auto labelInfoJson : jsonGameData["LabelInfo"])
		{
			FLabelInfo* pLabelInfo = CreateLabelInfoFromJson(labelInfoJson);

			state.SetLabelForAddress(pLabelInfo->Address,pLabelInfo);
		}
	}

	if (jsonGameData.contains("DataInfo"))
	{
		for (const auto dataInfoJson : jsonGameData["DataInfo"])
		{
			const int address = dataInfoJson["Address"];
			FDataInfo* pDataInfo = state.GetReadDataInfoForAddress(address);
			LoadDataInfoFromJson(pDataInfo, dataInfoJson);
		}
	}

	if (jsonGameData.contains("Watches"))
	{
		for (const auto& watch : jsonGameData["Watches"])
		{
			state.AddWatch(watch);
		}
	}

#if SPECCY
	if (jsonGameData.contains("CharacterSets"))
	{
		for (const auto& charSet : jsonGameData["CharacterSets"])
		{
			FCharSetCreateParams params;
			params.Address = charSet["Address"];
			params.AttribsAddress = charSet["AttribsAddress"];
			params.MaskInfo = charSet["MaskInfo"];
			params.ColourInfo = charSet["ColourInfo"];
			params.bDynamic = charSet["Dynamic"];
			CreateCharacterSetAt(state, params);
		}
	}

	if (jsonGameData.contains("CharacterMaps"))
	{
		for (const auto& charMap : jsonGameData["CharacterMaps"])
		{
			FCharMapCreateParams params;
			params.Address = charMap["Address"];
			params.Width = charMap["Width"];
			params.Height = charMap["Height"];
			params.CharacterSet = charMap["CharacterSet"];
			params.IgnoreCharacter = charMap["IgnoreCharacter"];
			CreateCharacterMap(state, params);
		}
	}
#endif
	
	return true;
}

// write a 1K page to Json
// the plan is to move to this so we can support 128K games
void WritePageToJson(const FCodeAnalysisPage& page, json& jsonDoc)
{
	jsonDoc["PageId"] = page.PageId;

	int pageAddr = 0;

	while (pageAddr < FCodeAnalysisPage::kPageSize)
	{
		const FCommentBlock* pCommentBlock = page.CommentBlocks[pageAddr];
		if (pCommentBlock != nullptr)
			WriteCommentBlockToJson(pageAddr, pCommentBlock, jsonDoc);

		const FLabelInfo* pLabelInfo = page.Labels[pageAddr];
		if (pLabelInfo)
			WriteLabelInfoToJson(pageAddr, pLabelInfo, jsonDoc);

		const FCodeInfo* pCodeInfoItem = page.CodeInfo[pageAddr];
		if (pCodeInfoItem)
		{
			WriteCodeInfoToJson(pCodeInfoItem, jsonDoc);
			if (pCodeInfoItem->bSelfModifyingCode == false)
			{
				pageAddr += pCodeInfoItem->ByteSize;
			}
		}

		// we do want data info for SMC operands
		if (pCodeInfoItem == nullptr || pCodeInfoItem->bSelfModifyingCode == true)
		{
			const FDataInfo* pDataInfo = &page.DataInfo[pageAddr];
			WriteDataInfoToJson(pDataInfo, jsonDoc);
			pageAddr += pDataInfo->ByteSize;
		}
	}
}


void ReadBankFromJson(FCpcEmu* pCpcEmu, int bankNo, const json& jsonDoc)
{
	const int bankPage = bankNo * 16;

	if (jsonDoc.contains("LastWriter"))
	{
		const json& lastWriterArray = jsonDoc["LastWriter"];
		const int noWriters = (int)lastWriterArray.size();
		for (int i = 0; i < noWriters; i++)
		{
			const uint16_t bankAddr = i;
			const uint16_t pageNo = bankPage + (bankAddr / 1024);
			const uint16_t pageAddr = bankAddr & 1023;

			pCpcEmu->RAMPages[pageNo].LastWriter[pageAddr] = lastWriterArray[i];
		}
	}

	if (jsonDoc.contains("CommentBlocks"))
	{
		for (const auto commentBlockJson : jsonDoc["CommentBlocks"])
		{
			FCommentBlock* pCommentBlock = CreateCommentBlockFromJson(commentBlockJson);

			const uint16_t bankAddr = pCommentBlock->Address;
			const uint16_t pageNo = bankPage + (bankAddr / 1024);
			const uint16_t pageAddr = bankAddr & 1023;

			pCpcEmu->RAMPages[pageNo].CommentBlocks[pageAddr] = pCommentBlock;
		}
	}

	if (jsonDoc.contains("LabelInfo"))
	{
		for (const auto labelInfoJson : jsonDoc["LabelInfo"])
		{
			FLabelInfo* pLabelInfo = CreateLabelInfoFromJson(labelInfoJson);
			const uint16_t bankAddr = pLabelInfo->Address;
			const uint16_t pageNo = bankPage + (bankAddr / 1024);
			const uint16_t pageAddr = bankAddr & 1023;

			pCpcEmu->RAMPages[pageNo].Labels[pageAddr] = pLabelInfo;
		}
	}

	if (jsonDoc.contains("DataInfo"))
	{
		for (const auto dataInfoJson : jsonDoc["DataInfo"])
		{
			const uint16_t bankAddr = dataInfoJson["Address"];
			const uint16_t pageNo = bankPage + (bankAddr / 1024);
			const uint16_t pageAddr = bankAddr & 1023;

			FDataInfo* pDataInfo = &pCpcEmu->RAMPages[pageNo].DataInfo[pageAddr];
			LoadDataInfoFromJson(pDataInfo, dataInfoJson);
		}
	}
}