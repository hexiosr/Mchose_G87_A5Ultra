#pragma once
// ============================================================
// G7Device.h
// 盖世小鸡 G7 Pro 2.4G 手柄接收器（VID_3537/PID_1004，微软 xboxgip 驱动接管）
//
// 关键事实（逆向确认）：
//  - 接收器无 HID 接口、无电池设备、XInput UNKNOWN、WGI 枚举不到、
//    GameInput 枚举到设备但电池字段为 Unknown、D4X 假 100%、
//    \\.\XboxGIP 写通道被驱动拒绝（err=87，只读）
//  - 唯一可读通道：\\.\XboxGIP 只读旁观设备主动上报的 GIP Status 消息
//  - Status 消息（24B）：8 字节设备ID + 04 00 00 00 头 + 12 字节 payload，
//    payload[8] = 电池状态字节：
//      低 2 位(0x03) = 电量档位 0~3（实测满电 = 3）
//      bit4 (0x10)   = 充电标志（0x97 充电中 / 0x87 未充）
//  - 精确百分比是 GameSir 私有查询协议（Nexus 专有），系统层不可获得，
//    本插件提供粗档（0/33/66/100）显示
// ============================================================
#include <windows.h>
#include <string>

class G7Device
{
public:
    G7Device() = default;
    ~G7Device() { Close(); }

    // 打开 GIP 总线设备（只读 + 重叠 I/O，不持有写权限、不影响其他客户端）
    bool Open();
    void Close();
    bool IsOpen() const { return m_handle != INVALID_HANDLE_VALUE; }

    // 阻塞读取一条上游消息（至多 timeout_ms），成功返回 true
    bool ReadMessage(BYTE* buf, int& len, DWORD timeout_ms);

    std::wstring ProductName() const { return L"盖世小鸡 G7 Pro"; }

private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};
