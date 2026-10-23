#pragma once
// ============================================================
// MouseDevice.h
// 迈从（MCHOSE）A5 Ultra 2.4G 鼠标电量读取
// 协议逆向自官方 M HUB 网页驱动（www.mchose.com.cn assets/theme + purify）
//
// 适用设备：A5 Ultra 2.4G 接收器（VID_3837 & PID_100C，UsagePage 0xFF01）
//
// 电量获取（实测确认 debug-probe5：SetFeature(11 06) -> GetFeature 电量帧）：
//   ① SetFeature(0x11, 0x06^0xFF, 0xFF..., 21B)  写"请求设备信息"命令（反码编码）
//   ② GetFeature(0x11, 65B)                     读响应（0x06 设备信息帧）
//      响应原码/反码交替，以 resp[1] 判别：
//        resp[0]=0x11；resp[1]=0x06（原码帧）或 0xF9（反码帧）
//        电量=resp[11]（原码）或 ~resp[11]（反码）；充电标志=resp[12]
// ============================================================
#include <windows.h>
#include <string>
#include <atomic>
#include "BatteryCommon.h"

class MouseDevice
{
public:
    MouseDevice();
    ~MouseDevice();

    // 在系统中搜索 A5 Ultra 2.4G 电量通道（0xFF01）并打开。
    bool Open(const std::wstring& device_path = L"");

    void Close();

    bool IsOpen() const { return m_hDevice != nullptr; }

    const std::wstring& DevicePath() const { return m_device_path; }
    const std::wstring& ProductName() const { return m_product_name; }

    // 读取设备信息（含电量）：SetFeature(11 06) + GetFeature(0x11)。
    // 重试间的等待可被 cancel 打断（Reader Stop 时尽快返回，不拖住 join）。
    bool QueryBattery(BatteryReading& out, const std::atomic<bool>& cancel);

private:
    bool OpenPath(const std::wstring& path);
    bool SetFeatureSafe(const BYTE* buf, int len);
    static bool ParseFeatureResponse(const BYTE* resp, int len, BatteryReading& out);

private:
    HANDLE     m_hDevice = nullptr;
    std::wstring m_device_path;
    std::wstring m_product_name;
};
