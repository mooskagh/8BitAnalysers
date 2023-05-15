#pragma once

#include <string>
#include <CodeAnalyser/CodeAnaysisPage.h> 

class FCpcEmu;

enum class CpcIODevice
{
	None = -1,
	Keyboard,
	BorderColour,

#if SPECCY
	Ear,	// for loading
	Mic,	// for saving
	Beeper,
	KempstonJoystick,
	MemoryBank,		// Switching in/out RAM banks
	SoundChip,		// AY
#endif

	Unknown,
	Count
};

struct FIOAccess
{
	uint16_t		Address = 0;
	int				ReadCount = 0;
	int				FrameReadCount = 0;
	int				WriteCount = 0;
	int				FrameWriteCount = 0;

	FItemReferenceTracker	Callers;
};

class FIOAnalysis
{
public:
	void	Init(FCpcEmu* pEmu);
	void	IOHandler(uint16_t pc, uint64_t pins);
	void	DrawUI();

private:
	FCpcEmu*			pCpcEmu = nullptr;
	FIOAccess			IODeviceAcceses[(int)CpcIODevice::Count];
	uint8_t				LastFE = 0;
	CpcIODevice			SelectedDevice = CpcIODevice::None;
};
