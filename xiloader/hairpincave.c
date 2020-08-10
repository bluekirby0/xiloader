#ifdef _MSC_VER
#include "hairpincave.h"
typedef unsigned long DWORD;

extern DWORD g_NewServerAddress;
extern DWORD g_HairpinReturnAddress;

/**
 * @brief Hairpin fix codecave.
 */
__declspec(naked) void HairpinFixCave()
{
    __asm mov eax, g_NewServerAddress
    __asm mov [edx + 0x012E90], eax
    __asm mov [edx], eax
    __asm jmp g_HairpinReturnAddress
}
#endif