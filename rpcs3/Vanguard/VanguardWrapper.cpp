#include "stdafx.h"
#include <rpcs3/Emu/Memory/vm.h>
#include <rpcs3/Emu/System.h>
#include <string>
#include <Utilities/File.h>
extern "C" __declspec(dllexport) unsigned char ManagedWrapper_peekbyte(long long addr)
{
	if (vm::check_addr(static_cast<u32>(addr)) == false)
		return 0x3F; // unmapped memory; denote this by ?s
	return vm::g_sudo_addr[static_cast<u32>(addr)];
}

extern "C" __declspec(dllexport) void ManagedWrapper_pokebyte(long long addr, unsigned char val)
{
	if (vm::check_addr(static_cast<u32>(addr)) == false)
		return;
	vm::g_sudo_addr[static_cast<u32>(addr)] = val;
}

extern "C" __declspec(dllexport) void ManagedWrapper_savesavestate(const char* filename)
{
	const std::string path = fs::get_cache_dir() + "/savestates/" + (Emu.GetTitleID().empty() ? Emu.GetBoot().substr(Emu.GetBoot().find_last_of(fs::delim) + 1) : Emu.GetTitleID()) + ".SAVESTAT";
	if (fs::exists(path))
	{
		fs::remove_file(path);
	}
	Emu.GracefulShutdown(true, false, true);
	if (!fs::exists(path))
	{
		return;
	}
	fs::copy_file(path, std::string(filename), true);
}

extern "C" __declspec(dllexport) void ManagedWrapper_loadsavestate(const char* filename)
{
	Emu.GracefulShutdown();
	Emu.BootGame(std::string(filename));
}

extern "C" __declspec(dllexport) void ManagedWrapper_pause()
{
	Emu.Pause();
}

extern "C" __declspec(dllexport) void ManagedWrapper_resume()
{
	Emu.Resume();
}