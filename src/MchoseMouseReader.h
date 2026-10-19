#pragma once
// ============================================================
// MchoseMouseReader.h
// 鼠标后台线程：按需采集（初次/唤醒/30秒刷新）+ 鼠标活动检测 + 断线重连
// 无周期轮询 -> 不会唤醒休眠中的鼠标/接收器
// ============================================================
#include "MchoseMouseDevice.h"
#include "BatteryCommon.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class MchoseMouseReader
{
public:
    MchoseMouseReader();
    ~MchoseMouseReader();

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
