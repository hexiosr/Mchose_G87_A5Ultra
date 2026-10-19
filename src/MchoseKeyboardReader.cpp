#include "MchoseKeyboardReader.h"
#include <windows.h>

MchoseKeyboardReader::MchoseKeyboardReader()
{
    m_snapshot.percent = -1;
}

MchoseKeyboardReader::~MchoseKeyboardReader()
{
    Stop();
}

void MchoseKeyboardReader::Start()
{
    if (m_thread.joinable())
        return;
    m_stop = false;
    m_thread = std::thread(&MchoseKeyboardReader::WorkerLoop, this);
    m_thread.detach(); // 插件生命周期内常驻；进程退出时随进程终止
}

void MchoseKeyboardReader::Stop()
{
    m_stop = true;
}

BatterySnapshot MchoseKeyboardReader::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void MchoseKeyboardReader::UpdateSnapshotLocked(const MchoseBatteryReading& reading, bool online, ULONGLONG now_ms)
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

// 检测是否有任意键盘键被按下（Windows API，不对 HID 设备做任何写入，
// 不影响键盘/接收器休眠；用于感知"键盘被唤醒/正在使用"）
static bool AnyKeyPressed()
{
    // 跳过鼠标键（0x01..0x07），只扫键盘键
    for (int vk = 0x08; vk <= 0xFE; ++vk)
    {
        if (GetAsyncKeyState(vk) & 0x8000)
            return true;
    }
    return false;
}

void MchoseKeyboardReader::WorkerLoop()
{
    MchoseKeyboardDevice device;
    constexpr DWORD kReconnectDelay = 3000;          // 打开设备失败的重试间隔
    constexpr ULONGLONG kQueryCooldownMs = 2000;     // 查询防抖
    constexpr int kMaxQueryFailures = 3;             // 连续查询失败达阈值 → 判离线
    constexpr ULONGLONG kActivityRefreshMs = 300000; // 使用中每 5 分钟刷新一次
    constexpr ULONGLONG kActiveWindowMs = 30000;     // 30 秒内有按键才视为"使用中"
    constexpr ULONGLONG kIdleWakeMs = 60000;         // 闲置满 60 秒后的首次按键 = 键盘唤醒

    int query_failures = 0;
    ULONGLONG last_query_ms = 0;
    ULONGLONG last_key_activity_ms = 0; // 最近一次检测到按键的时刻
    bool ever_opened = false;

    while (!m_stop)
    {
        ULONGLONG now = GetTickCount64();

        // ---- 按键状态采样（GetAsyncKeyState，纯 Windows API，零 HID 写入）----
        // 打字时每次按键间隔很短（< 60s），不会被误判为唤醒；
        // 只有闲置满 60 秒后的【首次】按键（键盘休眠后唤醒）才触发查询
        bool key_down = AnyKeyPressed();
        bool wake = false;
        if (key_down)
        {
            if (last_key_activity_ms != 0 && now - last_key_activity_ms >= kIdleWakeMs)
                wake = true;
            last_key_activity_ms = now;
        }

        // ---- 查询触发条件（无任何周期轮询，键盘可自然休眠）----
        bool need_query = false;
        if (last_query_ms == 0)
        {
            need_query = true;                    // ① 初次启动：连接后查一次
        }
        else if (wake)
        {
            need_query = true;                    // ② 键盘唤醒瞬间：查一次
        }
        else if (now - last_key_activity_ms < kActiveWindowMs &&
                 now - last_query_ms >= kActivityRefreshMs)
        {
            need_query = true;                    // ③ 使用中每 5 分钟刷新一次
        }

        // 防抖：距上次查询不足 2 秒则跳过
        if (need_query && now - last_query_ms < kQueryCooldownMs)
            need_query = false;

        if (need_query)
        {
            // ---- 查询前确保句柄打开（句柄只在查询瞬间持有）----
            if (!device.IsOpen())
            {
                bool ok = device.Open(device.DevicePath());
                // 旧路径失效（接收器被拔插/重置导致路径变化）→ 清空路径全量搜索
                if (!ok && !device.DevicePath().empty())
                    ok = device.Open(L"");
                if (!ok)
                {
                    if (ever_opened)
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_snapshot.online = false;
                    }
                    if (!m_stop)
                        Sleep(kReconnectDelay);
                    continue;
                }
                ever_opened = true;
                query_failures = 0;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_snapshot.has_device = true;
                    m_snapshot.product = device.ProductName();
                    m_snapshot.device_path = device.DevicePath();
                }
            }

            // ---- 临时开会话 + 查询（不开会话 dongle 对查询静音）----
            device.SendSessionOpen();
            if (!m_stop)
                Sleep(100);
            device.SendBatteryQuery();

            bool got = false;
            ULONGLONG deadline = GetTickCount64() + 600;
            while (!m_stop && GetTickCount64() < deadline)
            {
                BYTE frame[64]{};
                if (!device.ReadInput(frame, 100))
                    continue;
                MchoseBatteryReading r = MchoseKeyboardDevice::ParseQueryResponse(frame, sizeof(frame));
                if (r.valid)
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        UpdateSnapshotLocked(r, true, GetTickCount64());
                    }
                    got = true;
                    break;
                }
                if (MchoseKeyboardDevice::ParsePushFrame(frame, sizeof(frame), r))
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        UpdateSnapshotLocked(r, true, GetTickCount64());
                    }
                    got = true;
                    break;
                }
            }

            last_query_ms = GetTickCount64();
            if (got)
            {
                query_failures = 0;
            }
            else
            {
                // 无响应：键盘可能休眠（dongle 静音）——不立即离线；
                // 连续多次失败才判离线（接收器拔出等）
                ++query_failures;
                if (query_failures >= kMaxQueryFailures)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_snapshot.online = false;
                }
            }

            // ---- 释放会话：关闭句柄，下次查询再重开 ----
            // dongle 的配置会话一次只服务一个客户端；句柄关闭后会话释放，
            // 官方驱动/网页驱动可随时正常连接设置，无需关掉本插件。
            device.Close();

            if (!m_stop)
                Sleep(200);
        }
        else
        {
            // 空闲 tick（~80ms）：不持有句柄、不发送任何数据，
            // 只做按键检测——键盘/接收器完全自然休眠
            if (!m_stop)
                Sleep(80);
        }
    }

    // 线程退出：释放设备句柄
    device.Close();
}
