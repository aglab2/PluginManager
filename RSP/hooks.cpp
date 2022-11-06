#include "hooks.h"
#include "hooks_priv.h"

#include "pj64.h"

#include <stdint.h>

#define NAME "LINK's Project64"

static HookManager gHookManager;

void HookManager::init()
{
    plantRomClosed();
}

static int unprotect(void* address, size_t size)
{
    DWORD old_flags;
    BOOL result = VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_flags);
    return !result;
}

static uint8_t nops[] =
{ 0x90,
  0x66, 0x90,
  0x0F, 0x1F, 0x00,
  0x0F, 0x1F, 0x40, 0x00,
  0x0F, 0x1F, 0x44, 0x00, 0x00,
  0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00,
  0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#define MAX_NOP_SIZE 9 

static void fillNop(uint8_t* address, size_t size)
{
#if 1
    while (size)
    {
        size_t n = size > MAX_NOP_SIZE ? MAX_NOP_SIZE : size;
        uint8_t* nopi = &nops[n * (n - 1) / 2];
        memcpy(address, nopi, n);
        size -= n;
        address += n;
    }
#else
    memset(address, 0x90, size);
#endif
}

static void writeCall(uintptr_t codeStart, size_t sz, void* fn)
{
    unprotect((void*)codeStart, sz);

    uint8_t* hook = (uint8_t*)codeStart;
    *hook = 0xE8;
    *(uintptr_t*)(hook + 1) = (uintptr_t)(fn) - (uintptr_t)(hook + 5);

    fillNop(hook + 5, sz - 5);
}

void HookManager::plantRomClosed()
{
    /*
    CloseCpu
    0041DD35  mov         eax,dword ptr ds:[004D803Ch]
    0041DD3A  cmp         eax,edi
    0041DD3C  je          0041DD40
    0041DD3E  call        eax
    0041DD40  mov         eax,dword ptr ds:[004D7F84h]
    0041DD45  cmp         eax,edi
    0041DD47  je          0041DD4B
    0041DD49  call        eax
    0041DD4B  mov         eax,dword ptr ds:[004D819Ch]
    0041DD50  cmp         eax,edi
    0041DD52  je          0041DD56
    0041DD54  call        eax
    0041DD56  mov         eax,dword ptr ds:[004D7FB8h]
    0041DD5B  cmp         eax,edi
    0041DD5D  je          0041DD61
    0041DD5F  call        eax
    >>>
    0041DD61  cmp         dword ptr ds:[4D81A4h],edi
    */
    // Useless currently
    // writeCall(0x0041DD35, 0x0041DD5F + 2 - 0x0041DD35, &hookCloseCpuRomClosed);

    /*
    Machine_LoadState
    0041F2FE  mov         eax,dword ptr ds:[004D803Ch]
    0041F303  cmp         eax,ebx
    0041F305  je          0041F309
    0041F307  call        eax
    0041F309  mov         eax,dword ptr ds:[004D7F84h]
    0041F30E  cmp         eax,ebx
    0041F310  je          0041F314
    0041F312  call        eax
    0041F314  mov         eax,dword ptr ds:[004D819Ch]
    0041F319  cmp         eax,ebx
    0041F31B  je          0041F31F
    0041F31D  call        eax
    0041F31F  mov         eax,dword ptr ds:[004D7FB8h]
    0041F324  cmp         eax,ebx
    0041F326  je          0041F32A
    0041F328  call        eax
    0041F32A  mov         eax,dword ptr ds:[004D81ACh]
    0041F32F  cmp         eax,ebx
    0041F331  je          0041F335
    0041F333  call        eax
    0041F335  mov         eax,dword ptr ds:[004D7FE4h]
    0041F33A  cmp         eax,ebx
    0041F33C  je          0041F340
    0041F33E  call        eax
    */
    writeCall(0x0041F2FE, 0x0041F33E - 0x0041F2FE + 2, &hookMachine_LoadStateRomReinit);
}

void HookManager::hookCloseCpuRomClosed()
{
    auto gfxRomClosed = GfxRomClosed();
    auto aiRomClosed = AiRomClosed();
    auto contRomClosed = ContRomClosed();
    auto rspRomClosed = RSPRomClosed();

    if (gfxRomClosed) gfxRomClosed();
    if (aiRomClosed) aiRomClosed();
    if (contRomClosed) contRomClosed();
    if (rspRomClosed) rspRomClosed();
}

void HookManager::hookMachine_LoadStateRomReinit()
{
    auto gfxRomClosed = GfxRomClosed();
    auto aiRomClosed = AiRomClosed();
    auto contRomClosed = ContRomClosed();
    auto rspRomClosed = RSPRomClosed();
    auto gfxRomOpen = GfxRomOpen();
    auto contRomOpen = ContRomOpen();

    // if (gfxRomClosed) gfxRomClosed();
    // if (aiRomClosed) aiRomClosed();
    // if (contRomClosed) contRomClosed();
    if (rspRomClosed) rspRomClosed();
    // if (gfxRomOpen) gfxRomOpen();
    // if (contRomOpen) contRomOpen();
}

// MARK: Enterance from 'RSP' init
void plantHooks()
{
    bool ok = false;
    __try
    {
        ok = true;
        ok = !ok ? ok : SetWindowText(MainWindow(), NAME);
        ok = !ok ? ok : SetWindowText(HiddenWin(), NAME);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // not ok
        return;
    }

    if (ok)
    {
        gHookManager.init();
    }
}
