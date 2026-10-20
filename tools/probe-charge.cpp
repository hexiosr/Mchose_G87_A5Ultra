// ============================================================
// probe-charge.cpp —— 充电状态诊断工具
// 用法：probe-charge.exe [kb|ms|all]
//   在【未充电】时跑一次，再把设备插上充电线跑一次，
//   对比响应帧的充电字节（bit4）差异即可确定充电标志位。
// ============================================================
#include <windows.h>
#include "HidApi.h"
#include <setupapi.h>
#include <cstdio>
#include <vector>
#include <cstring>

static void DumpHex(const BYTE* b, int n)
{
    for (int i = 0; i < n; ++i)
        printf("%02X ", b[i]);
    printf("\n");
}

// 枚举 HID，打开匹配 VID/PID/UsagePage 的第一个接口
static HANDLE OpenChannel(WORD vid, WORD pid, USHORT usage_page,
                          USHORT* out_len, USHORT* in_len, USHORT* feat_len)
{
    GUID hid_guid = GUID_DEVINTERFACE_HID;
    HDEVINFO dev_info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
                                             DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    HANDLE result = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA ifd;
    ifd.cbSize = sizeof(ifd);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &hid_guid, i, &ifd); ++i)
    {
        DWORD req = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &ifd, nullptr, 0, &req, nullptr);
        if (!req)
            continue;
        std::vector<BYTE> buf(req);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &ifd, detail, req, nullptr, nullptr))
            continue;

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        if (!HidD_GetAttributes(h, &attr) || attr.VendorID != vid || attr.ProductID != pid)
        {
            CloseHandle(h);
            continue;
        }
        PHIDP_PREPARSED_DATA ppd = nullptr;
        if (!HidD_GetPreparsedData(h, &ppd))
        {
            CloseHandle(h);
            continue;
        }
        HIDP_CAPS caps{};
        HidP_GetCaps(ppd, &caps);
        HidD_FreePreparsedData(ppd);
        if (caps.UsagePage != usage_page)
        {
            CloseHandle(h);
            continue;
        }
        if (out_len) *out_len = caps.OutputReportByteLength;
        if (in_len)  *in_len  = caps.InputReportByteLength;
        if (feat_len)*feat_len = caps.FeatureReportByteLength;
        result = h;
        break;
    }
    SetupDiDestroyDeviceInfoList(dev_info);
    return result;
}

static void TestKeyboard()
{
    USHORT out_len = 0, in_len = 0, feat_len = 0;
    HANDLE h = OpenChannel(0x41E4, 0x2001, 0xFF02, &out_len, &in_len, &feat_len);
    if (h == INVALID_HANDLE_VALUE)
    {
        printf("[kb] 未找到 G87 键盘通道（VID_41E4/PID_2001/0xFF02）\n");
        return;
    }
    printf("[kb] opened out=%u in=%u feat=%u\n", out_len, in_len, feat_len);

    // 开会话 13 05 01 00*16 <sum>
    BYTE frame[20] = {0x13, 0x05, 0x01};
    BYTE s = 0;
    for (int i = 0; i < 19; ++i) s = (BYTE)(s + frame[i]);
    frame[19] = s;
    DWORD w = 0;
    WriteFile(h, frame, out_len ? out_len : 20, &w, nullptr);
    Sleep(100);

    // 电量查询 13 4A 01 00*16 <sum>
    BYTE q[20] = {0x13, 0x4A, 0x01};
    s = 0;
    for (int i = 0; i < 19; ++i) s = (BYTE)(s + q[i]);
    q[19] = s;
    WriteFile(h, q, out_len ? out_len : 20, &w, nullptr);

    for (int attempt = 0; attempt < 8; ++attempt)
    {
        BYTE r[64] = {0};
        DWORD rd = 0;
        if (ReadFile(h, r, in_len ? in_len : 20, &rd, nullptr) && rd > 0)
        {
            printf("[kb] resp(%u): ", rd);
            DumpHex(r, (int)rd);
            if (r[0] == 0x13 && r[1] == 0x4A)
            {
                printf("[kb]   percent=%u charge_byte[6]=0x%02X bit4=%d bit0=%d bit1=%d\n",
                       r[5], r[6], (r[6] >> 4) & 1, r[6] & 1, (r[6] >> 1) & 1);
            }
            if (r[0] == 0x13 && r[1] == 0x0A)
            {
                printf("[kb]   push type=%u charge_byte[7]=0x%02X bit4=%d\n",
                       r[5], r[7], (r[7] >> 4) & 1);
            }
        }
        Sleep(50);
    }
    CloseHandle(h);
    printf("[kb] done\n");
}

static void TestMouse()
{
    USHORT out_len = 0, in_len = 0, feat_len = 0;
    HANDLE h = OpenChannel(0x3837, 0x100C, 0xFF01, &out_len, &in_len, &feat_len);
    if (h == INVALID_HANDLE_VALUE)
    {
        printf("[ms] 未找到 A5 Ultra 鼠标通道（VID_3837/PID_100C/0xFF01）\n");
        return;
    }
    printf("[ms] opened out=%u in=%u feat=%u\n", out_len, in_len, feat_len);

    BYTE cmd[65] = {0};
    cmd[0] = 0x11;
    cmd[1] = 0x06 ^ 0xFF;
    for (int i = 0; i < 19; ++i)
        cmd[2 + i] = 0xFF;
    BOOL ok = HidD_SetFeature(h, cmd, 21);
    printf("[ms] SetFeature(11 06) ok=%d\n", ok ? 1 : 0);
    Sleep(200);

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        BYTE resp[66] = {0};
        resp[0] = 0x11;
        if (HidD_GetFeature(h, resp, sizeof(resp)))
        {
            printf("[ms] resp: ");
            DumpHex(resp, 21);
            if (resp[1] == 0x06 || resp[1] == 0xF9)
            {
                bool inv = (resp[1] == 0xF9);
                int pct = inv ? (resp[11] ^ 0xFF) : resp[11];
                int chg = inv ? (resp[12] ^ 0xFF) : resp[12];
                printf("[ms]   percent=%d charge_byte[12]=0x%02X bit4=%d bit0=%d bit1=%d\n",
                       pct, chg, (chg >> 4) & 1, chg & 1, (chg >> 1) & 1);
            }
        }
        Sleep(100);
    }
    CloseHandle(h);
    printf("[ms] done\n");
}

int wmain(int argc, wchar_t** argv)
{
    const wchar_t* which = (argc > 1) ? argv[1] : L"all";
    if (wcscmp(which, L"kb") == 0)
        TestKeyboard();
    else if (wcscmp(which, L"ms") == 0)
        TestMouse();
    else
    {
        TestKeyboard();
        TestMouse();
    }
    return 0;
}
