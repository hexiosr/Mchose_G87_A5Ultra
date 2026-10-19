#pragma once
// ============================================================
// MchoseKeyboardReader.h
// 键盘后台线程：按需查询（初次/唤醒/5分钟刷新）+ 按键检测 + 断线重连
// 不周期查询 -> 不会唤醒休眠中的键盘
// ============================================================
#include "MchoseKeyboardDevice.h"
#include "BatteryCommon.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class MchoseKeyboardReader
{
public:
    MchoseKeyboardReader();
    ~MchoseKeyboardReader();

    void Start();
    void Stop();                       // 请求停止（非阻塞）
    bool IsRunning() const { return m_thread.joinable(); }

    BatterySnapshot GetSnapshot() const;

private:
    void WorkerLoop();
    void UpdateSnapshotLocked(const MchoseBatteryReading& reading, bool online, ULONGLONG now_ms);

private:
    std::atomic<bool> m_stop{ false };
    std::thread m_thread;
    mutable std::mutex m_mutex;
    BatterySnapshot m_snapshot;
};
