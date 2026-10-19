# Mchose_G87_A5Ultra

迈从（MCHOSE）G87 键盘 + A5 Ultra 鼠标 2.4G 电量显示 —— TrafficMonitor 任务栏插件（合并版）。

一个 DLL 同时提供两个显示项：

- **键盘:** —— G87 2.4G 键盘电量（VID_41E4 / PID_2001）
- **鼠标:** —— A5 Ultra 2.4G 鼠标电量（VID_3837 / PID_100C）

## 功能特性

- 任务栏实时显示键盘 / 鼠标 2.4G 电量百分比
- **按需查询，不阻止休眠**：仅在设备唤醒（闲置一段时间后的首次按键/移动）或初次启动时查询一次，无周期轮询
- **句柄即查即关**：查询瞬间才打开 HID 句柄，查询后立即关闭，不与官方 M HUB 驱动/网页驱动争用
- 设备休眠时保持最后读数，唤醒后自动刷新
- 鼠标/接收器断开自动重连

## 电量读取原理

| 设备 | 通道 | 协议 |
|---|---|---|
| G87 键盘 | VID_41E4 / PID_2001，UsagePage 0xFF02，20 字节帧，Report ID 0x13 | 开会话 `13 05 01` → 查询 `13 4A 01` → 响应 `[5]=电量 [6]&0x10=充电`（校验=前19字节求和&0xFF） |
| A5 Ultra 鼠标 | VID_3837 / PID_100C，UsagePage 0xFF01，feature report 65 字节 | `SetFeature(0x11, 0x06^0xFF, 0xFF×19)` 请求设备信息 → `GetFeature(0x11, 65B)` 读响应：电量=resp[11]（反码帧则 ^0xFF） |

鼠标协议逆向自官方 M HUB 网页驱动（www.mchose.com.cn assets 中的 theme / purify JS）。

## 安装

1. 下载 Release 中的 `MchoseBatteryCombined.dll`
2. 复制到 TrafficMonitor 的 `plugins` 目录
3. 重启 TrafficMonitor，在插件管理中勾选"迈从设备电量显示"
4. 任务栏出现 `键盘:` 与 `鼠标:` 两个显示项

> 若此前装过旧版单设备插件（`MchoseBattery.dll` / `MchoseBatteryMouse.dll`），请先删除，避免重复显示。

## 编译

需要 Visual Studio（含"使用 C++ 的桌面开发"工作负载）与 Windows SDK。

```bat
build.bat
```

输出：`out\MchoseBatteryCombined.dll`

## 目录结构

```
src/
  PluginInterface.h          TrafficMonitor 插件接口
  HidApi.h                   HID API 声明（免 SDK hidsdi.h/hidpi.h）
  BatteryCommon.h            键盘/鼠标共用数据结构
  MchoseKeyboardDevice.*     G87 键盘 0x13 协议层
  MchoseKeyboardReader.*     键盘后台线程（按需查询 + 按键检测 + 重连）
  MchoseKeyboardItem.*       键盘显示项（IPluginItem）
  MchoseMouseDevice.*        A5 Ultra SetFeature+GetFeature 协议层
  MchoseMouseReader.*        鼠标后台线程（按需查询 + 活动检测 + 重连）
  MchoseMouseItem.*          鼠标显示项（IPluginItem）
  MchosePlugin.*             插件主类（GetItem(0)=键盘 / GetItem(1)=鼠标）
  dllmain.cpp                DLL 入口
build.bat                    一键编译脚本
```

## License

MIT
