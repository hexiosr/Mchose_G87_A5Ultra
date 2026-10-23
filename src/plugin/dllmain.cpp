// ============================================================
// dllmain.cpp —— 插件 DLL 入口
// ============================================================
#include <windows.h>

// 后台读取线程由插件主类持有（不 detach）：选项变更时 Stop() join 确认线程退出；
// 进程退出时 ExitProcess 先终止全部工作线程，静态对象析构（DLL_PROCESS_DETACH
// 阶段）中 join 已终结的线程立即返回，不会在 Loader Lock 下死锁。
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}
