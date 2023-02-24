#include "SNALoader.h"

#include "../CpcEmu.h"

#include <cstdint>
#include <Util/FileUtil.h>
#include <cassert>
#include <systems/cpc.h>

bool LoadSNAFile(FCpcEmu* pEmu, const char* fName)
{
	size_t byteCount = 0;
	uint8_t* pData = (uint8_t*)LoadBinaryFile(fName, byteCount);
	if (!pData)
		return false;
	const bool bSuccess = LoadSNAFromMemory(pEmu, pData, byteCount);
	free(pData);

	return bSuccess;
}

bool LoadSNAFromMemory(FCpcEmu * pEmu, const uint8_t * pData, size_t dataSize)
{
	cpc_quickload(&pEmu->CpcEmuState, pData, static_cast<int>(dataSize));
	return true;	// NOT implemented
}

