#pragma once
// ============================================================
// DualSenseDevice.h
// PS5 DualSense 手柄电量读取（DS5Dongle 桥接 = USB 形态）
//
// 适用设备：Sony DualSense（VID_054C / PID_0CE6）及 Edge（PID_0DF2），
//           DS5Dongle（Pico 2W）将蓝牙手柄桥接为 USB 有线连接后即为该 VID/PID。
//
// 协议（controllers.fandom.com/wiki/Sony_DualSense USBGetStateData）：
//   读取 Input Report 0x01（64 字节，含 Report ID）：
//     resp[52].0..3  = PowerPercent（0x00-0x0A，即 0~10 档，percent = min(档位*10+5, 100)）
//     resp[52].4..7  = PowerState（0x01=Charging，0x02=Complete，0x00=Discharging）
// ============================================================
#include <windows.h>
#include <string>
#include <atomic>
#include "BatteryCommon.h"

class DualSenseDevice
{
public:
    DualSenseDevice();
    ~DualSenseDevice();

    // 搜索 DualSense（054C:0CE6 / 0DF2）并打开；可指定设备路径重连
    bool Open(const std::wstring& device_path = L"");

    // 检查 DualSense HID 设备当前是否在系统中（本地设备枚举，零 BT/USB 请求）。
    // 手柄蓝牙断开时 DS5Dongle 会隐藏/重枚举 USB 设备（PnP 事件），设备即消失。
    static bool IsPresent();

    void Close();

    bool IsOpen() const { return m_hDevice != nullptr; }

    const std::wstring& DevicePath() const { return m_device_path; }
    const std::wstring& ProductName() const { return m_product_name; }

    // 读取 Input Report 0x01 解析电量 / 充电状态（纯被动 ReadFile，分片等待）。
    // cancel 置位（Reader Stop）时 100ms 内放弃等待，保证 Stop() 的 join 及时返回。
    bool QueryBattery(BatteryReading& out, const std::atomic<bool>& cancel, DWORD timeout_ms = 3000);

private:
    bool OpenPath(const std::wstring& path);
    static bool ParseInputReport(const BYTE* buf, int len, BatteryReading& out);

private:
    HANDLE     m_hDevice = nullptr;
    std::wstring m_device_path;
    std::wstring m_product_name;
};
