#include "Plugin.h"
#include <windows.h>
#include <windowsx.h>
#include <cstdio>

// ============ 设备条目表 ============
// 新增设备：1) 在下面表里加一行（id 递增） 2) 实现对应 Reader 并加入 Plugin 3) kDeviceCount +1
// 选项对话框会自动按此表生成勾选项，无需改界面代码。
struct DeviceEntry {
    int id;              // 稳定 id（ini 持久化 key、Reader 一一对应）
    const wchar_t* name; // 任务栏前缀名
    const wchar_t* desc; // 选项列表里显示的全名
};
const DeviceEntry kDevices[kDeviceCount] = {
    { 0, L"键盘", L"键盘电量（迈从 G87）" },
    { 1, L"鼠标", L"鼠标电量（迈从 A5 Ultra）" },
    { 2, L"手柄", L"G7 Pro 手柄电量（盖世小鸡）" },
    { 3, L"PS5",  L"PS5 手柄电量（DualSense）" },
};

Plugin Plugin::m_instance;

Plugin& Plugin::Instance()
{
    return m_instance;
}

IPluginItem* Plugin::GetItem(int index)
{
    switch (index)
    {
    case 0: return &m_item;    // 外设电量显示（组合键盘 / 鼠标 / G7 Pro / PS5）
    default: return nullptr;
    }
}

void Plugin::DataRequired()
{
    // 勾选才查询：按当前勾选启动各设备的后台读取线程
    SyncReaders();

    RefreshDisplay();
}

void Plugin::RefreshDisplay()
{
    std::wstring kb_txt, ms_txt, g7_txt, ds_txt;
    BuildValueText(m_key_reader.GetSnapshot(), kb_txt);
    BuildValueText(m_mouse_reader.GetSnapshot(), ms_txt);
    BuildG7ValueText(m_g7_reader.GetSnapshot(), g7_txt); // G7 用电池图标（4 档）
    BuildValueText(m_ds_reader.GetSnapshot(), ds_txt);
    if (ds_txt == L"--%")
        ds_txt = L"未连接"; // PS5 断开（PnP 检测）时显示"未连接"

    // 组合显示一块，按选项设置的勾选与顺序过滤设备
    std::wstring val;
    auto append = [&](const std::wstring& seg) {
        if (!val.empty())
            val += L" | ";
        val += seg;
    };
    const std::wstring* txts[kDeviceCount] = { &kb_txt, &ms_txt, &g7_txt, &ds_txt };
    for (int i = 0; i < kDeviceCount; ++i)
    {
        int dev = m_order[i];
        if (m_show[dev])
            append(std::wstring(kDevices[dev].name) + L": " + *txts[dev]);
    }
    if (val.empty())
        val = L"已全部关闭";
    m_value_text = val;

    std::wstring tip;
    for (int i = 0; i < kDeviceCount; ++i)
    {
        int dev = m_order[i];
        if (!m_show[dev])
            continue;
        switch (dev)
        {
        case 0: BuildKeyboardTooltip(m_key_reader.GetSnapshot(), tip); break;
        case 1: BuildMouseTooltip(m_mouse_reader.GetSnapshot(), tip); break;
        case 2: BuildG7Tooltip(m_g7_reader.GetSnapshot(), tip); break;
        case 3: BuildDualSenseTooltip(m_ds_reader.GetSnapshot(), tip); break;
        }
        tip += L"\n\n";
    }
    if (tip.size() >= 2)
        tip.resize(tip.size() - 2);
    m_tooltip = tip;
}

// G7 粗档：电池图标（▮ 实心 / ▯ 空心），4 格表示 4 档
//  ▮▮▮▮ = 满   ▮▮▮▯ = 高   ▮▮▯▯ = 中   ▮▯▯▯ = 低   ⚡ 后缀 = 充电中
void Plugin::BuildG7ValueText(const BatterySnapshot& snap, std::wstring& out)
{
    if (!snap.valid || snap.percent < 0)
    {
        out = L"未连接";
        return;
    }

    int bars = 1; // 0% → 1 格
    if (snap.percent >= 100) bars = 4;
    else if (snap.percent >= 66) bars = 3;
    else if (snap.percent >= 33) bars = 2;

    out = L"";
    for (int i = 0; i < 4; ++i)
        out += (i < bars) ? L"\u25AE" : L"\u25AF"; // ▮ 实心 / ▯ 空心
    if (snap.charging)
        out += L"\u26A1"; // ⚡
}

void Plugin::BuildValueText(const BatterySnapshot& snap, std::wstring& out)
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

void Plugin::BuildKeyboardTooltip(const BatterySnapshot& snap, std::wstring& out)
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

void Plugin::BuildMouseTooltip(const BatterySnapshot& snap, std::wstring& out)
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

void Plugin::BuildG7Tooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"盖世小鸡 G7 Pro 手柄电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到 G7 Pro 接收器（VID_3537/PID_1004）";
    }
    else if (!snap.valid)
    {
        out += L"手柄未连接";
    }
    else
    {
        wchar_t buf[64];
        out += L"电量：";
        int bars = 1;
        if (snap.percent >= 100) bars = 4;
        else if (snap.percent >= 66) bars = 3;
        else if (snap.percent >= 33) bars = 2;
        for (int i = 0; i < 4; ++i)
            out += (i < bars) ? L"\u25AE" : L"\u25AF";
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
}

void Plugin::BuildDualSenseTooltip(const BatterySnapshot& snap, std::wstring& out)
{
    out += L"PS5 手柄（DualSense）电量";
    if (!snap.product.empty())
        out += L"（" + snap.product + L"）";
    out += L"\n";

    if (!snap.has_device)
    {
        out += L"未检测到 DualSense（VID_054C，DS5Dongle 桥接）";
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
        out += L"手柄未连接";
    }
}

const wchar_t* Plugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"外设电量显示";
    case TMI_DESCRIPTION:
        return L"外设电量显示";
    case TMI_AUTHOR:
        return L"ziphooo";
    case TMI_COPYRIGHT:
        return L"MIT License";
    case TMI_VERSION:
        return L"0.1.1";
    default:
        break;
    }
    return L"";
}

const wchar_t* Plugin::GetTooltipInfo()
{
    return m_tooltip.c_str();
}

void Plugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    LoadConfig();
}

// ============ 选项配置（勾选显示哪些设备，存 ini） ============

void Plugin::LoadConfig()
{
    if (!m_app)
        return;
    std::wstring path = m_app->GetPluginConfigDir();
    path += L"\\PeripheralBattery.ini";

    // 勾选：show{id}=0/1（按设备 id 持久化，新增设备自动多一个 key）
    wchar_t buf[16];
    for (int d = 0; d < kDeviceCount; ++d)
    {
        wchar_t key[16];
        swprintf_s(key, L"show%d", d);
        m_show[d] = true;
        if (GetPrivateProfileStringW(L"Options", key, L"1", buf, 16, path.c_str()) > 0)
            m_show[d] = (buf[0] == L'1');
    }

    // 显示顺序：order=0,2,1,3（设备索引，逗号分隔）
    wchar_t ord[64];
    for (int i = 0; i < kDeviceCount; ++i)
        m_order[i] = i;
    if (GetPrivateProfileStringW(L"Options", L"order", L"", ord, 64, path.c_str()) > 0)
    {
        int idx = 0;
        wchar_t* ctx = nullptr;
        wchar_t* tok = wcstok_s(ord, L",", &ctx);
        while (tok && idx < kDeviceCount)
        {
            int dev = _wtoi(tok);
            if (dev >= 0 && dev < kDeviceCount)
                m_order[idx++] = dev;
            tok = wcstok_s(nullptr, L",", &ctx);
        }
        // 去重补全（保证每个设备恰好出现一次）
        bool seen[kDeviceCount] = {};
        int cnt = 0;
        for (int i = 0; i < kDeviceCount; ++i)
            if (!seen[m_order[i]])
            {
                seen[m_order[i]] = true;
                m_order[cnt++] = m_order[i];
            }
        for (int d = 0; d < kDeviceCount; ++d)
            if (!seen[d])
                m_order[cnt++] = d;
    }
}

void Plugin::SaveConfig()
{
    if (!m_app)
        return;
    std::wstring path = m_app->GetPluginConfigDir();
    path += L"\\PeripheralBattery.ini";
    for (int d = 0; d < kDeviceCount; ++d)
    {
        wchar_t key[16];
        swprintf_s(key, L"show%d", d);
        WritePrivateProfileStringW(L"Options", key, m_show[d] ? L"1" : L"0", path.c_str());
    }
    wchar_t ord[64];
    swprintf_s(ord, L"%d", m_order[0]);
    for (int i = 1; i < kDeviceCount; ++i)
    {
        wchar_t seg[16];
        swprintf_s(seg, L",%d", m_order[i]);
        wcscat_s(ord, seg);
    }
    WritePrivateProfileStringW(L"Options", L"order", ord, path.c_str());
}

// ============ 选项对话框工作副本 ============

void Plugin::DlgBegin()
{
    memcpy(m_dlg_show, m_show, sizeof(m_show));
    memcpy(m_dlg_order, m_order, sizeof(m_order));
}

void Plugin::DlgApply()
{
    memcpy(m_show, m_dlg_show, sizeof(m_show));
    memcpy(m_order, m_dlg_order, sizeof(m_order));
    SaveConfig();
    ApplyOptionsNow();
    SyncReaders(); // 勾选才查询：按最新勾选启动/停止设备 Reader
}

void Plugin::SyncReaders()
{
    for (int d = 0; d < kDeviceCount; ++d)
    {
        bool want = m_show[d];
        bool running = m_reader_started[d];
        if (want && !running)
        {
            switch (d)
            {
            case 0: m_key_reader.Start(); break;
            case 1: m_mouse_reader.Start(); break;
            case 2: m_g7_reader.Start(); break;
            case 3: m_ds_reader.Start(); break;
            }
            m_reader_started[d] = true;
        }
        else if (!want && running)
        {
            switch (d)
            {
            case 0: m_key_reader.Stop(); break;
            case 1: m_mouse_reader.Stop(); break;
            case 2: m_g7_reader.Stop(); break;
            case 3: m_ds_reader.Stop(); break;
            }
            m_reader_started[d] = false;
        }
    }
}

void Plugin::DlgMoveUp(int dev)
{
    for (int i = 0; i < kDeviceCount; ++i)
    {
        if (m_dlg_order[i] == dev && i > 0)
        {
            std::swap(m_dlg_order[i], m_dlg_order[i - 1]);
            return;
        }
    }
}

void Plugin::DlgMoveDown(int dev)
{
    for (int i = 0; i < kDeviceCount; ++i)
    {
        if (m_dlg_order[i] == dev && i < kDeviceCount - 1)
        {
            std::swap(m_dlg_order[i], m_dlg_order[i + 1]);
            return;
        }
    }
}

void Plugin::DlgRestoreDefault()
{
    for (int d = 0; d < kDeviceCount; ++d)
    {
        m_dlg_show[d] = true;
        m_dlg_order[d] = d;
    }
}

// ============ 选项设置对话框（仿 TrafficMonitor 显示设置样式） ============
// 左侧：设备复选列表（自绘，点击左侧框切换勾选、点击行选中）
// 右侧：上移 / 下移 / 恢复默认
// 底部：确定 / 取消

namespace {

constexpr UINT IDC_LIST = 500;
constexpr UINT IDC_UP = 600;
constexpr UINT IDC_DOWN = 601;
constexpr UINT IDC_RESET = 602;
constexpr UINT IDC_OK = 700;
constexpr UINT IDC_CANCEL = 701;
constexpr UINT IDC_APPLY = 702;
constexpr int kRowHeight = 36;

WNDPROC g_origListProc = nullptr; // ListBox 原窗口过程（子类化用）

// 按当前顺序重建列表内容（自绘行数据 = 设备 id）
static void RebuildList(HWND hList)
{
    Plugin* self = (Plugin*)GetWindowLongPtrW(GetParent(hList), GWLP_USERDATA);
    if (!self)
        return;
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (int row = 0; row < kDeviceCount; ++row)
    {
        int dev = self->DlgOrderAt(row);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)kDevices[dev].desc);
        SendMessageW(hList, LB_SETITEMDATA, row, dev);
    }
    SendMessageW(hList, LB_SETCURSEL, 0, 0); // 默认选中第一行
}

// 选中指定设备所在行
static void SelectDev(HWND hList, int dev)
{
    for (int row = 0; row < kDeviceCount; ++row)
        if ((int)SendMessageW(hList, LB_GETITEMDATA, row, 0) == dev)
        {
            SendMessageW(hList, LB_SETCURSEL, row, 0);
            break;
        }
}

// ListBox 子类化：点击左侧复选框区域切换勾选（不改选中），否则走默认选中逻辑
LRESULT CALLBACK ListProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_LBUTTONDOWN)
    {
        Plugin* self = (Plugin*)GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA);
        int rowH = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, 0, 0);
        if (rowH <= 0)
            rowH = kRowHeight;
        int row = (int)(GET_Y_LPARAM(lp) / rowH);
        if (row >= 0 && row < kDeviceCount)
        {
            int x = GET_X_LPARAM(lp);
            if (x < 30) // 复选框区域（8..26 + 边距）
            {
                int dev = (int)SendMessageW(hwnd, LB_GETITEMDATA, row, 0);
                if (self)
                {
                    self->DlgToggle(dev);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }
        }
    }
    return CallWindowProcW(g_origListProc, hwnd, msg, wp, lp);
}

LRESULT CALLBACK OptionDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Plugin* self = (Plugin*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_ERASEBKGND:
        // 原型：对话框整体浅灰背景（#f0f0f0），列表为白色内容卡片
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, (HBRUSH)GetSysColorBrush(COLOR_BTNFACE));
            return 1;
        }
    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            if (dis->CtlID != IDC_LIST || dis->itemID == (UINT)-1)
                break;
            int dev = (int)dis->itemData;
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool sel = (dis->itemState & ODS_SELECTED) != 0;

            // 行背景：选中高亮 / 白色
            FillRect(hdc, &rc, (HBRUSH)GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));

            // 复选框（18×18，仿原型样式：未勾选白底灰边、勾选蓝底白勾）
            int bh = rc.bottom - rc.top;
            int cs = 18;
            bool checked = self && self->DlgShowFor(dev);
            RECT box{ rc.left + 8, rc.top + (bh - cs) / 2, rc.left + 8 + cs, rc.top + (bh - cs) / 2 + cs };

            COLORREF fillCol = checked ? RGB(0, 120, 215) : RGB(255, 255, 255);
            COLORREF brdCol = sel ? RGB(255, 255, 255)
                                  : (checked ? RGB(0, 120, 215) : RGB(106, 106, 106));
            HBRUSH hbr = CreateSolidBrush(fillCol);
            FillRect(hdc, &box, hbr);
            DeleteObject(hbr);
            HPEN hpen = CreatePen(PS_SOLID, 1, brdCol);
            HPEN oldPen = (HPEN)SelectObject(hdc, hpen);
            HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, box.left, box.top, box.right, box.bottom);
            SelectObject(hdc, oldBr);
            if (checked)
            {
                // 白勾：两条线段（2px）
                HPEN hook = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                SelectObject(hdc, hook);
                MoveToEx(hdc, box.left + 4, box.top + 8, nullptr);
                LineTo(hdc, box.left + 8, box.top + 12);
                LineTo(hdc, box.left + 13, box.top + 6);
                DeleteObject(SelectObject(hdc, hook));
            }
            SelectObject(hdc, oldPen);
            DeleteObject(hpen);

            // 文本（缩进：padding 8 + 复选框 18 + 间距 6）
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, sel ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_WINDOWTEXT));
            RECT rcTxt = rc;
            rcTxt.left += 32;
            DrawTextW(hdc, kDevices[dev].desc, -1, &rcTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return 0;
        }
    case WM_COMMAND:
        {
            UINT id = LOWORD(wp);
            HWND hList = GetDlgItem(hwnd, IDC_LIST);
            if (id == IDC_UP || id == IDC_DOWN)
            {
                int cur = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
                if (cur != LB_ERR)
                {
                    int dev = (int)SendMessageW(hList, LB_GETITEMDATA, cur, 0);
                    if (id == IDC_UP) self->DlgMoveUp(dev);
                    else self->DlgMoveDown(dev);
                    RebuildList(hList);
                    SelectDev(hList, dev);
                    SetFocus(hList);
                }
                return 0;
            }
            if (id == IDC_RESET)
            {
                if (self)
                    self->DlgRestoreDefault();
                RebuildList(hList);
                SetFocus(hList);
                return 0;
            }
            switch (id)
            {
            case IDC_APPLY:
                // 立即保存并刷新显示，对话框保持打开（方便预览效果）
                if (self)
                {
                    self->DlgApply();
                    self->SetOptionChanged(true);
                }
                return 0;
            case IDC_OK:
                if (self)
                {
                    self->DlgApply(); // 写回配置、保存并立即刷新显示
                    self->SetOptionChanged(true);
                }
                {
                    HWND p = GetParent(hwnd);
                    if (p)
                    {
                        EnableWindow(p, TRUE);
                        SetActiveWindow(p);
                        SetForegroundWindow(p);
                    }
                }
                DestroyWindow(hwnd);
                return 0;
            case IDC_CANCEL:
                {
                    HWND p = GetParent(hwnd);
                    if (p)
                    {
                        EnableWindow(p, TRUE);
                        SetActiveWindow(p);
                        SetForegroundWindow(p);
                    }
                }
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;
    case WM_CLOSE:
        {
            HWND p = GetParent(hwnd);
            if (p)
            {
                EnableWindow(p, TRUE);
                SetActiveWindow(p);
                SetForegroundWindow(p);
            }
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

ITMPlugin::OptionReturn Plugin::ShowOptionsDialog(void* hParent)
{
    HWND parent = (HWND)hParent;
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    // 窗口尺寸（Win32 边框会吃掉客户区，留足余量避免右侧按钮被裁）
    const int W = 520, H = 384;

    // 创建系统对话框窗口（#32770），再替换窗口过程接管消息
    HWND hDlg = CreateWindowExW(0, L"#32770", L"显示设置",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
                                CW_USEDEFAULT, CW_USEDEFAULT, W, H,
                                parent, nullptr, hInst, nullptr);
    if (!hDlg)
        return OR_OPTION_UNCHANGED;
    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)OptionDlgProc);
    SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)this);

    // 工作副本：对话框期间所有勾选/排序只改副本，确定才写回
    DlgBegin();

    // 系统 UI 字体（Segoe UI / 微软雅黑），对话框与所有控件统一设置
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);
    SendMessageW(hDlg, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 列表字体：比系统字体大两档（18px），选项文字更醒目
    LOGFONTW lfList = ncm.lfMessageFont;
    lfList.lfHeight = -18;
    HFONT hListFont = CreateFontIndirectW(&lfList);

    auto mk = [&](UINT id, const wchar_t* text, int x, int y, int w, int h, DWORD style) {
        HWND ctrl = CreateWindowExW(0, L"BUTTON", text,
                                    WS_CHILD | WS_VISIBLE | style,
                                    x, y, w, h, hDlg, (HMENU)(UINT_PTR)id, hInst, nullptr);
        if (ctrl)
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFont, TRUE);
    };

    // 左侧设备列表（原型：白底灰边、高 264、行高 24、复选框 + 文本）
    // 点击左侧复选框切换勾选，点击行选中（支持上移/下移）
    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                     LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY,
                                 11, 6, 355, 264,
                                 hDlg, (HMENU)IDC_LIST, hInst, nullptr);
    SendMessageW(hList, WM_SETFONT, (WPARAM)hListFont, TRUE);
    SendMessageW(hList, LB_SETITEMHEIGHT, 0, kRowHeight);
    g_origListProc = (WNDPROC)SetWindowLongPtrW(hList, GWLP_WNDPROC, (LONG_PTR)ListProc);
    RebuildList(hList);

    // 右侧按钮列（原型：宽 80、三按钮纵向紧排 gap 7、与列表顶部对齐）
    mk(IDC_UP, L"上移", 377, 6, 110, 34, BS_PUSHBUTTON);
    mk(IDC_DOWN, L"下移", 377, 47, 110, 34, BS_PUSHBUTTON);
    mk(IDC_RESET, L"恢复默认", 377, 88, 110, 34, BS_PUSHBUTTON);

    // 底部：确定 / 取消（原型：右对齐、gap 7、88×26）
    mk(IDC_OK, L"确定", 149, 281, 110, 34, BS_DEFPUSHBUTTON);
    mk(IDC_APPLY, L"应用", 266, 281, 110, 34, BS_PUSHBUTTON);
    mk(IDC_CANCEL, L"取消", 383, 281, 110, 34, BS_PUSHBUTTON);

    m_option_changed = false;
    if (parent)
        EnableWindow(parent, FALSE);

    // 屏幕工作区居中（不依赖父窗口位置，避免对话框被摆到屏幕外）
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.left + ((wa.right - wa.left) - W) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - H) / 2;
    SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    UpdateWindow(hDlg);

    // 模态消息循环：取本线程所有消息（子控件消息也在此队列），
    // 子控件导航交给 IsDialogMessage（Tab 切换、回车确认、Esc 取消），对话框销毁即退出。
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(hDlg))
            break;
    }

    // 取消 / 叉掉：丢弃工作副本，不写回配置（任务栏不受影响）

    if (parent)
    {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent); // 关闭后把前台还给父窗口，避免焦点跳到其他应用
    }
    DeleteObject(hFont);
    DeleteObject(hListFont);
    return m_option_changed ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

ITMPlugin* TMPluginGetInstance()
{
    return &Plugin::Instance();
}
