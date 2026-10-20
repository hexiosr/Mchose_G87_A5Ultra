#pragma once
// ============================================================
// MchosePlugin.h —— 迈从键盘 + 鼠标电量监控插件（ITMPlugin 实现）
// 一个 DLL 一个显示项：外设电量显示（组合键盘 / 鼠标）
// ============================================================
#include "PluginInterface.h"
#include "MchosePeripheralItem.h"
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

    // 供显示项使用（UI 线程缓存，由 DataRequired 更新）
    const std::wstring& CombinedLabelText() const { return m_label_text; }
    const std::wstring& CombinedValueText() const { return m_value_text; }
    const std::wstring& CombinedSampleText() const { return m_sample_text; }

private:
    MchosePlugin() = default;
    MchosePlugin(const MchosePlugin&) = delete;

    void RefreshDisplay();
    void BuildValueText(const BatterySnapshot& snap, std::wstring& out);
    void BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out);
    void BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out);

private:
    static MchosePlugin m_instance;

    MchosePeripheralItem m_item;
    MchoseKeyboardReader m_key_reader;
    MchoseMouseReader m_mouse_reader;
    std::once_flag m_key_reader_started;
    std::once_flag m_mouse_reader_started;

    // UI 线程缓存（由 DataRequired 更新，GetItem*Text 读取）
    std::wstring m_label_text = L"";   // 不显示标签
    std::wstring m_value_text = L"键盘: --% | 鼠标: --%";
    std::wstring m_sample_text = L"键盘: 100%\u26A1 | 鼠标: 100%\u26A1";
    std::wstring m_tooltip;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
