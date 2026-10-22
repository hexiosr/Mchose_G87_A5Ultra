#include "DualSenseDevice.h"
#include "HidApi.h"
#include <setupapi.h>
#include <vector>
#include <cstring>

// ---- DualSense 特征 ----
static constexpr WORD   SONY_VID       = 0x054C;
static constexpr WORD   DS_PID         = 0x0CE6;  // DualSense
static constexpr WORD   EDGE_PID       = 0x0DF2;  // DualSense Edge
static constexpr BYTE   INPUT_RID      = 0x01;    // Input Report 0x01（USB 状态报告）
// 电池字节在报告 byte[53]（0-based）：
//   byte[0]=ReportID，USBGetStateData[52]（PowerPercent 低4位 + PowerState 高4位）
//   对应报告偏移 = 1 + 52 = 53（DS5Dongle 固件 utils.h USBGetStateData 布局确认）
static constexpr int    POWER_BYTE     = 53;
static constexpr BYTE   POWERSTATE_CHARGING = 0x01;
static constexpr BYTE   POWERSTATE_COMPLETE  = 0x02;

DualSenseDevice::DualSenseDevice() = default;

DualSenseDevice::~DualSenseDevice()
{
    Close();
}

bool DualSenseDevice::Open(const std::wstring& device_path)
{
    Close();
    if (!device_path.empty())
        return OpenPath(device_path);

    // 枚举 HID 设备接口，匹配 Sony VID + DualSense/Edge PID
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

bool DualSenseDevice::IsPresent()
{
    // 枚举 HID 设备接口，打开后按 VID/PID 匹配（与探针同款，已验证可靠）。
    // 仅做 CreateFile + HidD_GetAttributes + CloseHandle，不读写数据，无副作用。
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

        HANDLE h = CreateFileW(detail->DevicePath,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        if (HidD_GetAttributes(h, &attr) &&
            attr.VendorID == SONY_VID &&
            (attr.ProductID == DS_PID || attr.ProductID == EDGE_PID))
        {
            found = true;
            CloseHandle(h);
            break;
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(dev_info);
    return found;
}

bool DualSenseDevice::OpenPath(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    HIDD_ATTRIBUTES attr{};
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(h, &attr) || attr.VendorID != SONY_VID ||
        (attr.ProductID != DS_PID && attr.ProductID != EDGE_PID))
    {
        CloseHandle(h);
        return false;
    }

    WCHAR name_buf[128]{};
    if (HidD_GetProductString(h, name_buf, sizeof(name_buf)))
        m_product_name = name_buf;

    m_hDevice = h;
    m_device_path = path;
    return true;
}

void DualSenseDevice::Close()
{
    if (m_hDevice)
    {
        CloseHandle(m_hDevice);
        m_hDevice = nullptr;
    }
    m_device_path.clear();
    m_product_name.clear();
}

bool DualSenseDevice::ParseInputReport(const BYTE* buf, int len, BatteryReading& out)
{
    // 报告 0x01：64 字节（含 Report ID）
    if (!buf || len < POWER_BYTE + 1 || buf[0] != INPUT_RID)
        return false;

    // 空壳报告防护（对照 Linux hid-playstation.c 研究）：
    // 连接/断开瞬间会收到报告体为空的 0x01（下标 10 起全 0），
    // 电量下标 53 上是填充 0x00，照读会误报"0 档"。
    bool body_empty = true;
    for (int i = 10; i < len; ++i)
    {
        if (buf[i] != 0) { body_empty = false; break; }
    }
    if (body_empty)
        return false;

    BYTE power_byte = buf[POWER_BYTE];
    int  level      = power_byte & 0x0F;       // 0x00-0x0A → 0~10 档
    BYTE state      = (power_byte >> 4) & 0x0F;

    if (level > 10)
        return false; // 无效读数

    out.valid = true;
    // 与 Linux 驱动 dualsense_parse_report 一致：
    // battery = min(level * 10 + 5, 100)；0→5%, 5→55%, 9→95%, 10→100%
    if (level >= 10)
        out.percent = 100;
    else
        out.percent = level * 10 + 5;

    if (state == POWERSTATE_CHARGING)
        out.charging = true;                       // 0x01 充电中
    else if (state == POWERSTATE_COMPLETE)
    {
        out.charging = true;                       // 0x02 已充满 → 100%
        out.percent = 100;
    }
    else
        out.charging = false;                      // 0x00 放电等
    return true;
}

bool DualSenseDevice::QueryBattery(BatteryReading& out)
{
    if (!m_hDevice)
        return false;

    // 纯被动同步读取 Input Report 0x01：DS5Dongle 在手柄连接期间持续上报，
    // ReadFile 立即返回（最初 60s 被动版同款方式，实测可用）。
    // 连接状态由调用方通过 IsPresent()（PnP 检测）保证，断开时不会调用到这里。
    BYTE buf[65] = { 0 };
    DWORD read = 0;
    if (!ReadFile(m_hDevice, buf, sizeof(buf), &read, nullptr))
        return false;
    if (read < POWER_BYTE + 1)
        return false;
    return ParseInputReport(buf, (int)read, out);
}
