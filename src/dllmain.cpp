// ============================================================
// dllmain.cpp —— 插件 DLL 入口
// ============================================================
#include <windows.h>
#include "MchoseKeyboardReader.h"
#include "MchoseMouseReader.h"

// 全局读取器实例由插件主类持有；此处仅用于在进程退出时请求停止后台线程
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_DETACH:
        // 请求停止后台线程（不等待，避免在 Loader Lock 下死锁；进程退出时会自然终止）
        break;
    default:
        break;
    }
    return TRUE;
}
