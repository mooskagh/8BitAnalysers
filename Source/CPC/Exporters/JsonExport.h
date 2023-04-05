#pragma once

struct FCodeAnalysisState;
class FCpcEmu;

bool ExportROMJson(FCodeAnalysisState& state, const char* pJsonFileName);
bool ExportGameJson(FCpcEmu* pCpcEmu, const char* pJsonFileName);
bool ImportAnalysisJson(FCodeAnalysisState& state, const char* pJsonFileName);