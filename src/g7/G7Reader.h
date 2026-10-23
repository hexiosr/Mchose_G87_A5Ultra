#pragma once
// ============================================================
// G7Reader.h
// G7 Pro 后台监听线程：持续只读旁观 \\.\XboxGIP 总线，
// 解析设备主动上报的 Status 消息（电量档位 + 充电标志）。
// 短暂无消息保持最后读数；90 秒无消息视为手柄断开（显示"未连接"），
// 设备唤醒/插拔会自动收到新消息而更新。
// ============================================================
#include "G7Device.h"
#include "BatteryCommon.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class G7Reader
{
public:
    ~G7Reader();

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
