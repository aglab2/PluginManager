#pragma once

class HookManager
{
public:
    HookManager() = default;

    void init();

private:
    void romClosed(bool fromSavestate);

    static void plantRomClosed();
    static void hookCloseCpuRomClosed();
    static void hookMachine_LoadStateRomReinit();
};
