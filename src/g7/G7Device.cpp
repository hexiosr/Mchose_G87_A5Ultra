#include "G7Device.h"

bool G7Device::Open()
{
    if (IsOpen())
        return true;

    m_handle = CreateFileW(
        L"\\\\.\\XboxGIP",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr);

    return IsOpen();
}

void G7Device::Close()
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

bool G7Device::ReadMessage(BYTE* buf, int& len, DWORD timeout_ms)
{
    if (!IsOpen())
        return false;

    OVERLAPPED ov{};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent)
        return false;

    DWORD rd = 0;
    BOOL ok = ReadFile(m_handle, buf, 512, &rd, &ov);
    if (!ok)
    {
        DWORD e = GetLastError();
        if (e == ERROR_IO_PENDING)
        {
            if (WaitForSingleObject(ov.hEvent, timeout_ms) == WAIT_OBJECT_0)
                ok = GetOverlappedResult(m_handle, &ov, &rd, FALSE);
            else
            {
                CancelIo(m_handle);
                CloseHandle(ov.hEvent);
                return false; // 超时：无消息
            }
        }
        else
        {
            CloseHandle(ov.hEvent);
            return false;
        }
    }

    CloseHandle(ov.hEvent);
    if (rd == 0)
        return false;
    len = (int)rd;
    return true;
}
