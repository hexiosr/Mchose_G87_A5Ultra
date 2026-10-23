#include "KeyboardDevice.h"
#include "HidApi.h"
#include <setupapi.h>
#include <vector>
#include <cstring>

// ---- 协议常量（来自 VigilWatt 文档 §1） ----
static constexpr BYTE REPORT_ID = 0x13;
static constexpr BYTE CMD_SESSION_OPEN = 0x05;   // 参数 0x01 -> 13 05 01 00*16 19
static constexpr BYTE CMD_BATTERY_QUERY = 0x4A;  // 参数 0x01 -> 13 4A 01 00*16 5E
static constexpr int  REPORT_LEN = 20;           // 20 字节帧
static constexpr WORD VENDOR_VID = 0x41E4;       // 迈从 VID

// 电量通道集合特征：UsagePage 0xFF02，输入/输出 20 字节，Report ID 0x13
static constexpr USHORT CHANNEL_USAGE_PAGE = 0xFF02;
static constexpr USHORT CHANNEL_MIN_LEN = 20;

KeyboardDevice::KeyboardDevice()
{
    m_readEvent = CreateEventW(nullptr, TRUE, TRUE, nullptr); // 手动重置、初始有信号
}

KeyboardDevice::~KeyboardDevice()
{
    Close();
    if (m_readEvent)
    {
        CloseHandle(m_readEvent);
        m_readEvent = nullptr;
    }
}

bool KeyboardDevice::Open(const std::wstring& device_path)
{
    Close();

    if (!device_path.empty())
    {
        if (!OpenPath(device_path))
            return false;
        return true;
    }

    // ---- 通过 SetupAPI 枚举 HID 设备接口 ----
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

bool KeyboardDevice::OpenPath(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    // 读取 VID/PID
    HIDD_ATTRIBUTES attr{};
    attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(h, &attr) || attr.VendorID != VENDOR_VID)
    {
        CloseHandle(h);
        return false;
    }

    // 读取 HID caps，确认是 0xFF02 电量通道
    PHIDP_PREPARSED_DATA ppd = nullptr;
    if (!HidD_GetPreparsedData(h, &ppd))
    {
        CloseHandle(h);
        return false;
    }
    HIDP_CAPS caps{};
    HidP_GetCaps(ppd, &caps);
    if (caps.UsagePage != CHANNEL_USAGE_PAGE ||
        caps.OutputReportByteLength < CHANNEL_MIN_LEN ||
        caps.InputReportByteLength < CHANNEL_MIN_LEN ||
        caps.InputReportByteLength > 256) // 输入报告长度须 ≤ Reader 侧帧缓冲（256B）
    {
        HidD_FreePreparsedData(ppd);
        CloseHandle(h);
        return false;
    }

    // 读取产品名（用于提示信息）
    WCHAR name_buf[128]{};
    if (HidD_GetProductString(h, name_buf, sizeof(name_buf)))
        m_product_name = name_buf;

    HidD_FreePreparsedData(ppd);

    // 全部匹配，保留句柄
    m_hDevice = h;
    m_device_path = path;
    m_input_length = caps.InputReportByteLength;
    m_output_length = caps.OutputReportByteLength;
    return true;
}

void KeyboardDevice::Close()
{
    if (m_hDevice)
    {
        CloseHandle(m_hDevice);
        m_hDevice = nullptr;
    }
    // 保留 m_device_path / m_product_name：路径作为下次 Open() 的缓存
    // （接收器未重插时直接按路径重开，免全量枚举），产品名供快照展示
    m_input_length = 0;
    m_output_length = 0;
}

void KeyboardDevice::BuildOutputReport(BYTE* out20, BYTE cmd, BYTE param)
{
    std::memset(out20, 0, REPORT_LEN);
    out20[0] = REPORT_ID;
    out20[1] = cmd;
    out20[2] = param;
    // 校验：前 19 字节求和 & 0xFF
    BYTE sum = 0;
    for (int i = 0; i < REPORT_LEN - 1; ++i)
        sum = static_cast<BYTE>(sum + out20[i]);
    out20[REPORT_LEN - 1] = sum;
}

bool KeyboardDevice::WriteReport(const BYTE* buf, int len, DWORD timeout_ms)
{
    if (!m_hDevice || len <= 0)
        return false;

    OVERLAPPED ov{};
    ov.hEvent = m_readEvent;
    ResetEvent(m_readEvent);

    DWORD written = 0;
    if (!WriteFile(m_hDevice, buf, len, &written, &ov))
    {
        if (GetLastError() != ERROR_IO_PENDING)
            return false;
        DWORD wait = WaitForSingleObject(m_readEvent, timeout_ms);
        if (wait != WAIT_OBJECT_0)
        {
            CancelIo(m_hDevice);
            // 等取消完成再返回：否则内核完成取消时仍会写已销毁栈帧上的 OVERLAPPED
            DWORD canceled = 0;
            GetOverlappedResult(m_hDevice, &ov, &canceled, TRUE);
            return false;
        }
        if (!GetOverlappedResult(m_hDevice, &ov, &written, FALSE))
            return false;
    }
    return written == static_cast<DWORD>(len);
}

bool KeyboardDevice::SendCommand(BYTE cmd, BYTE param)
{
    BYTE frame[REPORT_LEN];
    BuildOutputReport(frame, cmd, param);
    // 写入长度取协议帧与 caps 报告长度的较小者：
    // 报告长度超过 20 时不能越过帧缓冲读栈（防御未来报告加长的接收器）
    int len = (m_output_length > 0 && m_output_length < REPORT_LEN) ? m_output_length : REPORT_LEN;
    return WriteReport(frame, len, 500);
}

bool KeyboardDevice::SendSessionOpen()
{
    return SendCommand(CMD_SESSION_OPEN, 0x01);
}

bool KeyboardDevice::SendBatteryQuery()
{
    return SendCommand(CMD_BATTERY_QUERY, 0x01);
}

bool KeyboardDevice::ReadInput(BYTE* buf, DWORD timeout_ms)
{
    if (!m_hDevice || !buf || m_input_length == 0)
    {
        return false;
    }

    OVERLAPPED ov{};
    ov.hEvent = m_readEvent;
    ResetEvent(m_readEvent);

    DWORD bytes_read = 0;
    if (!ReadFile(m_hDevice, buf, m_input_length, &bytes_read, &ov))
    {
        if (GetLastError() != ERROR_IO_PENDING)
            return false;
        DWORD wait = WaitForSingleObject(m_readEvent, timeout_ms);
        if (wait == WAIT_TIMEOUT)
        {
            CancelIo(m_hDevice);
            // 等取消完成再返回：否则内核完成取消时仍会写已销毁栈帧上的 OVERLAPPED，
            // 且迟到的取消完成信号会误触发共享的 m_readEvent，污染下一次读写
            DWORD canceled = 0;
            GetOverlappedResult(m_hDevice, &ov, &canceled, TRUE);
            return false;
        }
        if (wait != WAIT_OBJECT_0)
            return false;
        if (!GetOverlappedResult(m_hDevice, &ov, &bytes_read, FALSE))
            return false;
    }
    if (bytes_read == 0)
        return false;
    return true;
}

bool KeyboardDevice::CheckSum(const BYTE* frame, int len)
{
    if (!frame || len < REPORT_LEN)
        return false;
    BYTE sum = 0;
    for (int i = 0; i < REPORT_LEN - 1; ++i)
        sum = static_cast<BYTE>(sum + frame[i]);
    return sum == frame[REPORT_LEN - 1];
}

BatteryReading KeyboardDevice::ParseQueryResponse(const BYTE* frame, int len)
{
    BatteryReading r;
    // 帧：[0]=0x13 [1]=0x4A [2]=0x01 [3]=0x00 [4]=0x02 [5]=电量 [6]=充电标志
    if (!frame || len < REPORT_LEN || !CheckSum(frame, len))
        return r;
    if (frame[0] != REPORT_ID || frame[1] != CMD_BATTERY_QUERY)
        return r;
    int percent = frame[5];
    // ≤1 一律视为无效读数（真实电量到不了 1，避免把占位/错误值当电量）
    if (percent <= 1 || percent > 100)
        return r;
    r.valid = true;
    r.percent = percent;
    r.charging = (frame[6] & 0x10) != 0; // 0x10 位 = 充电中
    return r;
}

bool KeyboardDevice::ParsePushFrame(const BYTE* frame, int len, BatteryReading& out)
{
    // 推送：[0]=0x13 [1]=0x0A ... [5]=类型 [6]/[7]=数据
    if (!frame || len < REPORT_LEN || !CheckSum(frame, len))
        return false;
    if (frame[0] != REPORT_ID || frame[1] != 0x0A)
        return false;
    BYTE type = frame[5];
    if (type != 0x05)
        return false; // 0x02 等状态通知仅记录，不作为电量来源
    int percent = frame[6];
    if (percent <= 1 || percent > 100)
        return false;
    out.valid = true;
    out.percent = percent;
    out.charging = (frame[7] & 0x10) != 0; // 推送的充电标志在 [7]
    return true;
}
