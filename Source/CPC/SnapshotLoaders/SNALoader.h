#pragma once

#include <cstddef>
#include <cinttypes>

class FCpcEmu;

bool LoadSNAFile(FCpcEmu* pEmu, const char* fName);
bool LoadSNAFromMemory(FCpcEmu* pEmu, const uint8_t* pData, size_t dataSize);
