#include "MchoseMouseItem.h"
#include "MchosePlugin.h"

const wchar_t* MchoseMouseItem::GetItemName() const
{
    return L"鼠标电量显示";
}

const wchar_t* MchoseMouseItem::GetItemId() const
{
    return L"mouseBattery";
}

const wchar_t* MchoseMouseItem::GetItemLableText() const
{
    return L"鼠标:";
}

const wchar_t* MchoseMouseItem::GetItemValueText() const
{
    return MchosePlugin::Instance().MouseValueText().c_str();
}

const wchar_t* MchoseMouseItem::GetItemValueSampleText() const
{
    // 给足宽度预算（主程序按此字符串长度计算显示区域宽度），
    // 避免末尾的 "%" 因空间不足被裁剪
    return L"100%";
}

bool MchoseMouseItem::IsCustomDraw() const
{
    return false;
}
