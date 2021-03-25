// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to https://github.com/citra-emu/citra/blob/master/license.txt.
// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// For the license refer to https://github.com/dolphin-emu/dolphin/blob/master/license.txt
// Code from both projects are modified for RPCS3, with https://github.com/VelocityRa/rpcs3/commit/eb61b9ae58d2c1dc65c1d5b51b49a7d17f0dbfc8 as a guideline.
#pragma once

#include <vector>
#include "Emu/System.h"
#include "3rdparty/ChunkFile.h"
namespace SaveState
{

	struct RPCS3STHeader;

	enum StateMode
	{
		NONE = 0,
		SAVE = 1,
		LOAD = 2,
	};
	struct SaveStateInfo
	{
		u32 slot;
		u64 time;
		enum class ValidationStatus
		{
			OK,
			RevisionDismatch,
		} status;
	};
	void Init();
	std::string MakeStateFileName();
	void Shutdown();
	bool ReadHeader(const std::string& filename, RPCS3STHeader& header);
	void SaveToBuffer(std::vector<u8>& buffer);
	void LoadFromBuffer(std::vector<u8>& buffer);
	void VerifyBuffer(std::vector<u8>& buffer);
	void Flush();
	using AfterLoadCallbackFunc = std::function<void()>;
	void SetOnAfterLoadCallback(AfterLoadCallbackFunc callback);

	constexpr u32 SaveStateSlotCount = 10; // Maximum count of savestate slots

	std::string DoState(PointerWrap& p);
	void SaveSavestate(std::string path);
	void LoadSavestate(std::string path);
}