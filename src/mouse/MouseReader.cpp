#include "MouseReader.h"
#include <windows.h>

MouseReader::MouseReader()
{
    m_snapshot.percent = -1;
}

MouseReader::~MouseReader()
{
    Stop();
}

void MouseReader::Start()
{
    if (m_thread.joinable())
        return;
    m_stop = false;
    m_thread = std::thread(&MouseReader::WorkerLoop, this);
    m_thread.detach();
}

void MouseReader::Stop()
{
    m_stop = true;
}

BatterySnapshot MouseReader::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void MouseReader::UpdateSnapshotLocked(const BatteryReading& reading, bool online, ULONGLONG now_ms)
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

// 鼠标活动检测：光标移动 / 鼠标键（纯 Windows API，零 HID 写入，
// 不影响鼠标与接收器休眠；用于感知"鼠标被唤醒/正在使用"）
static bool MouseActivity(POINT& last_pos)
{
    POINT pt{};
    GetCursorPos(&pt);
    bool moved = (pt.x != last_pos.x || pt.y != last_pos.y);
    last_pos = pt;
    static const int keys[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    bool clicked = false;
    for (int k : keys)
    {
        if (GetAsyncKeyState(k) & 0x8000)
        {
            clicked = true;
            break;
        }
    }
    return moved || clicked;
}

// 采集：打开句柄 → QueryBattery（SetFeature 11 06 + GetFeature 读响应）→ 关闭。
// 查询后释放句柄（即查即关，不长期持有，避免与官方驱动/网页驱动争用集合）。
static bool CollectBattery(MouseDevice& device, BatteryReading& out,
                           std::wstring& out_product, std::wstring& out_path)
{
    if (!device.IsOpen())
    {
        if (!device.Open(device.DevicePath()) &&
            !device.Open(L""))
            return false;
    }

    out_product = device.ProductName();
    out_path = device.DevicePath();

    bool got = device.QueryBattery(out);
    device.Close();
    return got;
}

void MouseReader::WorkerLoop()
{
    MouseDevice device;
    constexpr ULONGLONG kQueryCooldownMs = 2000;     // 采集冷却（防抖）
    constexpr ULONGLONG kActivityRefreshMs = 5000;   // 使用中每 5 秒刷新一次
    constexpr ULONGLONG kActiveWindowMs = 30000;     // 最近 30 秒内有活动才视为"使用中"
    constexpr ULONGLONG kIdleWakeMs = 120000;        // 闲置满 120 秒后的首次活动 = 鼠标唤醒

    ULONGLONG last_query_ms = 0;
    ULONGLONG last_activity_ms = 0; // 最近一次检测到鼠标活动的时刻
    POINT last_pos{};
    GetCursorPos(&last_pos);

    while (!m_stop)
    {
        ULONGLONG now = GetTickCount64();

        // ---- 鼠标活动采样（GetCursorPos / GetAsyncKeyState，零 HID 写入）----
        bool active = MouseActivity(last_pos);
        bool wake = false;
        if (active)
        {
            if (last_activity_ms != 0 && now - last_activity_ms >= kIdleWakeMs)
                wake = true;
            last_activity_ms = now;
        }

        // ---- 采集触发条件（无周期轮询，鼠标可自然休眠）----
        bool need_collect = false;
        if (last_query_ms == 0)
        {
            need_collect = true;                 // ① 初次启动：采集一次
        }
        else if (wake)
        {
            need_collect = true;                 // ② 鼠标唤醒瞬间：采集一次
        }
        else if (now - last_activity_ms < kActiveWindowMs &&
                 now - last_query_ms >= kActivityRefreshMs)
        {
            need_collect = true;                 // ③ 使用中（最近 30 秒内有活动）每 5 秒刷新一次
        }

        if (need_collect && now - last_query_ms < kQueryCooldownMs)
            need_collect = false;

        if (need_collect)
        {
            BatteryReading r;
            std::wstring product, devpath;
            bool got = CollectBattery(device, r, product, devpath);
            last_query_ms = GetTickCount64();

            if (got)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_snapshot.has_device)
                {
                    m_snapshot.has_device = true;
                    m_snapshot.product = product;
                    m_snapshot.device_path = devpath;
                }
                UpdateSnapshotLocked(r, true, GetTickCount64());
            }
            // 查询失败（休眠/唤醒中链路未就绪、接收器被拔出）：
            // 不做离线判定，保持最后读数继续显示
        }

        // 空闲 tick（~80ms）：不持有句柄、不发送任何数据，
        // 只做鼠标活动检测——鼠标/接收器完全自然休眠
        if (!m_stop)
            Sleep(80);
    }

    device.Close();
}
