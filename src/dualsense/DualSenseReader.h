#pragma once
// ============================================================
// DualSenseReader.h
// PS5 DualSense 后台读取线程
// DS5Dongle 桥接后手柄为 USB 形态（持续供电），无休眠顾虑，
// 采用固定间隔查询（每 60 秒一次，电量变化慢，无需高频）
// ============================================================
#include "DualSenseDevice.h"
#include "BatteryCommon.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class DualSenseReader
{
public:
    DualSenseReader();
    ~DualSenseReader();

    void Start();
    void Stop();                       // 请求停止（非阻塞）

    BatterySnapshot GetSnapshot() const;

private:
    void WorkerLoop();
    void UpdateSnapshotLocked(const BatteryReading& reading, bool online, ULONGLONG now_ms);

private:
    std::atomic<bool> m_stop{ false };
    std::thread m_thread;
    mutable std::mutex m_mutex;
    BatterySnapshot m_snapshot;
};
