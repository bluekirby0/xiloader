/*
===========================================================================

Copyright (c) 2010-2014 Darkstar Dev Teams

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/

===========================================================================
*/

#include "defines.h"

#include "console.h"
#include "functions.h"
#include "network.h"
#include "inet_ntop.h"
#include "inet_pton.h"
#include "clienthacks.h"
#include "globals.h"

#include <thread>
#include <string>

xiloader::globals* globalVars;

// Kludge until I can think of a better fix for the hairpin code cave (cannot have parameters)
DWORD g_NewServerAddress;		// Hairpin server address to be overriden with.
DWORD g_HairpinReturnAddress;	// Hairpin return address to allow the code cave to return properly.

/**
 * @brief Detour function definitions.
 */
extern "C"
{
    // FFXI is still using gethostbyname and we cannot easily change that behavior
    // so we will ignore the deprecated method
    #pragma warning(suppress: 4996)
    typedef hostent* (WINAPI __stdcall * GETHOSTBYNAME)(const char* name);
    GETHOSTBYNAME fp_gethostbyname;
}

/**
 * @brief gethostbyname detour callback.
 *
 * @param name      The hostname to obtain information of.
 *
 * @return Hostname information object.
 */
hostent* __stdcall Mine_gethostbyname(const char* name)
{
    xiloader::console::output(xiloader::color::debug, "Resolving host: %s", name);

    std::string host;
    globalVars->getServerAddress(&host);
    if (!strcmp("ffxi00.pol.com", name))
        return fp_gethostbyname(host.c_str());
    if (!strcmp("pp000.pol.com", name))
        return fp_gethostbyname("127.0.0.1");

    return fp_gethostbyname(name);
}

/**
 * @brief Locates the INET mutex function call inside of polcore.dll
 *
 * @param language  POL language.
 *
 * @return The pointer to the function call.
 */
inline DWORD FindINETMutex(const xiloader::Language& language)
{
    const char* module = (language == xiloader::Language::European) ? "polcoreeu.dll" : "polcore.dll";
    auto result = (DWORD)xiloader::functions::FindPattern(module, (BYTE*)"\x8B\x56\x2C\x8B\x46\x28\x8B\x4E\x24\x52\x50\x51", "xxxxxxxxxxxx");
    return (*(DWORD*)(result - 4) + (result));
}

/**
 * @brief Locates the PlayOnline connection object inside of polcore.dll
 *
 * @param language  POL language.
 *
 * @return Pointer to the pol connection object.
 */
inline DWORD FindPolConn(const xiloader::Language& language)
{
    const char* module = (language == xiloader::Language::European) ? "polcoreeu.dll" : "polcore.dll";
    auto result = (DWORD)xiloader::functions::FindPattern(module, (BYTE*)"\x81\xC6\x38\x03\x00\x00\x83\xC4\x04\x81\xFE", "xxxxxxxxxxx");
    return (*(DWORD*)(result - 10));
}

/**
 * @brief Locates the current character information block.
 *
 * @return Pointer to the character information table.
 */
inline LPVOID FindCharacters(void** commFuncs)
{
    LPVOID lpCharTable = NULL;
    memcpy(&lpCharTable, (char*)commFuncs[0xD3] + 31, sizeof(lpCharTable));
    return lpCharTable;
}

/**
 * @brief Launches POL Core / FFXI and setup Hairpin/Detours.
 *
 * @param useHairpinFix Apply Hairpin fix modification.
 * @param language      POL language.
 * @param characterList Pointer to character list in memory.
 * @param sharedState   Shared thread state (bool, mutex, condition_variable).
 *
 * @return void.
 */
void LaunchFFXI(bool* hacksEnabled, const xiloader::Language& language, char*& characterList, xiloader::SharedState& sharedState)
{
    bool errorState = false;

    /* Initialize COM */
    auto hResult = CoInitialize(NULL);
    if (hResult != S_OK && hResult != S_FALSE)
    {
        xiloader::console::output(xiloader::color::error, "Failed to initialize COM, error code: %d", hResult);
        errorState = true;
    }

    if (!errorState)
    {
        /* Attach detour for gethostbyname.. */
        if(MH_CreateHook(&(LPVOID&)gethostbyname, &(LPVOID&)Mine_gethostbyname,
            reinterpret_cast<LPVOID*>(&fp_gethostbyname)) != MH_OK)
        {
            xiloader::console::output(xiloader::color::error, "Failed to hook function 'gethostbyname'. Cannot continue!");
            errorState = true;
        }

        if(MH_EnableHook(&(LPVOID&)gethostbyname) != MH_OK)
        {
            xiloader::console::output(xiloader::color::error, "Failed to enable hook 'gethostbyname'. Cannot continue!");
            errorState = true;
        }

    }

    /* Start hairpin hack thread if required.. */
    std::thread thread_hairpinfix;
    if (!errorState && hacksEnabled[0])
        thread_hairpinfix = std::thread(ApplyHairpinFix, std::ref(sharedState));

    /* Start draw distance hack thread if required.. */
    std::thread thread_drawdist;
    if (!errorState && hacksEnabled[1])
        thread_drawdist = std::thread(ApplyDrawDistanceHack, std::ref(sharedState));

    /* Start mob distance hack thread if required.. */
    std::thread thread_mobdist;
    if (!errorState && hacksEnabled[2])
        thread_mobdist = std::thread(ApplyMobDistanceHack, std::ref(sharedState));

    /* Start framerate hack thread if required.. */
    std::thread thread_fps;
    if (!errorState && hacksEnabled[3])
        thread_fps = std::thread(ApplyFPSHack, std::ref(sharedState));

    /* Start language filter hack thread if required.. */
    std::thread thread_swear;
    if (!errorState && hacksEnabled[4])
        thread_swear = std::thread(ApplySwearFilterHack, std::ref(sharedState));

    if (!errorState)
    {
        /* Attempt to create polcore instance..*/
        IPOLCoreCom* polcore = NULL;
        if (CoCreateInstance(xiloader::CLSID_POLCoreCom[language], NULL, 0x17, xiloader::IID_IPOLCoreCom[language], (LPVOID*)&polcore) != S_OK)
        {
            xiloader::console::output(xiloader::color::error, "Failed to initialize instance of polcore!");
        }
        else
        {
            /* Invoke the setup functions for polcore.. */
            polcore->SetAreaCode(language);
            polcore->SetParamInit(GetModuleHandle(NULL), const_cast<char*>(" /game eAZcFcB -net 3"));

            /* Obtain the common function table.. */
            void* (**lpCommandTable)(...);
            polcore->GetCommonFunctionTable((unsigned long**)&lpCommandTable);

            /* Invoke the inet mutex function.. */
            auto findMutex = (void* (*)(...))FindINETMutex(language);
            findMutex();

            /* Locate and prepare the pol connection.. */
            auto polConnection = (char*)FindPolConn(language);
            memset(polConnection, 0x00, 0x68);
            auto enc = (char*)malloc(0x1000);
            memset(enc, 0x00, 0x1000);
            memcpy(polConnection + 0x48, &enc, sizeof(char**));

            /* Locate the character storage buffer.. */
            characterList = (char*)FindCharacters((void**)lpCommandTable);

            /* Invoke the setup functions for polcore.. */
            lpCommandTable[POLFUNC_REGISTRY_LANG](language);
            lpCommandTable[POLFUNC_FFXI_LANG](xiloader::functions::GetRegistryPlayOnlineLanguage(language));
            lpCommandTable[POLFUNC_REGISTRY_KEY](xiloader::functions::GetRegistryPlayOnlineKey(language));
            lpCommandTable[POLFUNC_INSTALL_FOLDER](xiloader::functions::GetRegistryPlayOnlineInstallFolder(language));
            lpCommandTable[POLFUNC_INET_MUTEX]();

            /* Attempt to create FFXi instance..*/
            IFFXiEntry* ffxi = NULL;
            if (CoCreateInstance(xiloader::CLSID_FFXiEntry, NULL, 0x17, xiloader::IID_IFFXiEntry, (LPVOID*)&ffxi) != S_OK)
            {
                xiloader::console::output(xiloader::color::error, "Failed to initialize instance of FFxi!");
            }
            else
            {
                /* Attempt to start Final Fantasy.. */
                IUnknown* message = NULL;
                xiloader::console::hide();
                ffxi->GameStart(polcore, &message);
                xiloader::console::show();
                ffxi->Release();
            }

            /* Cleanup polcore object.. */
            if (polcore != NULL)
            {
                polcore->Release();
            }
        }
    }

    if (thread_hairpinfix.joinable())
        thread_hairpinfix.join();

    if (thread_drawdist.joinable())
        thread_drawdist.join();

    if (thread_mobdist.joinable())
        thread_mobdist.join();

    if (thread_fps.joinable())
        thread_fps.join();

    if (thread_swear.joinable())
        thread_swear.join();


    xiloader::NotifyShutdown(sharedState);

    /* Detach detour for gethostbyname. */
    if(MH_DisableHook(&(LPVOID&)gethostbyname) != MH_OK)
    {
        xiloader::console::output(xiloader::color::error, "Failed unhooking function 'gethostbyname'.");
    }

    /* Cleanup COM */
    CoUninitialize();
}

/**
 * @brief Main program entrypoint.
 *
 * @param argc      The count of arguments being passed to this application on launch.
 * @param argv      Pointer to array of argument data.
 *
 * @return 1 on error, 0 on success.
 */
int __cdecl main(int argc, char* argv[])
{
    if (MH_Initialize() != MH_OK)
    {
        xiloader::console::output(xiloader::color::error, "Failed to initialize hooking library.");
        return 1;
    }

    globalVars = new xiloader::globals();

    bool bUseHairpinFix = false;
    bool bUseDrawDistanceHack = false;
    bool bUseMobDistanceHack = false;
    bool bUseFPSHack = false;
    bool bUseSwearHack = false;

    xiloader::Language language = xiloader::Language::English; // The language of the loader to be used for polcore.
    std::string lobbyServerPort = "51220"; // The server lobby server port.
    std::string username = ""; // The username being logged in with.
    std::string password = ""; // The password being logged in with.
    char* characterList = NULL; // Pointer to the character list data being sent from the server.

    xiloader::SharedState sharedState; // shared thread state

    /* Output the banner.. */
    xiloader::console::output(xiloader::color::lightred, "==========================================================");
    xiloader::console::output(xiloader::color::lightgreen, "xiloader (c) 2015 DarkStar Team");
    xiloader::console::output(xiloader::color::lightgreen, "This program comes with ABSOLUTELY NO WARRANTY.");
    xiloader::console::output(xiloader::color::lightgreen, "This is free software; see LICENSE file for details.");
    xiloader::console::output(xiloader::color::lightpurple, "Git Repo   : https://github.com/zircon-tpl/xiloader");
    xiloader::console::output(xiloader::color::lightred, "==========================================================");

    /* Initialize Winsock */
    WSADATA wsaData = { 0 };
    auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        xiloader::console::output(xiloader::color::error, "Failed to initialize winsock, error code: %d", ret);
        MH_Uninitialize();
        return 1;
    }

    /* Read Command Arguments */
    for (auto x = 1; x < argc; ++x)
    {
        /* Server Address Argument */
        if (!_strnicmp(argv[x], "--server", 8))
        {
            globalVars->setServerAddress(argv[++x]);
            continue;
        }

        /* Server Port Argument */
        if (!_strnicmp(argv[x], "--port", 6))
        {
            lobbyServerPort = argv[++x];
            continue;
        }

        /* Username Argument */
        if (!_strnicmp(argv[x], "--user", 6))
        {
            username = argv[++x];
            continue;
        }

        /* Password Argument */
        if (!_strnicmp(argv[x], "--pass", 6))
        {
            password = argv[++x];
            continue;
        }

        /* Language Argument */
        if (!_strnicmp(argv[x], "--lang", 6))
        {
            std::string lang = argv[++x];

            if (!_strnicmp(lang.c_str(), "JP", 2) || !_strnicmp(lang.c_str(), "0", 1))
                language = xiloader::Language::Japanese;
            if (!_strnicmp(lang.c_str(), "US", 2) || !_strnicmp(lang.c_str(), "1", 1))
                language = xiloader::Language::English;
            if (!_strnicmp(lang.c_str(), "EU", 2) || !_strnicmp(lang.c_str(), "2", 1))
                language = xiloader::Language::European;

            continue;
        }

        /* Hairpin Argument */
        if (!_strnicmp(argv[x], "--hairpin", 9))
        {
            bUseHairpinFix = true;
            continue;
        }

        /* Hide Argument */
        if (!_strnicmp(argv[x], "--hide", 6))
        {
            globalVars->setHide(true);
            continue;
        }

        /*Override Client Draw Distance*/
        if (!_strnicmp(argv[x], "--drawdistance", 14))
        {
            float dist = atof(argv[++x]);
            if (dist <= 0.0f)
                dist = 20.0f;
            globalVars->setDrawDistance(dist);

            bUseDrawDistanceHack = true;
            continue;
        }

        /*Override Client Mob Draw Distance*/
        if (!_strnicmp(argv[x], "--mobdistance", 14))
        {
            float dist = atof(argv[++x]);
            if (dist <= 0.0f)
                dist = 20.0f;
            globalVars->setMobDistance(dist);

            bUseMobDistanceHack = true;
            continue;
        }

        /*Override Client Framerate*/
        if (!_strnicmp(argv[x], "--fps", 5))
        {
            bUseFPSHack = true;
            continue;
        }

        /*Disable Client Language Filter*/
        if (!_strnicmp(argv[x], "--nofilter", 10))
        {
            bUseSwearHack = true;
            continue;
        }

xiloader::console::output(xiloader::color::warning, "Found unknown command argument: %s", argv[x]);
    }

    /* Attempt to resolve the server address.. */
    ULONG ulAddress = 0;
    std::string host;
    globalVars->getServerAddress(&host);
    if (xiloader::network::ResolveHostname(host.c_str(), &ulAddress))
    {
        char address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ulAddress, address, INET_ADDRSTRLEN);
        globalVars->setServerAddress(address);

        /* Attempt to create socket to server..*/
        xiloader::datasocket sock;
        SOCKET pol_socket;
        SOCKET pol_clientSocket;
        globalVars->getServerAddress(&host);
        if (xiloader::network::CreateConnection(&sock, host, "54231"))
        {
            /* Attempt to verify the users account info.. */
            while (!xiloader::network::VerifyAccount(&sock, host, username, password))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            bool hacksEnabled[] = { bUseHairpinFix, bUseDrawDistanceHack,
                bUseMobDistanceHack, bUseFPSHack, bUseSwearHack};

            /* Create listen servers.. */
            sharedState.isRunning = true;
            std::thread thread_ffxiServer(xiloader::network::FFXiDataComm, &sock, std::cref(host), std::ref(characterList), std::ref(sharedState));
            std::thread thread_polServer(xiloader::network::PolServer, std::ref(pol_socket), std::ref(pol_clientSocket), std::cref(lobbyServerPort), std::ref(sharedState));
            std::thread thread_ffxi(LaunchFFXI, hacksEnabled, std::cref(language), std::ref(characterList), std::ref(sharedState));

            std::unique_lock<std::mutex> lock(sharedState.mutex);
            sharedState.conditionVariable.wait(lock, [&] { return !sharedState.isRunning; });

            /* Cleanup sockets.. */
            xiloader::network::CleanupSocket(pol_socket, SD_RECEIVE);
            xiloader::network::CleanupSocket(pol_clientSocket, SD_RECEIVE);
            xiloader::network::CleanupSocket(sock.s, SD_SEND);

            /* Cleanup threads.. */
            thread_polServer.join();
            thread_ffxiServer.join();
            if (thread_ffxi.joinable())
            {
                thread_ffxi.join();
            }
        }
    }
    else
    {
        xiloader::console::output(xiloader::color::error, "Failed to resolve server hostname.");
    }

    /* Cleanup Winsock */
    WSACleanup();

    xiloader::console::output(xiloader::color::error, "Closing...");

    if (MH_Uninitialize() != MH_OK)
    {
        xiloader::console::output(xiloader::color::error, "Failed to cleanup hooking library.");
        return 1;
    }

    delete globalVars;
    return ERROR_SUCCESS;
}
