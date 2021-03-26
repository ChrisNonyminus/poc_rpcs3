// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to https://github.com/citra-emu/citra/blob/master/license.txt.
// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// For the license refer to https://github.com/dolphin-emu/dolphin/blob/master/license.txt
// Copyright (C) 2002-2021  The DOSBox Team
// Licensed under GPLv2
// Refer to https://github.com/joncampbell123/dosbox-x/blob/master/COPYING
// Code from all projects are modified for RPCS3, with https://github.com/VelocityRa/rpcs3/commit/eb61b9ae58d2c1dc65c1d5b51b49a7d17f0dbfc8 as a guideline.
#include <chrono>
#include "Utilities/File.h"
#include "Crypto/utils.h"
#include "SaveState.h"
#include "Emu/Cell/lv2/sys_rsx.h"
#include "Emu/System.h"
#include "Crypto/lz.h"
#include "Emu/Memory/vm.h"
#include "Emu/CPU/CPUThread.h"
#include "Emu/Cell/SPUThread.h"
#include "3rdparty/7z/7z.h"
#include "Utilities/Thread.h"
#include "Emu/RSX/RSXThread.h"
#include "3rdparty/ChunkFile.h"
#include "stdafx.h"
#include "Emu/Memory/vm.h"
#include "Emu/CPU/CPUThread.h"
#include "Emu/Cell/SPUThread.h"
#include "3rdparty/ChunkFile.h"
#include "Utilities/Thread.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"
#include "util/sysinfo.hpp"
#include "Utilities/bin_patch.h"
#include "Emu/Memory/vm.h"
#include "Emu/System.h"
#include "Emu/perf_meter.hpp"

#include "Emu/Cell/ErrorCodes.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/PPUCallback.h"
#include "Emu/Cell/PPUOpcodes.h"
#include "Emu/Cell/PPUDisAsm.h"
#include "Emu/Cell/PPUAnalyser.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/Cell/RawSPUThread.h"
#include "Emu/RSX/RSXThread.h"
#include "Emu/Cell/lv2/sys_process.h"
#include "Emu/Cell/lv2/sys_memory.h"
#include "Emu/Cell/lv2/sys_sync.h"
#include "Emu/Cell/lv2/sys_prx.h"
#include "Emu/Cell/lv2/sys_overlay.h"
#include "Emu/Cell/lv2/sys_rsx.h"
#include "Emu/Cell/Modules/cellMsgDialog.h"

#include "Emu/title.h"
#include "Emu/IdManager.h"
#include "Emu/RSX/Capture/rsx_replay.h"

#include "Loader/PSF.h"
#include "Loader/ELF.h"

#include "Utilities/StrUtil.h"

#include "util/sysinfo.hpp"
#include "util/yaml.hpp"
#include "util/logs.hpp"
#include "util/cereal.hpp"

#include <thread>
#include <queue>
#include <fstream>
#include <memory>
#include <regex>
#include <charconv>

#include "Utilities/JIT.h"

#include "display_sleep_control.h"
#include <zlib.h>
#include <Emu/Audio/AudioBackend.h>
//#include <Emu/Audio/XAudio2/XAudio2Backend.h>
//#include <3rdparty/XAudio2Redist/include/xaudio2redist.h>


namespace SaveState
{
	LOG_CHANNEL(sys_log, "SYS");
	static std::vector<u8> g_current_buffer;
	static std::mutex g_cs_current_buffer;
#pragma pack(push, 1)
	struct RPCS3STHeader
	{
		std::array<u8, 8> filetype; /// Unique Identifier to check the file type (always "RPCS3ST"0xFF)
		std::array<u8, 8> dummybytes1 =
		    {
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		};                  // fill the next 8 bytes with zeroes so the serial doesn't wrap around
		char serial[9]; /// The game's serial in ascii
		std::array<u8, 7> dummybytes2 =
		    {
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0
		}; // fill the next 7 bytes with zeroes
		u64 size; //uncompressed size
		std::array<u8, 8> dummybytes3 =
		    {
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		        0x0,
		};
	};
#pragma pack(pop)

	constexpr std::array<u8, 8> header_magic_bytes{{'R', 'P', 'C', 'S', '3', 'S', 'T', 0x7F}};

	std::string DoState(PointerWrap& p){
		//Note to self: The order of state saving should be: IDM (somewhat done)->  RSX (somewhat done) -> IO? (none) -> fs (none) -> Audio (not working) -> VM (done) -> CPU (somewhat done) -> PPU (somewhat done) -> SPU (somewhat done)
		//done = serialized
		//none = not serialized (yet)
		//somewhat done = not everything in the namespace is serialized
		//not working = attempt at serializing was made but it's broken
		
		//rsx::DoState(p);
		//p.DoMarker("rsx", 0x0);
		/*p.Do(g_fxo);
		p.DoMarker("stx::g_fxo", 0x50);
		p.Do(g_fixed_typemap);
		p.DoMarker("stx::g_fixed_typemap", 0x7);*/ //rpcs3 won't let me deserialize fxo
		//rsx::thread::DoState(p);
		g_fxo->get<idm>().DoState(p);
		p.DoMarker("idm", 0x1);
		//g_fxo->get<rsx::thread>().DoState(p);
		//g_fxo->get<AudioBackend>().DoState(p);
		/*g_fxo->get<XAudio2Backend>().DoState(p);
		p.DoMarker("XAudio2Backend", 0x6);*/
		
		idm::select<rsx::thread>([&p](u32, rsx::thread& rsxthr) { rsxthr.DoState(p); });
		p.DoMarker("rsx::thread", 0x0);
		idm::select<AudioBackend>([&p](u32, AudioBackend& audiob) { audiob.DoState(p); });
		p.DoMarker("AudioBackend", 0x5);
		vm::DoState(p);
		p.DoMarker("vm", 0x2);
		
		//get_current_cpu_thread()->DoState(p);
		//p.DoMarker("cpu_thread", 0x3);
		/*idm::select<cpu_thread>([&p](u32, cpu_thread& cpu) { cpu.DoState(p); });
		p.DoMarker("cpu_thread", 0x8);*/
		idm::select<ppu_thread>([&p](u32, ppu_thread& ppu) { ppu.DoState(p); });
		p.DoMarker("ppu_thread", 0x4);
		idm::select<spu_thread>([&p](u32, spu_thread& spu) { spu.DoState(p); });
		p.DoMarker("spu_thread", 0x3);
		idm::select<lv2_obj>([&p](u32, lv2_obj& obj) { obj.DoState(p); });
		p.DoMarker("lv2_obj", 0x6);
		return "";
	}
	void LoadFromBuffer(std::vector<u8>& buffer)
	{
		//Core::RunAsCPUThread([&] {
		u8* ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_READ);
		DoState(p);
		//});
	}

	void SaveToBuffer(std::vector<u8>& buffer)
	{
		//Core::RunAsCPUThread([&] {
		u8* ptr = nullptr;
		PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

		DoState(p);
		const size_t buffer_size = reinterpret_cast<size_t>(ptr);
		buffer.resize(buffer_size);

		ptr = &buffer[0];
		p.SetMode(PointerWrap::MODE_WRITE);
		DoState(p);
		//});
	}

	void VerifyBuffer(std::vector<u8>& buffer)
	{
		//Core::RunAsCPUThread([&] {
		u8* ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
		DoState(p);
		//});
	}
	struct CompressAndDumpState_args
	{
		std::vector<u8>* buffer_vector;
		std::mutex* buffer_mutex;
		std::string filename;
	};

	static void CompressAndDumpState(CompressAndDumpState_args save_args)
	{
		const u8* const buffer_data = &(*(save_args.buffer_vector))[0];
		const size_t buffer_size    = (save_args.buffer_vector)->size();
		std::string& filename       = save_args.filename;
		bool fileexists             = false;
		// For easy debugging
		// Common::SetCurrentThreadName("SaveState thread");

		// Moving to last overwritten save-state
		if (fs::is_file(filename))
		{
			/*if (fs::is_file("lastState.sav"))
				fs::remove_file(("lastState.sav"));

			if (!fs::rename(filename, "lastState.sav", true))
				sys_log.error("Failed to move previous state to state undo backup");*/
			fileexists = true;
		}
		else
		{
		}

		fs::file f(filename, fileexists ? fs::rewrite : fs::create);
		if (!f)
		{
			sys_log.error("Could not save state");
			return;
		}

		// Setting up the header
		RPCS3STHeader header{};
		header.filetype = header_magic_bytes;
		//header.serial   = Emu.GetTitleID().c_str();
		const auto title_id = Emu.GetTitleID();
		title_id.copy(header.serial, 9);
		header.size     = buffer_size;
		f.write(&header, sizeof(header));

		f.write(buffer_data, buffer_size);
		/*const uLong compressedsize = ::compressBound((uLong)buffer_size);
		std::string compresseddata;
		compresseddata.resize(compressedsize);
		uLongf actualSize = compressedsize;
		if (::compress2(reinterpret_cast<Bytef*>(&compresseddata[0]), &actualSize, reinterpret_cast<const Bytef*>(buffer_data), buffer_size, Z_DEFAULT_COMPRESSION) != Z_OK)
		{
			sys_log.error("Failed to compress savestate");
			return;
		}
		compresseddata.resize(actualSize);
		f.write(compresseddata, actualSize);*/
		sys_log.success("Saved State to %s", filename.c_str());
	}
	void Init()
	{
	}

	std::string MakeStateFileName()
	{
		return fmt::format("%s.ps3st", Emu.GetTitleID().c_str() /*, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()*/);
	}

	void Shutdown()
	{
		//SaveState::Flush();
	}

	void Flush()
	{
	}
	bool ReadHeader(const std::string& filename, RPCS3STHeader& header)
	{
		Flush();
		fs::file f(filename, fs::read);
		if (!f)
		{
			sys_log.error("State not found");
			return false;
		}

		f.read(&header, sizeof(header));
		return true;
	}
	void SaveSavestate(std::string path)
	{
		Emu.Pause();
		// Serialize

		//const std::string& str{sstream.str()};
		//std::string buffer = ;

		//Core::RunAsCPUThread([&] {
		// Measure the size of the buffer.
		u8* ptr = nullptr;
		PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
		DoState(p);
		const size_t buffer_size = reinterpret_cast<size_t>(ptr);

		// Then actually do the write.
		{
			std::lock_guard<std::mutex> lk(g_cs_current_buffer);
			g_current_buffer.resize(buffer_size);
			ptr = &g_current_buffer[0];
			p.SetMode(PointerWrap::MODE_WRITE);
			DoState(p);
		}

		if (p.GetMode() == PointerWrap::MODE_WRITE)
		{
			sys_log.notice("Saving State...");

			CompressAndDumpState_args save_args;
			save_args.buffer_vector = &g_current_buffer;
			save_args.buffer_mutex  = &g_cs_current_buffer;
			save_args.filename      = path;

			Flush();
			CompressAndDumpState(save_args);

			//g_last_filename = filename;
		}
		else
		{
			// someone aborted the save by changing the mode?
			sys_log.error("Unable to save: Internal DoState Error");
		}
		//});

		Emu.Resume();
	}

	static void LoadFileStateData(const std::string& filename, std::vector<u8>& ret_data)
	{
		Flush();
		fs::file f(filename, fs::read);
		if (!f)
		{
			sys_log.error("State not found");
			return;
		}

		RPCS3STHeader header;
		f.read(&header, sizeof(header));

		if (strncmp(Emu.GetTitleID().c_str(), header.serial, 9))
		{
			sys_log.error("State belongs to a different game (ID %.*s)", 9, header.serial);
			return;
		}
		u64 uncompressedSize = header.size;
		std::vector<u8> buffer;

		{
			const size_t size = (size_t)(f.size() - sizeof(RPCS3STHeader));
			buffer.resize(size);

			if (!f.read(&buffer[0], size))
			{
				sys_log.error("wtf? reading bytes: %zu", size);
				return;
			}
			/*std::string output;
			output.resize(uncompressedSize);

			uLongf actualsize = (uLongf)uncompressedSize;
			if (::uncompress(reinterpret_cast<Bytef*>(&output[0]), &actualsize, reinterpret_cast<const Bytef*>(buffer), (uLong)(size)) != Z_OK)
			{
				sys_log.error("Failed to uncompress savestate");
				return;
			}
			::memcpy(&buffer, &)*/
		}

		// all good
		ret_data.swap(buffer);
	}
	void LoadSavestate(std::string path)
	{
		Emu.Pause();
		//if (!Core::IsRunning())
		//{
		//	return;
		//}

		//Core::RunAsCPUThread([&] {
		bool loaded             = false;
		bool loadedSuccessfully = false;
		std::string version_created_by;

		// brackets here are so buffer gets freed ASAP
		{
			std::vector<u8> buffer;
			LoadFileStateData(path, buffer);

			if (!buffer.empty())
			{
				u8* ptr = &buffer[0];
				PointerWrap p(&ptr, PointerWrap::MODE_READ);
				version_created_by = DoState(p);
				loaded             = true;
				loadedSuccessfully = (p.GetMode() == PointerWrap::MODE_READ);
			}
		}

		if (loaded)
		{
			if (loadedSuccessfully)
			{
				sys_log.notice("Loaded state from %s", path.c_str());
			}
			else
			{
				sys_log.error("Unable to load");
			}
		}

		Emu.Resume();
		//});
	}

} // namespace SaveState