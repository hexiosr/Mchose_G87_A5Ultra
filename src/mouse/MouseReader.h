#pragma once
// ============================================================
// MouseReader.h
// 鼠标后台线程：按需采集（初次/唤醒/使用中每 5 秒刷新）+ 鼠标活动检测 + 断线重连
// 无周期轮询 -> 不会唤醒休眠中的鼠标/接收器
// ============================================================
#include "MouseDevice.h"
#include "BatteryCommon.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class MouseReader
{
public:
    ~MouseReader();

    void Start();
    void Stop();                       // 请求停止并 join 等待线程退出（线程内长等待可被打断）

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
