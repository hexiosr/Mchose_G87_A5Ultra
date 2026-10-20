#include "MchoseKeyboardItem.h"
#include "MchosePlugin.h"

const wchar_t* MchoseKeyboardItem::GetItemName() const
{
    return L"键盘电量显示";
}

const wchar_t* MchoseKeyboardItem::GetItemId() const
{
    return L"keyboardBattery";
}

const wchar_t* MchoseKeyboardItem::GetItemLableText() const
{
    return L"键盘:";
}

const wchar_t* MchoseKeyboardItem::GetItemValueText() const
{
    return MchosePlugin::Instance().KeyboardValueText().c_str();
}

const wchar_t* MchoseKeyboardItem::GetItemValueSampleText() const
{
    // 给足宽度预算（主程序按此字符串长度计算显示区域宽度），
    // 含充电 ⚡ 标记，避免末尾符号被裁剪
    return L"100%\u26A1";
}

bool MchoseKeyboardItem::IsCustomDraw() const
{
    return false;
}
