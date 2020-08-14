#include "functions.h"
#include "hairpincave.h"
#include "clienthacks.h"
#include "globals.h"

#include <thread>

extern xiloader::globals* globalVars;
extern DWORD g_NewServerAddress;
extern DWORD g_HairpinReturnAddress;

static bool waitForGame(xiloader::SharedState& sharedState)
{
    do
    {
        /* Sleep until we find FFXiMain loaded.. */
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (GetModuleHandleA("FFXiMain.dll") == NULL && sharedState.isRunning);

    if (!sharedState.isRunning)
    {
        xiloader::console::output(xiloader::color::error,
            "Lost shared state before FFXiMain loaded into process!");
        return false;
    }
    return true;
}

/**
 * @brief Applies the hairpin fix modifications.
 *
 * @return void
 */
void ApplyHairpinFix(xiloader::SharedState& sharedState)
{
    if (!waitForGame(sharedState))
        return;

    std::string host;
    DWORD newhost;
    globalVars->getServerAddress(&host);

    /* Convert server address.. */
    xiloader::network::ResolveHostname(host.c_str(), &newhost);
    g_NewServerAddress = newhost;

    // Locate the main hairpin location..
    //
    // As of 07.08.2013:
    //      8B 82 902E0100        - mov eax, [edx+00012E90]
    //      89 02                 - mov [edx], eax <-- edit this

    auto hairpinAddress = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll",
        (BYTE*)"\x8B\x82\xFF\xFF\xFF\xFF\x89\x02\x8B\x0D", "xx????xxxx");
    if (hairpinAddress == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate main hairpin hack address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    // Locate zoning IP change address..
    //
    // As of 07.08.2013
    //      74 08                 - je FFXiMain.dll+E5E72
    //      8B 0D 68322B03        - mov ecx, [FFXiMain.dll+463268]
    //      89 01                 - mov [ecx], eax <-- edit this
    //      8B 46 0C              - mov eax, [esi+0C]
    //      85 C0                 - test eax, eax

    auto zoneChangeAddress = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll",
        (BYTE*)"\x8B\x0D\xFF\xFF\xFF\xFF\x89\x01\x8B\x46", "xx????xxxx");
    if (zoneChangeAddress == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate zone change hairpin address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    /* Apply the hairpin fix.. */
    auto caveDest = ((int)HairpinFixCave - ((int)hairpinAddress)) - 5;
    g_HairpinReturnAddress = hairpinAddress + 0x08;

    *(BYTE*)(hairpinAddress + 0x00) = 0xE9; // jmp
    *(UINT*)(hairpinAddress + 0x01) = caveDest;
    *(BYTE*)(hairpinAddress + 0x05) = 0x90; // nop
    *(BYTE*)(hairpinAddress + 0x06) = 0x90; // nop
    *(BYTE*)(hairpinAddress + 0x07) = 0x90; // nop

    /* Apply zone ip change patch.. */
    memset((LPVOID)(zoneChangeAddress + 0x06), 0x90, 2);

    xiloader::console::output(xiloader::color::success, "Hairpin fix applied!");
}


/**
 * @brief Modifies FPS lock. Created by Cloudef
 *
 * @return void
 */
void ApplyFPSHack(xiloader::SharedState& sharedState)
{
    if (!waitForGame(sharedState))
        return;

    static float frameskip = 60.0f;

    auto raddr = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll", (BYTE*)"\x89\x46\x28\xD9\x46\x28\xD8\x1D", "xxxxxxxx");
    if (raddr == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate frame skip address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    *(unsigned long*)(raddr + 0x8) = (unsigned long)&frameskip;

    auto raddr2 = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll", (BYTE*)"\xC7\x46\x28\x00\x00\xA0\x41\xD8\x15", "xxxxxxxxx");
    if (raddr2 == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate frame skip address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    *(unsigned long*)(raddr2 + 0x9) = (unsigned long)&frameskip;

    struct {
        const char *pattern;
        const char *mask;
        DWORD offset;
    } regions[4] = {
        { "\x6A\x04\xE8\xFF\xFF\xFF\xFF\xA0", "xxx????x", 0x1 }, // 15FPS (04) [Fishing]
        { "\x6A\x02\x66\xC7\x41\x3A", "xxxxxx", 0x1 }, // 30FPS (02) [???]
        { "\x6A\x02\x81\xE2", "xxxx", 0x1 }, // 30FPS (02) [Zoning]
        // { "\xC2\x04\x00\x90\x90\x90\x90\x6A\x02\xE8", "xxxxxxxx?x", 0x8 }, // 30FPS (02) [Startup]
        { "\x75\x0A\x6A\x02\xE8", "xxxx", 0x3 }, // 30FPS (02) [Title/Logout]
    };

    DWORD addr[4];
    for (int i = 0; i < 4; ++i) {
        addr[i] = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll",
            (BYTE*)regions[i].pattern, regions[i].mask);
        if (addr[i] == 0)
        {
            xiloader::console::output(xiloader::color::error,
                "Failed to locate FPS cap address! (%d)", i);
            xiloader::NotifyShutdown(sharedState);
            return;
        }
    }

    for (int i = 0; i < 4; ++i)
        *(BYTE*)(addr[i] + regions[i].offset) = 0x01;

    xiloader::console::output(xiloader::color::success, "FPS hack applied!");
}

/**
 * @brief Modifies draw distance larger. Created by Cloudef
 *
 * @return void
 */
void ApplyDrawDistanceHack(xiloader::SharedState& sharedState)
{
    if (!waitForGame(sharedState))
        return;

    auto addr = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll", (BYTE*)"\x8B\xC1\x48\x74\x08\xD8\x0D", "xxxxxxx");
    if (addr == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate draw distance address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    float dist;
    globalVars->getDrawDistance(&dist);
    addr += 0x07;
    *(reinterpret_cast<float**>(addr)) = globalVars->getDrawDistanceUnsafe();

    xiloader::console::output(xiloader::color::success,
        "Draw distance hack applied! (%.2f)", dist);
}

/**
 * @brief Modifies mob draw distance. Created by Cloudef
 *
 * @return void
 */
void ApplyMobDistanceHack(xiloader::SharedState& sharedState)
{
    if (!waitForGame(sharedState))
        return;

    auto addr = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll",
        (BYTE*)"\x8B\xC1\x48\x74\x08\xD8\x0D", "xxxxxxx");
    if (addr == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate mob distance address!");
        xiloader::NotifyShutdown(sharedState);
        return;
    }

    float dist;
    globalVars->getMobDistance(&dist);
    addr += 0x0F;
    *(reinterpret_cast<float**>(addr)) = globalVars->getMobDistanceUnsafe();

    xiloader::console::output(xiloader::color::success,
        "Mob distance hack applied! (%.2f)", dist);
}

/**
 * @brief Disables swear filter. Created by Cloudef
 *
 * @return void
 */
void ApplySwearFilterHack(xiloader::SharedState& sharedState)
{
    if (!waitForGame(sharedState))
        return;

    auto addr = (DWORD)xiloader::functions::FindPattern("FFXiMain.dll",
        (BYTE*)"\x83\xF8\xFF\x89\x46\x04", "xx?xxx");
    if (addr == 0)
    {
        xiloader::console::output(xiloader::color::error,
            "Failed to locate swear filter address!");
        xiloader::NotifyShutdown(sharedState);
    }

    *(BYTE*)(addr + 0x02) = 0x0;
    xiloader::console::output(xiloader::color::success,
        "Swear filter hack applied!");
}
