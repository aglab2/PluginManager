#include "hooks.h"
#include "hooks_priv.h"

#include "backtrace.h"
#include "minizip.h"
#include "pj64_globals.h"
#include "timer.h"
#include "reset_recognizer.h"
#include "unzdispatch.h"

#include <stdint.h>

#include <Commctrl.h>

constexpr char NAME[] = "LINK's Project64";

static HookManager gHookManager;

static int unprotect(void* address, size_t size)
{
    DWORD old_flags;
    BOOL result = VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_flags);
    return 0 != result;
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

static void doWriteCall(uintptr_t codeStart, size_t sz, void* fn)
{
    uint8_t* hook = (uint8_t*)codeStart;
    *hook = 0xE8;
    *(uintptr_t*)(hook + 1) = (uintptr_t)(fn)-(uintptr_t)(hook + 5);

    fillNop(hook + 5, sz - 5);
}

static void doWriteJump(uintptr_t codeStart, size_t sz, void* fn)
{
    uint8_t* hook = (uint8_t*)codeStart;
    *hook = 0xE9;
    *(uintptr_t*)(hook + 1) = (uintptr_t)(fn)-(uintptr_t)(hook + 5);

    fillNop(hook + 5, sz - 5);
}

static void writeCall(uintptr_t codeStart, size_t sz, void* fn)
{
    unprotect((void*)codeStart, sz);
    doWriteCall(codeStart, sz, fn);
}

static void writeJump(uintptr_t codeStart, size_t sz, void* fn)
{
    unprotect((void*)codeStart, sz);
    doWriteJump(codeStart, sz, fn);
}

void HookManager::init()
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
    writeCall(0x0041DD35, 0x0041DD5F + 2 - 0x0041DD35, &hookCloseCpuRomClosed);

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

    /*
    OpenChosenFile
    00449ED6  jl          00449EC9  
    00449ED8  mov         ecx,dword ptr ds:[4D7F50h]  
    00449EDE  push        ecx  
    00449EDF  call        dword ptr ds:[46720Ch]  
    00449EE5  push        3E8h  
    // Useless Sleep(1000)
    > 00449EEA  call        dword ptr ds:[46719Ch]  
    00449EF0  call        0041DC00  
    */
    fillNop((uint8_t*)0x00449EE5, 0x00449EF0 - 0x00449EE5);
    /*
    0044A7E6  push        ecx  
    0044A7E7  call        dword ptr ds:[467300h]  
    // Useless Sleep(100)
    0044A7ED  push        64h  
    0044A7EF  call        dword ptr ds:[46719Ch] 
    0044A7F5  mov         eax,dword ptr ds:[004D526Ch] 
    */
    fillNop((uint8_t*)0x0044A7ED, 0x0044A7F5 - 0x0044A7ED);
    /*
    0044A31B  push        0  
    0044A31D  push        401h  
    0044A322  push        edx  
    0044A323  call        ebp  
    0044A325  push        64h  
    0044A327  call        dword ptr ds:[46719Ch]  
    0044A32D  mov         eax,dword ptr ds:[004D526Ch] 
    */
    fillNop((uint8_t*)0x0044A325, 0x0044A32D - 0x0044A325);

    /*
    0041DC00  mov         eax,dword ptr ds:[004D7610h]
    0041DC05  sub         esp,8
    0041DC08  push        esi
    0041DC09  xor         esi,esi
    0041DC0B  cmp         eax,esi
    0041DC0D  je          0041DD9B
    0041DC13  cmp         dword ptr ds:[4D75E4h],esi
    0041DC19  mov         dword ptr ds:[4D74C0h],esi
    0041DC1F  je          0041DC31
    0041DC21  call        0041FD00
    0041DC26  push        3E8h
    > Sleep
    0041DC2B  call        dword ptr ds:[46719Ch]
    0041DC31  mov         ecx,dword ptr ds:[4D7F50h]
    0041DC37  push        ebx
    0041DC38  push        ebp
    0041DC39  push        edi
    0041DC3A  mov         edi,dword ptr ds:[4D7FC4h]
    0041DC40  mov         dword ptr ds:[4D7FC4h],esi
    0041DC46  call        0040B460
    0041DC4B  mov         ebx,dword ptr ds:[4670A8h]
    0041DC51  mov         dword ptr ds:[4D7FC4h],edi
    0041DC57  mov         edi,dword ptr ds:[4670A4h]
    0041DC5D  mov         ebp,1
    0041DC62  mov         eax,dword ptr ds:[004D7380h]
    0041DC67  push        eax
    0041DC68  mov         dword ptr ds:[4D7388h],ebp
    0041DC6E  mov         dword ptr ds:[4D73A4h],0
    0041DC78  mov         dword ptr ds:[4D7384h],ebp
    0041DC7E  call        edi
    0041DC80  push        64h
    > Sleep
    0041DC82  call        dword ptr ds:[46719Ch]
    0041DC88  mov         edx,dword ptr ds:[4D75ECh]
    0041DC8E  lea         ecx,[esp+10h]
    0041DC92  push        ecx
    0041DC93  push        edx
    0041DC94  call        ebx
    0041DC96  cmp         dword ptr [esp+10h],103h
    0041DC9E  je          0041DCAF
    0041DCA0  mov         dword ptr ds:[4D75ECh],0
    0041DCAA  mov         esi,64h
    0041DCAF  inc         esi
    0041DCB0  cmp         esi,14h
    0041DCB3  jb          0041DC62
    0041DCB5  mov         eax,dword ptr ds:[004D75ECh]
    0041DCBA  xor         edi,edi
    0041DCBC  cmp         eax,edi
    0041DCBE  je          0041DCCE
    0041DCC0  push        edi
    0041DCC1  push        eax
    > TerminateThread
    0041DCC2  call        dword ptr ds:[4670A0h]
    0041DCC8  mov         dword ptr ds:[4D75ECh],edi
    0041DCCE  mov         ecx,dword ptr ds:[4D6A24h]
    */
    // Remove dumb sleep
    unprotect((void*)0x0041DC80, 0x0041DC96 - 0x0041DC80);
    uint8_t code[] = { 0x8D, 0x4C, 0x24, 0x10, 0x51 };
    memcpy((void*)0x0041DC80, code, sizeof(code));
    doWriteCall(0x0041DC80 + sizeof(code), 0x0041DC96 - (0x0041DC80 + sizeof(code)), &hookCloseCpu);

    // Some AV solution might slap me for this but I do not want to hook all 'SendMessage' calls from hCPU
    // TODO: Realistically there is only one "bad" call so hook only that
    *(uintptr_t*)0x467300 = (uintptr_t) &HookManager::hookSendMessageA;

    /*
    StartRecompilerCPU
    00432EA6  push        ebx  
    00432EA7  push        eax  
    00432EA8  call        dword ptr ds:[46718Ch]  
    00432EAE  mov         dword ptr ds:[4AEB3Ch],ebx  
    00432EB4  mov         eax,dword ptr ds:[004D81ACh]  
    00432EB9  cmp         eax,ebx  
    00432EBB  je          00432EBF  
    00432EBD  call        eax  
    00432EBF  mov         eax,dword ptr ds:[004D7FE4h]  
    00432EC4  cmp         eax,ebx  
    00432EC6  je          00432ECA  
    00432EC8  call        eax  
    00432ECA  call        0042BA40  
    00432ECF  mov         ecx,803h  
    00432ED4  xor         eax,eax  
    00432ED6  mov         edi,53C660h  
    00432EDB  rep stos    dword ptr es:[edi]
    */
    // Only handling recompiler case here, don't care about other crap
    writeCall(0x00432EB4, 0x00432ECA - 0x00432EB4, &hookStartRecompiledCpuRomOpen);

    // hook create file just for debugging
    // *(uintptr_t*)0x467068 = (uintptr_t)&HookManager::hookCreateFileA;

    // Optimized unz
    // call        0045CC98 - sprintf
    // 0044A27D  je          0044A5DC - if (!Allocate_ROM())
    // 0044A343  jne         0044A5F5 - if ((int)RomFileSize != len)
    // 0044A118  or          ecx,0FFFFFFFFh  strcpy start
    // 4AF0F0h - CurrentFileName
 
    // 0044A175  call        00419FB0 - unzOpen
    // 0044A1B9  call        0041AEE0 - unzGoToFirstFile
    // 0044A203  call        0041A590 - unzGetCurrentFileInfo
    // 0044A213  call        0041AFB0 - unzLocateFile
    // 0044A224  call        0041B1A0 - unzOpenCurrentFile
    // 0044A23C  call        0041B5C0 - unzReadCurrentFile
    // 0044A5DD  call        0041B7B0 - unzCloseCurrentFile
    // 0044A5E3  call        0041A520 - unzClose
    // 0044A578  call        0041AF30 - unzGoToNextFile
#if 0
    writeJump(0x00419FB0, 5, unzOpen);
    writeJump(0x0041AEE0, 5, unzGoToFirstFile);
    writeJump(0x0041A590, 5, unzGetCurrentFileInfo);
    writeJump(0x0041AFB0, 5, unzLocateFile);
    writeJump(0x0041B1A0, 5, unzOpenCurrentFile);
    writeJump(0x0041B5C0, 5, unzReadCurrentFile);
    writeJump(0x0041B7B0, 5, unzCloseCurrentFile);
    writeJump(0x0041A520, 5, unzClose);
    writeJump(0x0041AF30, 5, unzGoToNextFile);
#endif
#if 1
    writeJump(0x00419FB0, 5, unzDispatchOpen);
    writeJump(0x0041AEE0, 5, unzDispatchGoToFirstFile);
    writeJump(0x0041A590, 5, unzDispatchGetCurrentFileInfo);
    writeJump(0x0041AFB0, 5, unzDispatchLocateFile);
    writeJump(0x0041B1A0, 5, unzDispatchOpenCurrentFile);
    writeJump(0x0041B5C0, 5, unzDispatchReadCurrentFile);
    writeJump(0x0041B7B0, 5, unzDispatchCloseCurrentFile);
    writeJump(0x0041A520, 5, unzDispatchClose);
    writeJump(0x0041AF30, 5, unzDispatchGoToNextFile);
#endif

    // CF1 by default
    // 004492FA C7 05 E0 75 4D 00 02 00 00 00 mov         dword ptr ds:[4D75E0h],2 
    unprotect((void*)0x449300, 1);
    *(uint8_t*) 0x449300 = 1;
}

#define INVOKE_PJ64_PLUGIN_CALLBACK(name) if (auto fn = PJ64::Globals::name()) { fn(); }

static bool gIsInitialized = false;
void HookManager::hookCloseCpuRomClosed()
{
    auto type = ResetRecognizer::recognize(ResetRecognizer::stackAddresses());
    bool fastReset = type == ResetRecognizer::Type::OPEN_ROM || type == ResetRecognizer::Type::RESET;
    if (1 != PJ64::Globals::CPU_Type() || !fastReset)
    {
        gIsInitialized = false;
        INVOKE_PJ64_PLUGIN_CALLBACK(GfxRomClosed)
        INVOKE_PJ64_PLUGIN_CALLBACK(AiRomClosed)
        INVOKE_PJ64_PLUGIN_CALLBACK(ContRomClosed)
    }
    INVOKE_PJ64_PLUGIN_CALLBACK(RSPRomClosed)
}

void HookManager::hookStartRecompiledCpuRomOpen()
{
    if (!gIsInitialized)
    {
        gIsInitialized = true;
        INVOKE_PJ64_PLUGIN_CALLBACK(GfxRomOpen)
        INVOKE_PJ64_PLUGIN_CALLBACK(ContRomOpen)
    }
}

void HookManager::hookMachine_LoadStateRomReinit()
{
    // INVOKE_PJ64_PLUGIN_CALLBACK(GfxRomClosed)
    // INVOKE_PJ64_PLUGIN_CALLBACK(AiRomClosed)
    // INVOKE_PJ64_PLUGIN_CALLBACK(ContRomClosed)
    INVOKE_PJ64_PLUGIN_CALLBACK(RSPRomClosed)
    // INVOKE_PJ64_PLUGIN_CALLBACK(GfxRomOpen)
    // INVOKE_PJ64_PLUGIN_CALLBACK(ContRomOpen)

    // Fix for the stutters
    PJ64::Globals::FPSTimer()->reset();
}

void __stdcall HookManager::hookCloseCpu(DWORD* ExitCode)
{
    HANDLE hCPU = PJ64::Globals::hCPU();
    WaitForSingleObject(hCPU, 100);
    GetExitCodeThread(hCPU, ExitCode);
}

#define LINE_LENGTH 128
#define LINE_CNT 16
static int sContentCycle = 0;
static char sContent[LINE_LENGTH*LINE_CNT];
LRESULT WINAPI HookManager::hookSendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    DWORD hCPUId = GetThreadId(PJ64::Globals::hCPU());
    DWORD curThreadId = GetCurrentThreadId();
    if (hCPUId == curThreadId)
    {
        if (Msg == SB_SETTEXT)
        {
            char* line = &sContent[LINE_LENGTH * (sContentCycle % LINE_CNT)];
            sContentCycle++;
            strncpy_s(line, LINE_LENGTH, (char*)lParam, LINE_LENGTH);
            ::PostMessageA(hWnd, Msg, wParam, (LPARAM)line);
            return 0;
        }
        else
        {
            return ::SendMessageA(hWnd, Msg, wParam, lParam);
        }
    }
    else
    {
        return ::SendMessageA(hWnd, Msg, wParam, lParam);
    }
}

HANDLE WINAPI HookManager::hookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static bool tryNamePJ64() noexcept
{
    bool ok = false;
    __try
    {
        ok = true;
        ok = !ok ? ok : SetWindowText(PJ64::Globals::MainWindow(), NAME);
        ok = !ok ? ok : SetWindowText(PJ64::Globals::HiddenWin(), NAME);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // not ok
        return false;
    }

    return ok;
}

// MARK: Enterance from 'RSP' init
void plantHooks()
{
    bool ok = false;
    // Check if we are in PJ64 1.6, heuristically
    {
        auto addresses = Backtrace::collectStackAddresses();
        constexpr uintptr_t PJ64GetDllInfoSym0 = 0x0044dce7;
        constexpr uintptr_t PJ64GetDllInfoSym1 = 0x0044dcf7;
        for (uintptr_t addr : addresses)
        {
            if (PJ64GetDllInfoSym0 == addr || PJ64GetDllInfoSym1 == addr)
            {
                ok = true;
                break;
            }
        }
    } 

    ok = !ok ? ok : tryNamePJ64();

    if (ok)
    {
        gHookManager.init();
    }
}
