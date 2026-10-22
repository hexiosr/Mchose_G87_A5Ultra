#include "G7Reader.h"
#include <windows.h>
#include <cstring>

G7Reader::G7Reader()
{
    m_snapshot.percent = -1;
}

G7Reader::~G7Reader()
{
    Stop();
}

void G7Reader::Start()
{
    if (m_thread.joinable())
        return;
    m_stop = false;
    m_thread = std::thread(&G7Reader::WorkerLoop, this);
    m_thread.detach(); // 插件生命周期内常驻；进程退出时随进程终止
}

void G7Reader::Stop()
{
    m_stop = true;
}

BatterySnapshot G7Reader::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void G7Reader::UpdateSnapshotLocked(const BatteryReading& reading, bool online, ULONGLONG now_ms)
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

// GIP Status 消息电池字节：低 2 位档位 0~3，bit4 充电标志（逆向实测）
static void ParseStatusBattery(BYTE battery_byte, int& percent, bool& charging)
{
    int level = battery_byte & 0x03;
    charging = (battery_byte & 0x10) != 0;

    switch (level)
    {
    case 0: percent = 0;   break;
    case 1: percent = 33;  break;
    case 2: percent = 66;  break;
    case 3: percent = 100; break;
    default: percent = -1; break;
    }
}

void G7Reader::WorkerLoop()
{
    G7Device device;
    BYTE dev_id[8] = { 0 };          // 当前关注的接收器设备 ID（首个 Status 消息学习）
    ULONGLONG last_msg_ms = 0;       // 最近一次收到该设备消息的时间

    constexpr DWORD kReconnectDelay = 3000;     // 打不开 GIP 总线的重试间隔
    constexpr DWORD kReadTimeoutMs = 5000;      // 单次读超时（无消息则继续等）
    constexpr ULONGLONG kIdleResetMs = 60000;   // 60s 无消息 → 认为接收器被换插，重置设备 ID 重新学习
    constexpr ULONGLONG kNoMsgOfflineMs = 90000;// 90s 无消息 → 视为手柄断开（正常连接约 20s 一条 Status）

    while (!m_stop)
    {
        if (!device.IsOpen())
        {
            bool ok = device.Open();
            if (!ok)
            {
                // GIP 总线打不开（驱动未就绪/系统无 GIP 设备）→ 保持最后读数，延迟重试
                if (!m_stop)
                    Sleep(kReconnectDelay);
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_snapshot.has_device)
                {
                    m_snapshot.has_device = true;
                    m_snapshot.product = device.ProductName();
                }
            }
        }

        BYTE buf[512];
        int len = 0;
        ULONGLONG now = GetTickCount64();

        if (!device.ReadMessage(buf, len, kReadTimeoutMs))
        {
            // 超时无消息
            if (last_msg_ms != 0 && now - last_msg_ms >= kIdleResetMs)
                memset(dev_id, 0, sizeof(dev_id)); // 接收器可能被换插，重新学习设备 ID
            // 兜底：长时间无消息 → 视为手柄断开，显示 --（防止漏掉断开标志消息）
            if (last_msg_ms != 0 && now - last_msg_ms >= kNoMsgOfflineMs)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_snapshot.valid = false;
                m_snapshot.online = false;
            }
            continue;
        }

        if (len < 24)
            continue;

        BYTE type = buf[8];
        if (type != 0x03) // GipMessageStatus
            continue;

        // 设备 ID 学习/过滤（只认当前接收器）
        if (dev_id[0] == 0)
            memcpy(dev_id, buf, 8);
        else if (memcmp(dev_id, buf, 8) != 0)
            continue; // 其他设备的 Status，忽略

        // Status 消息 24B：12 字节头部 + 12 字节 payload
        // 电池字节 payload[8] = buf[20]（低 2 位档位 0~3，bit4 充电标志）
        // 链路状态字节 payload[11] = buf[23]：
        //   0x28 = 无线链路连接（bit3=1）；0x20 = 断开（bit3=0，实测断开瞬间）
        BYTE battery_byte = buf[20];
        BYTE link_byte    = buf[23];

        if ((link_byte & 0x08) == 0)
        {
            // 手柄断开（关闭/休眠）：显示 --（与其他设备"保持末值"不同，
            // G7 手柄按用户要求断开即显示未连接）
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_snapshot.valid = false;
                m_snapshot.online = false;
            }
            last_msg_ms = GetTickCount64(); // 刷新活动时间，避免 60s 闲置重置设备 ID
            continue;
        }

        int percent = -1;
        bool charging = false;
        ParseStatusBattery(battery_byte, percent, charging);
        if (percent < 0)
            continue;

        BatteryReading r;
        r.valid = true;
        r.percent = percent;
        r.charging = charging;

        last_msg_ms = GetTickCount64();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            UpdateSnapshotLocked(r, true, GetTickCount64());
        }
    }

    device.Close();
}
