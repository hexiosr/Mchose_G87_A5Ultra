#pragma once
// ============================================================
// PeripheralItem.h —— 外设电量显示项（IPluginItem 实现）
// 一个显示项组合键盘 / 鼠标 / G7 Pro / PS5 电量（自绘：⚡ 垂直居中）
// ============================================================
#include "PluginInterface.h"

class PeripheralItem : public IPluginItem
{
public:
    PeripheralItem() = default;

    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual int GetItemWidthEx(void* hDC) const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
};
