#include "MchosePlugin.h"
#include <windows.h>
#include <cstdio>

MchosePlugin MchosePlugin::m_instance;

MchosePlugin& MchosePlugin::Instance()
{
    return m_instance;
}

IPluginItem* MchosePlugin::GetItem(int index)
{
    switch (index)
    {
    case 0: return &m_item;    // 外设电量显示（组合键盘 / 鼠标）
    default: return nullptr;
    }
}

void MchosePlugin::DataRequired()
{
    // 首次调用时启动两个后台读取线程（键盘 + 鼠标独立）
    std::call_once(m_key_reader_started, [this] {
        m_key_reader.Start();
    });
    std::call_once(m_mouse_reader_started, [this] {
        m_mouse_reader.Start();
    });

    RefreshDisplay();
}

void MchosePlugin::RefreshDisplay()
{
    std::wstring kb_txt, ms_txt;
    BuildValueText(m_key_reader.GetSnapshot(), kb_txt);
    BuildValueText(m_mouse_reader.GetSnapshot(), ms_txt);

    // 键盘 + 鼠标始终组合显示（离线 / 无读数时该设备显示 --%）
    // 不显示"外设:"标签，任务栏只显示设备电量
    m_label_text = L"";
    m_value_text = L"键盘: " + kb_txt + L" | 鼠标: " + ms_txt;

    std::wstring tip;
    BuildKeyboardTooltip(m_key_reader.GetSnapshot(), tip);
    tip += L"\n\n";
    BuildMouseTooltip(m_mouse_reader.GetSnapshot(), tip);
    m_tooltip = tip;
}

void MchosePlugin::BuildValueText(const BatterySnapshot& snap, std::wstring& out)
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

void MchosePlugin::BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out)
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

void MchosePlugin::BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out)
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

const wchar_t* MchosePlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"迈从设备电量显示";
    case TMI_DESCRIPTION:
        return L"迈从（MCHOSE）G87 键盘 / A5 Ultra 鼠标 2.4G 电量显示";
    case TMI_AUTHOR:
        return L"ziphooo";
    case TMI_COPYRIGHT:
        return L"MIT License";
    case TMI_VERSION:
        return L"1.0.7";
    default:
        break;
    }
    return L"";
}

const wchar_t* MchosePlugin::GetTooltipInfo()
{
    return m_tooltip.c_str();
}

ITMPlugin* TMPluginGetInstance()
{
    return &MchosePlugin::Instance();
}
