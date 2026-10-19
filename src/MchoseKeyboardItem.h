#pragma once
// ============================================================
// MchoseKeyboardItem.h —— 键盘电量显示项（IPluginItem 实现）
// ============================================================
#include "PluginInterface.h"

class MchoseKeyboardItem : public IPluginItem
{
public:
    MchoseKeyboardItem() = default;

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
};
