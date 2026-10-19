#pragma once
// ============================================================
// HidApi.h
// 手写 HID API 声明（hidsdi.h / hidpi.h 的替代）
// 原因：本机（及部分环境）的 Windows SDK 头文件包中不含 hidsdi.h / hidpi.h。
// 这些类型与函数均为公开且稳定的 ABI，按微软官方头文件定义手写。
// 链接时使用从 System32\hid.dll 生成的导入库（见 tools\gen-libs.ps1）。
// ============================================================
#include <windows.h>

// USAGE 类型（hidpi.h 中定义：typedef USHORT USAGE;）
#ifndef USAGE
typedef USHORT USAGE;
#endif

// GUID_DEVINTERFACE_HID（hidsdi.h 中定义）
#ifndef GUID_DEVINTERFACE_HID
#define GUID_DEVINTERFACE_HID \
    {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}}
#endif

// HIDD_ATTRIBUTES
typedef struct _HIDD_ATTRIBUTES {
    ULONG  Size;
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

// HIDP_PREPARSED_DATA（不透明）
typedef struct _HIDP_PREPARSED_DATA HIDP_PREPARSED_DATA, *PHIDP_PREPARSED_DATA;

// HIDP_REPORT_TYPE
typedef enum _HIDP_REPORT_TYPE {
    HidP_Input = 0,
    HidP_Output = 1,
    HidP_Feature = 2
} HIDP_REPORT_TYPE;

// HIDP_CAPS
typedef struct _HIDP_CAPS {
    USHORT Usage;
    USHORT UsagePage;
    USHORT InputReportByteLength;
    USHORT OutputReportByteLength;
    USHORT FeatureReportByteLength;
    USHORT Reserved[17];
    USHORT NumberLinkCollectionNodes;
    USHORT NumberInputButtonCaps;
    USHORT NumberInputValueCaps;
    USHORT NumberInputDataIndices;
    USHORT NumberOutputButtonCaps;
    USHORT NumberOutputValueCaps;
    USHORT NumberOutputDataIndices;
    USHORT NumberFeatureButtonCaps;
    USHORT NumberFeatureValueCaps;
    USHORT NumberFeatureDataIndices;
} HIDP_CAPS, *PHIDP_CAPS;

// HIDP_VALUE_CAPS
typedef struct _HIDP_VALUE_CAPS {
    USAGE   UsagePage;
    UCHAR   ReportID;
    BOOLEAN IsAlias;
    USHORT  BitField;
    USHORT  LinkCollection;
    USAGE   LinkUsage;
    USAGE   LinkUsagePage;
    BOOLEAN IsRange;
    BOOLEAN IsStringRange;
    BOOLEAN IsDesignatorRange;
    BOOLEAN IsAbsolute;
    BOOLEAN HasNull;
    USHORT  Reserved;
    union {
        struct {
            USHORT UsageMin;
            USHORT UsageMax;
            USHORT StringMin;
            USHORT StringMax;
            USHORT DesignatorMin;
            USHORT DesignatorMax;
            USHORT DataIndexMin;
            USHORT DataIndexMax;
        } Range;
        struct {
            USAGE  Usage;
            USHORT Reserved1;
            USHORT StringIndex;
            USHORT Reserved2;
            USHORT DesignatorIndex;
            USHORT Reserved3;
            USHORT DataIndex;
            USHORT Reserved4;
        } NotRange;
    } u;
} HIDP_VALUE_CAPS, *PHIDP_VALUE_CAPS;

// HIDP_BUTTON_CAPS
typedef struct _HIDP_BUTTON_CAPS {
    USAGE   UsagePage;
    UCHAR   ReportID;
    BOOLEAN IsAlias;
    USHORT  BitField;
    USHORT  LinkCollection;
    USAGE   LinkUsage;
    USAGE   LinkUsagePage;
    BOOLEAN IsRange;
    BOOLEAN IsStringRange;
    BOOLEAN IsDesignatorRange;
    BOOLEAN IsAbsolute;
    BOOLEAN HasNull;
    USHORT  Reserved;
    union {
        struct {
            USAGE  UsageMin;
            USAGE  UsageMax;
            USHORT StringMin;
            USHORT StringMax;
            USHORT DesignatorMin;
            USHORT DesignatorMax;
            USHORT DataIndexMin;
            USHORT DataIndexMax;
        } Range;
        struct {
            USAGE  Usage;
            USHORT Reserved1;
            USHORT StringIndex;
            USHORT Reserved2;
            USHORT DesignatorIndex;
            USHORT Reserved3;
            USHORT DataIndex;
            USHORT Reserved4;
        } NotRange;
    } u;
} HIDP_BUTTON_CAPS, *PHIDP_BUTTON_CAPS;

// hid.dll —— HidD_*
EXTERN_C
BOOLEAN __stdcall HidD_GetAttributes(HANDLE HidDeviceObject, PHIDD_ATTRIBUTES Attributes);

EXTERN_C
BOOLEAN __stdcall HidD_GetProductString(HANDLE HidDeviceObject, PWCHAR Buffer, ULONG BufferLength);

EXTERN_C
BOOLEAN __stdcall HidD_GetPreparsedData(HANDLE HidDeviceObject, PHIDP_PREPARSED_DATA* PreparsedData);

EXTERN_C
BOOLEAN __stdcall HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);

EXTERN_C
BOOLEAN __stdcall HidD_FlushQueue(HANDLE HidDeviceObject);

EXTERN_C
BOOLEAN __stdcall HidD_GetFeature(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);

EXTERN_C
BOOLEAN __stdcall HidD_SetFeature(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);

EXTERN_C
BOOLEAN __stdcall HidD_GetInputReport(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);

EXTERN_C
BOOLEAN __stdcall HidD_SetOutputReport(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);

// hid.dll —— HidP_*
EXTERN_C
NTSTATUS __stdcall HidP_GetCaps(PHIDP_PREPARSED_DATA PreparsedData, PHIDP_CAPS Capabilities);

EXTERN_C
NTSTATUS __stdcall HidP_GetValueCaps(HIDP_REPORT_TYPE ReportType, PHIDP_VALUE_CAPS ValueCaps, PULONG ValueCapsLength, PHIDP_PREPARSED_DATA PreparsedData);

EXTERN_C
NTSTATUS __stdcall HidP_GetButtonCaps(HIDP_REPORT_TYPE ReportType, PHIDP_BUTTON_CAPS ButtonCaps, PULONG ButtonCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
