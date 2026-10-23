#include "DualSenseReader.h"
#include <windows.h>

DualSenseReader::DualSenseReader()
{
    m_snapshot.percent = -1;
}

DualSenseReader::~DualSenseReader()
{
    Stop();
}

void DualSenseReader::Start()
{
    if (m_thread.joinable())
        return;
    m_stop = false;
    m_thread = std::thread(&DualSenseReader::WorkerLoop, this);
    m_thread.detach();
}

void DualSenseReader::Stop()
{
    m_stop = true;
}

BatterySnapshot DualSenseReader::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void DualSenseReader::UpdateSnapshotLocked(const BatteryReading& reading, bool online, ULONGLONG now_ms)
{
    m_snapshot.online = online;
    if (reading.valid)
    {
        m_snapshot.valid = true;
        m_snapshot.percent = reading.percent;
        m_snapshot.charging = reading.charging;
        m_snapshot.last_update_ms = now_ms;
    }
}

void DualSenseReader::WorkerLoop()
{
    DualSenseDevice device;
    // 连接检测：本地设备枚举（SetupDi 硬件 ID 匹配），零 BT/USB 请求、不影响耗电
    constexpr ULONGLONG kPnpCheckMs = 2000;   // 每 2s 检查设备是否在系统
    // 电量读取：纯被动 ReadFile，60s 一次（与最初被动版一致的节奏）
    constexpr ULONGLONG kReadMs = 60000;

    ULONGLONG last_pnp_ms = 0;
    ULONGLONG last_read_ms = 0;
    bool prev_present = false; // 上次 PnP 检查的设备状态（检测"刚刚出现"）

    while (!m_stop)
    {
        ULONGLONG now = GetTickCount64();

        if (last_pnp_ms == 0 || now - last_pnp_ms >= kPnpCheckMs)
        {
            last_pnp_ms = now;

            if (!DualSenseDevice::IsPresent())
            {
                // 设备不在系统（手柄蓝牙断开 → DS5Dongle 隐藏 USB 设备）→ 显示 --
                std::lock_guard<std::mutex> lock(m_mutex);
                m_snapshot.valid = false;
                m_snapshot.online = false;
                prev_present = false;
                device.Close();
                if (!m_stop)
                    Sleep(200);
                continue;
            }

            // 设备在系统。设备刚刚出现（PnP 到达）→ 立即读一次电量，不等 60s 间隔
            bool just_appeared = !prev_present;
            if (just_appeared)
                last_read_ms = 0;

            if (last_read_ms == 0 || now - last_read_ms >= kReadMs)
            {
                if (!device.IsOpen() && !device.Open(device.DevicePath()))
                {
                    // 打开失败（设备重枚举瞬间/驱动未就绪）→ 视为尚未真正可用，下次 2s 再试
                    prev_present = false;
                    if (!m_stop)
                        Sleep(200);
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (!m_snapshot.has_device)
                    {
                        m_snapshot.has_device = true;
                        m_snapshot.product = device.ProductName();
                        m_snapshot.device_path = device.DevicePath();
                    }
                }

                BatteryReading r;
                bool got = device.QueryBattery(r);
                if (got)
                {
                    last_read_ms = now; // 仅读取成功才进入 60s 节流；失败保持快速重试
                    std::lock_guard<std::mutex> lock(m_mutex);
                    UpdateSnapshotLocked(r, true, GetTickCount64());
                }
                // 查询失败/超时（待机无上报）→ 保持最后读数，连接状态已由 IsPresent 保证
                device.Close(); // 即查即关
                prev_present = true;
            }
        }

        if (!m_stop)
            Sleep(200);
    }

    device.Close();
}
