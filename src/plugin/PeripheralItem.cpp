#include "PeripheralItem.h"
#include "Plugin.h"
#include <windows.h>
#include <vector>

const wchar_t* PeripheralItem::GetItemName() const
{
    return L"外设电量显示";
}

const wchar_t* PeripheralItem::GetItemId() const
{
    return L"peripheralBattery";
}

const wchar_t* PeripheralItem::GetItemLableText() const
{
    return Plugin::Instance().CombinedLabelText().c_str();
}

const wchar_t* PeripheralItem::GetItemValueText() const
{
    return Plugin::Instance().CombinedValueText().c_str();
}

const wchar_t* PeripheralItem::GetItemValueSampleText() const
{
    // 给足宽度预算（按全部设备组合的最宽情况），避免末尾符号被裁剪
    return Plugin::Instance().CombinedSampleText().c_str();
}

bool PeripheralItem::IsCustomDraw() const
{
    return true; // 自绘：⚡ 符号在系统字体中基线偏低，需单独上移实现视觉居中
}

// 完整显示文本（label + value；label 为空时只显示 value）
static std::wstring FullText()
{
    const std::wstring& label = Plugin::Instance().CombinedLabelText();
    const std::wstring& value = Plugin::Instance().CombinedValueText();
    if (label.empty())
        return value;
    return label + L" " + value;
}

int PeripheralItem::GetItemWidth() const
{
    // 96 DPI 基准宽度（主程序会按系统 DPI 放大）
    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
        return 0;
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ old = SelectObject(hdc, font);
    std::wstring txt = FullText();
    SIZE sz{};
    GetTextExtentPoint32W(hdc, txt.c_str(), (int)txt.size(), &sz);
    SelectObject(hdc, old);
    DeleteDC(hdc);
    return sz.cx + 6;
}

int PeripheralItem::GetItemWidthEx(void* hDC) const
{
    // 实际 hDC 下的精确宽度（主程序不缩放）
    std::wstring txt = FullText();
    SIZE sz{};
    GetTextExtentPoint32W((HDC)hDC, txt.c_str(), (int)txt.size(), &sz);
    return sz.cx + 6;
}

void PeripheralItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    HDC dc = (HDC)hDC;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, dark_mode ? RGB(225, 225, 225) : RGB(30, 30, 30));

    TEXTMETRIC tm{};
    GetTextMetrics(dc, &tm);

    std::wstring text = FullText();

    // 按 ⚡ (U+26A1) 分割，普通段正常绘制，⚡ 段上移使其视觉居中
    const wchar_t kLightning = 0x26A1;
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t ch : text)
    {
        if (ch == kLightning)
        {
            parts.push_back(cur);
            parts.push_back(std::wstring(1, kLightning));
            cur.clear();
        }
        else
        {
            cur += ch;
        }
    }
    parts.push_back(cur);

    // 垂直居中：文本按行高居中，基线上移使视觉重心对齐
    int top = y + (h - tm.tmHeight) / 2;
    int lightning_dy = tm.tmHeight / 6; // ⚡ 相对普通文本上移量（微调此值可调整位置）

    SetTextAlign(dc, TA_LEFT | TA_TOP);
    int cx = x;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const std::wstring& part = parts[i];
        if (part.empty())
            continue;
        bool is_lightning = (i % 2 == 1) && (part == std::wstring(1, kLightning));
        int py = is_lightning ? (top - lightning_dy) : top;
        TextOutW(dc, cx, py, part.c_str(), (int)part.size());
        SIZE sz{};
        GetTextExtentPoint32W(dc, part.c_str(), (int)part.size(), &sz);
        cx += sz.cx;
    }
}
