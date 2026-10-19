#pragma once
// ============================================================
// MchosePlugin.h —— 迈从键盘 + 鼠标电量监控插件（ITMPlugin 实现）
// 一个 DLL 提供两个显示项：键盘电量（index 0）+ 鼠标电量（index 1）
// ============================================================
#include "PluginInterface.h"
#include "MchoseKeyboardItem.h"
#include "MchoseMouseItem.h"
#include "MchoseKeyboardReader.h"
#include "MchoseMouseReader.h"
#include <mutex>
#include <string>

class MchosePlugin : public ITMPlugin
{
public:
    static MchosePlugin& Instance();

    // ITMPlugin（仅必需接口，其余用基类默认实现）
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;

    // 供显示项使用
    const std::wstring& KeyboardValueText() const { return m_key_value_text; }
    const std::wstring& MouseValueText() const { return m_mouse_value_text; }

private:
    MchosePlugin() = default;
    MchosePlugin(const MchosePlugin&) = delete;

    void RefreshDisplay();
    void BuildValueText(const BatterySnapshot& snap, std::wstring& out);
    void BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out);
    void BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out);

private:
    static MchosePlugin m_instance;

    MchoseKeyboardItem m_key_item;
    MchoseMouseItem m_mouse_item;
    MchoseKeyboardReader m_key_reader;
    MchoseMouseReader m_mouse_reader;
    std::once_flag m_key_reader_started;
    std::once_flag m_mouse_reader_started;

    // UI 线程缓存（由 DataRequired 更新，GetItemValueText 读取）
    std::wstring m_key_value_text = L"--%";
    std::wstring m_mouse_value_text = L"--%";
    std::wstring m_tooltip;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
