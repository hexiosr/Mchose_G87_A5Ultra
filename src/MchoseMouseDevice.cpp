#include "MchoseMouseDevice.h"
#include "HidApi.h"
#include <setupapi.h>
#include <vector>
#include <cstring>

// ---- A5 Ultra 2.4G 电量通道特征 ----
static constexpr WORD   MCHOSE_VID      = 0x3837; // 14391
static constexpr WORD   A5U_PID         = 0x100C; // 4108
static constexpr USHORT CHANNEL_USAGE_PAGE = 0xFF01;
static constexpr BYTE   FEATURE_RID     = 0x11;   // feature report ID
static constexpr BYTE   CMD_DEVICE_INFO = 0x06;   // "11 06" 设备信息（含电量）

MchoseMouseDevice::MchoseMouseDevice() = default;

MchoseMouseDevice::~MchoseMouseDevice()
{
    Close();
}

bool MchoseMouseDevice::Open(const std::wstring& device_path)
{
    Close();
    if (!device_path.empty())
        return OpenPath(device_path);

    // 枚举 HID 设备接口，找 3837:100C 且 UsagePage=0xFF01
    GUID hid_guid = GUID_DEVINTERFACE_HID;
    HDEVINFO dev_info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
                                             DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE)
        return false;

    bool found = false;
    SP_DEVICE_INTERFACE_DATA if_data;
    if_data.cbSize = sizeof(if_data);
    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(dev_info, nullptr, &hid_guid, index, &if_data); ++index)
    {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, nullptr, 0, &required, nullptr);
        if (required == 0)
            continue;
        std::vector<BYTE> buffer(required);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &if_data, detail, required, nullptr, nullptr))
            continue;
        if (OpenPath(detail->DevicePath))
        {
            found = true;
            break;
        }
    }
    SetupDiDestroyDeviceInfoList(dev_info);
    return found;
}

bool MchoseMouseDevice::OpenPath(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    HIDD_ATTRIBUTES attr{};
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(h, &attr) || attr.VendorID != MCHOSE_VID || attr.ProductID != A5U_PID)
    {
        CloseHandle(h);
        return false;
    }

    PHIDP_PREPARSED_DATA ppd = nullptr;
    if (!HidD_GetPreparsedData(h, &ppd))
    {
        CloseHandle(h);
        return false;
    }
    HIDP_CAPS caps{};
    HidP_GetCaps(ppd, &caps);
    if (caps.UsagePage != CHANNEL_USAGE_PAGE ||
        caps.FeatureReportByteLength < 20)
    {
        HidD_FreePreparsedData(ppd);
        CloseHandle(h);
        return false;
    }

    WCHAR name_buf[128]{};
    if (HidD_GetProductString(h, name_buf, sizeof(name_buf)))
        m_product_name = name_buf;

    HidD_FreePreparsedData(ppd);

    m_hDevice = h;
    m_device_path = path;
    return true;
}

void MchoseMouseDevice::Close()
{
    if (m_hDevice)
    {
        CloseHandle(m_hDevice);
        m_hDevice = nullptr;
    }
    m_device_path.clear();
    m_product_name.clear();
}

// SetFeature 在设备休眠时可能挂起，实测本设备正常立即返回（debug-probe5 OK err=0）
bool MchoseMouseDevice::SetFeatureSafe(const BYTE* buf, int len)
{
    if (!m_hDevice || !buf || len <= 0 || len > 65)
        return false;
    return HidD_SetFeature(m_hDevice, const_cast<BYTE*>(buf), (ULONG)len) != FALSE;
}

bool MchoseMouseDevice::ParseFeatureResponse(const BYTE* resp, int len, MchoseBatteryReading& out)
{
    // resp[0] = Report ID(0x11)，resp[1] = 命令原码(0x06)或反码(0xF9)
    if (!resp || len < 13 || resp[0] != FEATURE_RID)
        return false;
    BYTE marker = resp[1];
    if (marker != CMD_DEVICE_INFO && marker != (BYTE)(CMD_DEVICE_INFO ^ 0xFF))
        return false;
    bool inverted = (marker == (BYTE)(CMD_DEVICE_INFO ^ 0xFF));
    int percent = inverted ? (resp[11] ^ 0xFF) : resp[11];
    int charge  = inverted ? (resp[12] ^ 0xFF) : resp[12];
    // ≤1 一律视为无效读数（设备休眠/链路未就绪时电量字段为 0，
    // 真实电量到不了 1；避免把 0 当有效值显示，保持最后读数）
    if (percent <= 1 || percent > 100)
        return false;
    out.valid = true;
    out.percent = percent;
    out.charging = (charge & 0x10) != 0; // 参照键盘：0x10 位 = 充电中
    return true;
}

bool MchoseMouseDevice::QueryBattery(MchoseBatteryReading& out)
{
    if (!m_hDevice)
        return false;

    // ① SetFeature(11 06) 请求设备信息（21B，反码填充）
    BYTE cmd[65] = {0};
    cmd[0] = FEATURE_RID;
    cmd[1] = CMD_DEVICE_INFO ^ 0xFF;
    for (int i = 0; i < 19; ++i)
        cmd[2 + i] = 0xFF;

    // ② GetFeature 读响应；原码/反码交替，最多重试 3 次
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!SetFeatureSafe(cmd, 21))
            return false;
        Sleep(200);

        BYTE resp[66] = {0};
        resp[0] = FEATURE_RID;
        if (HidD_GetFeature(m_hDevice, resp, sizeof(resp)))
        {
            if (ParseFeatureResponse(resp, (int)sizeof(resp), out))
                return true;
        }
        if (attempt < 2)
            Sleep(100);
    }
    return false;
}
