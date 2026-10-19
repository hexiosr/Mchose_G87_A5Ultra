#pragma once
// ============================================================
// MchoseKeyboardDevice.h
// 迈从（MCHOSE）G87 2.4G 键盘电量读取
// 协议移植自 VigilWatt docs/hardware/protocols.md §1（VID_41E4 老款接收器）
//
// 适用设备：MCHOSE G87 2.4G 接收器（VID_41E4 & PID_2001）
//   接口：MI_01 & Col01（UsagePage 0xFF02），输入/输出 20 字节，Report ID 0x13
//
// 协议要点：
//   - 输出帧 20 字节：[0]=0x13 [1]=命令 [2]=参数 [3..18]=0 [19]=前19字节求和&0xFF
//   - 开会话：13 05 01 00*16 19 （不开会话，dongle 对查询静音）
//   - 电量查询：13 4A 01 00*16 5E
//       响应：13 4A 01 00 02 <电量%> <充电标志> 00*12 <校验>
//       [5]=电量百分比（≤1 视为无效读数）；[6] 的 0x10 位=充电中
//   - 自发推送（[1]=0x0A）：[5]=消息类型；0x05=电量推送（[6]=电量%，[7]=充电标志）
// ============================================================
#include <windows.h>
#include <string>
#include "BatteryCommon.h"

class MchoseKeyboardDevice
{
public:
    MchoseKeyboardDevice();
    ~MchoseKeyboardDevice();

    // 在系统中搜索迈从键盘电量通道 HID 设备；找到后打开并读取接口信息。
    bool Open(const std::wstring& device_path = L"");

    void Close();

    bool IsOpen() const { return m_hDevice != nullptr; }

    const std::wstring& DevicePath() const { return m_device_path; }
    const std::wstring& ProductName() const { return m_product_name; }

    USHORT OutputReportLength() const { return m_output_length; }
    USHORT InputReportLength() const { return m_input_length; }

    // 开会话（dongle 记住会话状态；句柄重开后重发一次无妨）
    bool SendSessionOpen();

    // 主动电量查询（13 4A 01 ...）
    bool SendBatteryQuery();

    // 读取一条输入报告（超时返回 false）
    bool ReadInput(BYTE* buf, DWORD timeout_ms);

    // 解析一条 0x4A 查询响应帧
    static MchoseBatteryReading ParseQueryResponse(const BYTE* frame, int len);

    // 解析一条 0x0A 自发推送帧（返回是否解析到有效电量）
    static bool ParsePushFrame(const BYTE* frame, int len, MchoseBatteryReading& out);

    // 帧校验和：前 19 字节求和 & 0xFF == 第 20 字节
    static bool CheckSum(const BYTE* frame, int len);

    // 构造输出报告（cmd/param -> 20 字节，含校验和）
    static void BuildOutputReport(BYTE* out20, BYTE cmd, BYTE param);

private:
    bool OpenPath(const std::wstring& path);
    bool WriteReport(const BYTE* buf, int len, DWORD timeout_ms);

private:
    HANDLE     m_hDevice = nullptr;
    HANDLE     m_readEvent = nullptr;
    std::wstring m_device_path;
    std::wstring m_product_name;
    USHORT     m_input_length = 0;
    USHORT     m_output_length = 0;
};
