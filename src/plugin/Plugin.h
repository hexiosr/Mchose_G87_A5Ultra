#pragma once
// ============================================================
// Plugin.h —— 外设电量监控插件（ITMPlugin 实现）
// 单个组合显示项：外设电量显示（键盘 / 鼠标 / G7 Pro / PS5），
// 插件选项对话框中可勾选显示哪些设备，勾选结果持久化到 ini。
// ============================================================
#include "PluginInterface.h"
#include "PeripheralItem.h"
#include "KeyboardReader.h"
#include "MouseReader.h"
#include "DualSenseReader.h"
#include "G7Reader.h"
#include <mutex>
#include <string>

// 设备条目数量（与 Plugin.cpp 中 kDevices 表一一对应，新增设备时同步 +1）
constexpr int kDeviceCount = 4;

class Plugin : public ITMPlugin
{
public:
    static Plugin& Instance();

    // ITMPlugin
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;

    // 供显示项使用（UI 线程缓存，由 DataRequired 更新）
    const std::wstring& CombinedValueText() const { return m_value_text; }
    const std::wstring& CombinedSampleText() const { return m_sample_text; }

    void SetOptionChanged(bool v) { m_option_changed = v; }
    void ApplyOptionsNow() { RefreshDisplay(); } // 选项确定后立即刷新显示
    void LoadConfig();
    void SyncReaders();              // 勾选才查询：按勾选状态启停各设备 Reader

    // 选项对话框工作副本：勾选/排序只改副本，点"确定"才写回保存生效
    void DlgBegin();                 // 打开对话框时拷贝当前配置
    void DlgApply();                 // 确定：写回配置、保存并刷新显示
    bool DlgShowFor(int dev) const { return dev >= 0 && dev < kDeviceCount ? m_dlg_show[dev] : false; }
    void DlgToggle(int dev) { if (dev >= 0 && dev < kDeviceCount) m_dlg_show[dev] = !m_dlg_show[dev]; }
    int DlgOrderAt(int i) const { return m_dlg_order[i]; }
    void DlgMoveUp(int dev);
    void DlgMoveDown(int dev);
    void DlgRestoreDefault();
    void SaveConfig();

private:
    Plugin() = default;
    Plugin(const Plugin&) = delete;

    void RefreshDisplay();
    void BuildValueText(const BatterySnapshot& snap, std::wstring& out);
    void BuildG7ValueText(const BatterySnapshot& snap, std::wstring& out);
    void BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out);
    void BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out);
    void BuildDualSenseTooltip(const BatterySnapshot& snap, std::wstring& out);
    void BuildG7Tooltip(const BatterySnapshot& snap, std::wstring& out);

private:
    static Plugin m_instance;

    PeripheralItem m_item;
    KeyboardReader m_key_reader;
    MouseReader m_mouse_reader;
    DualSenseReader m_ds_reader;
    G7Reader m_g7_reader;
    bool m_reader_started[kDeviceCount] = {};   // 各设备 Reader 是否已启动（勾选才查询）

    ITrafficMonitor* m_app = nullptr;
    bool m_show[kDeviceCount] = { true, true, true, true }; // 设备勾选状态
    int m_order[kDeviceCount] = { 0, 1, 2, 3 };             // 显示顺序（设备索引）
    bool m_dlg_show[kDeviceCount];                          // 选项对话框工作副本
    int m_dlg_order[kDeviceCount];
    bool m_option_changed = false;

    // UI 线程缓存（由 DataRequired 更新，PeripheralItem::GetItemValueText 读取）
    std::wstring m_value_text = L"键盘: --% | 鼠标: --% | 手柄: 未连接 | PS5: 未连接";
    std::wstring m_sample_text = L"键盘: 100%\u26A1 | 鼠标: 100%\u26A1 | 手柄: \u25AE\u25AE\u25AE\u25AE\u26A1 | PS5: 100%\u26A1";
    std::wstring m_tooltip;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
