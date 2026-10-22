#include "Plugin.h"
#include <windows.h>
#include <cstdio>

Plugin Plugin::m_instance;

Plugin& Plugin::Instance()
{
    return m_instance;
}

IPluginItem* Plugin::GetItem(int index)
{
    switch (index)
    {
    case 0: return &m_item;    // 外设电量显示（组合键盘 / 鼠标 / G7 Pro / PS5）
    default: return nullptr;
    }
}

void Plugin::DataRequired()
{
    // 首次调用时启动各后台读取线程（键盘 / 鼠标 / DualSense / G7 独立）
    std::call_once(m_key_reader_started, [this] {
        m_key_reader.Start();
    });
    std::call_once(m_mouse_reader_started, [this] {
        m_mouse_reader.Start();
    });
    std::call_once(m_ds_reader_started, [this] {
        m_ds_reader.Start();
    });
    std::call_once(m_g7_reader_started, [this] {
        m_g7_reader.Start();
    });

    RefreshDisplay();
}

void Plugin::RefreshDisplay()
{
    std::wstring kb_txt, ms_txt, g7_txt, ds_txt;
    BuildValueText(m_key_reader.GetSnapshot(), kb_txt);
    BuildValueText(m_mouse_reader.GetSnapshot(), ms_txt);
    BuildG7ValueText(m_g7_reader.GetSnapshot(), g7_txt); // G7 用电池图标（4 档）
    BuildValueText(m_ds_reader.GetSnapshot(), ds_txt);

    // 键盘 / 鼠标 / 手柄 / PS5 始终组合显示（无读数时该设备显示 --%）
    // 不显示"外设:"标签，任务栏只显示设备电量
    m_label_text = L"";
    m_value_text = L"键盘: " + kb_txt +
                   L" | 鼠标: " + ms_txt +
                   L" | 手柄: " + g7_txt +
                   L" | PS5: " + ds_txt;

    std::wstring tip;
    BuildKeyboardTooltip(m_key_reader.GetSnapshot(), tip);
    tip += L"\n\n";
    BuildMouseTooltip(m_mouse_reader.GetSnapshot(), tip);
    tip += L"\n\n";
    BuildG7Tooltip(m_g7_reader.GetSnapshot(), tip);
    tip += L"\n\n";
    BuildDualSenseTooltip(m_ds_reader.GetSnapshot(), tip);
    m_tooltip = tip;
}

// G7 粗档：电池图标（▮ 实心 / ▯ 空心 + 正极头 ▕），4 格表示 4 档
//  ▮▮▮▮▕ = 满   ▮▮▮▯▕ = 高   ▮▮▯▯▕ = 中   ▮▯▯▯▕ = 低   ⚡ 后缀 = 充电中
void Plugin::BuildG7ValueText(const BatterySnapshot& snap, std::wstring& out)
{
    if (!snap.valid || snap.percent < 0)
    {
        out = L"--";
        return;
    }

    int bars = 1; // 0% → 1 格
    if (snap.percent >= 100) bars = 4;
    else if (snap.percent >= 66) bars = 3;
    else if (snap.percent >= 33) bars = 2;

    out = L"";
    for (int i = 0; i < 4; ++i)
        out += (i < bars) ? L"\u25AE" : L"\u25AF"; // ▮ 实心 / ▯ 空心
    out += L"\u2595"; // ▕ 正极头
    if (snap.charging)
        out += L"\u26A1"; // ⚡
}

void Plugin::BuildValueText(const BatterySnapshot& snap, std::wstring& out)
{
    if (!snap.has_device || !snap.online || !snap.valid)
    {
        out = L"--%"; // 未找到设备 / 离线 / 暂无有效读数
        return;
    }

    wchar_t buf[32];
    if (snap.charging)
        swprintf_s(buf, L"%d%%\u26A1", snap.percent); // ⚡ 充电中
    else
        swprintf_s(buf, L"%d%%", snap.percent);
    out = buf;
}

void Plugin::BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"键盘 2.4G 设备电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到键盘接收器（VID_41E4）";
    }
    else if (!snap.online)
    {
        out += L"设备离线/无响应";
    }
    else if (snap.valid)
    {
        wchar_t buf[64];
        swprintf_s(buf, L"电量：%d%%", snap.percent);
        out += buf;
        if (snap.charging)
            out += L"（充电中）";
        if (snap.last_update_ms != 0)
        {
            ULONGLONG now = GetTickCount64();
            ULONGLONG diff = (now >= snap.last_update_ms) ? (now - snap.last_update_ms) : 0;
            swprintf_s(buf, L"\n更新于 %llu 秒前", diff / 1000);
            out += buf;
        }
    }
    else
    {
        out += L"等待设备上报电量…";
    }
}

void Plugin::BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"鼠标 2.4G 设备电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到 A5 Ultra 接收器（VID_3837/PID_100C）";
    }
    else if (!snap.online)
    {
        out += L"设备离线/无响应";
    }
    else if (snap.valid)
    {
        wchar_t buf[64];
        swprintf_s(buf, L"电量：%d%%", snap.percent);
        out += buf;
        if (snap.charging)
            out += L"（充电中）";
        if (snap.last_update_ms != 0)
        {
            ULONGLONG now = GetTickCount64();
            ULONGLONG diff = (now >= snap.last_update_ms) ? (now - snap.last_update_ms) : 0;
            swprintf_s(buf, L"\n更新于 %llu 秒前", diff / 1000);
            out += buf;
        }
    }
    else
    {
        out += L"等待设备上报电量…";
    }
}

void Plugin::BuildG7Tooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"盖世小鸡 G7 Pro 手柄电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到 G7 Pro 接收器（VID_3537/PID_1004）";
    }
    else if (!snap.valid)
    {
        out += L"等待设备上报电量…";
    }
    else
    {
        wchar_t buf[64];
        out += L"电量：";
        int bars = 1;
        if (snap.percent >= 100) bars = 4;
        else if (snap.percent >= 66) bars = 3;
        else if (snap.percent >= 33) bars = 2;
        for (int i = 0; i < 4; ++i)
            out += (i < bars) ? L"\u25AE" : L"\u25AF";
        out += L"\u2595";
        if (snap.charging)
            out += L"（充电中）";
        if (snap.last_update_ms != 0)
        {
            ULONGLONG now = GetTickCount64();
            ULONGLONG diff = (now >= snap.last_update_ms) ? (now - snap.last_update_ms) : 0;
            swprintf_s(buf, L"\n更新于 %llu 秒前", diff / 1000);
            out += buf;
        }
    }
}

void Plugin::BuildDualSenseTooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"PS5 手柄（DualSense）电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到 DualSense（VID_054C，DS5Dongle 桥接）";
    }
    else if (snap.valid)
    {
        wchar_t buf[64];
        swprintf_s(buf, L"电量：%d%%", snap.percent);
        out += buf;
        if (snap.charging)
            out += L"（充电中）";
        if (snap.last_update_ms != 0)
        {
            ULONGLONG now = GetTickCount64();
            ULONGLONG diff = (now >= snap.last_update_ms) ? (now - snap.last_update_ms) : 0;
            swprintf_s(buf, L"\n更新于 %llu 秒前", diff / 1000);
            out += buf;
        }
    }
    else
    {
        out += L"等待设备上报电量…";
    }
}

const wchar_t* Plugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"外设电量显示";
    case TMI_DESCRIPTION:
        return L"外设电量显示";
    case TMI_AUTHOR:
        return L"ziphooo";
    case TMI_COPYRIGHT:
        return L"MIT License";
    case TMI_VERSION:
        return L"0.1.0";
    default:
        break;
    }
    return L"";
}

const wchar_t* Plugin::GetTooltipInfo()
{
    return m_tooltip.c_str();
}

ITMPlugin* TMPluginGetInstance()
{
    return &Plugin::Instance();
}
