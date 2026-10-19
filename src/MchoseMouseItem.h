#pragma once
// ============================================================
// MchoseMouseItem.h —— 鼠标电量显示项（IPluginItem 实现）
// ============================================================
#include "PluginInterface.h"

class MchoseMouseItem : public IPluginItem
{
public:
    MchoseMouseItem() = default;

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
};
