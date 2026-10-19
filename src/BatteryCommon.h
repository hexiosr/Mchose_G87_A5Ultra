#pragma once
// ============================================================
// BatteryCommon.h —— 键盘 / 鼠标两个设备共用的数据结构
// ============================================================
#include <windows.h>
#include <string>

// 单次电量读数（键盘 0x13 协议与鼠标 0x11 feature 协议共用）
struct MchoseBatteryReading
{
    bool   valid = false;    // 是否有有效读数
    int    percent = -1;     // 电量百分比 0~100
    bool   charging = false; // 是否充电中
};

// 供 UI 侧读取的快照（每个 Reader 各自维护一份）
struct BatterySnapshot
{
    bool   has_device = false;   // 是否找到并打开过设备
    bool   online = false;       // 最近查询是否成功
    bool   valid = false;        // 是否有有效读数
    int    percent = -1;         // 电量百分比
    bool   charging = false;     // 充电中
    ULONGLONG last_update_ms = 0;// 最近一次成功读数时间（GetTickCount64）
    std::wstring product;        // 产品名
    std::wstring device_path;    // 设备路径（重连用）
};
