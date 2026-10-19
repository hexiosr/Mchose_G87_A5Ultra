// ============================================================
// dllmain.cpp —— 插件 DLL 入口
// ============================================================
#include <windows.h>

// 后台读取线程由插件主类持有，线程 detach 后随进程退出自然终止；
// 不在 DLL_PROCESS_DETACH 中等待线程，避免 Loader Lock 下死锁。
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}
