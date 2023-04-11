#include "SNALoader.h"

#include "../CpcEmu.h"

#include <Util/FileUtil.h>
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

bool LoadSNAFileCached(FCpcEmu* pEmu, const char* fName, uint8_t*& pData , size_t& dataSize)
{
	if (pData == nullptr)
	{
		pData = (uint8_t*)LoadBinaryFile(fName, dataSize);
		if (!pData)
			return false;
	}
	const bool bSuccess = LoadSNAFromMemory(pEmu, pData, dataSize);

	return bSuccess;
}

bool LoadSNAFromMemory(FCpcEmu * pEmu, const uint8_t * pData, size_t dataSize)
{
	return cpc_quickload(&pEmu->CpcEmuState, pData, static_cast<int>(dataSize));
}

