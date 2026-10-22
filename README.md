# PeripheralBattery

利用[TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 插件的支持，做的外设电量显示，支持：

- 迈从（MCHOSE）G87 2.4G 键盘
- 迈从（MCHOSE）A5 Ultra 2.4G 鼠标
- 盖世小鸡（GameSir）G7 Pro 2.4G 手柄
- PS5 DualSense 手柄（通过 DS5Dongle 桥接为 USB HID）

任务栏显示 `键盘: 99% | 鼠标: 99% | 手柄: ▮▮▮▮▕ | PS5: 55%`，充电时带 ⚡ 标识，悬停可看详情。

## 各设备读取方式

| 设备 | 通道 | 电量来源 |
| --- | --- | --- |
| G87 键盘 | HID（VID\_41E4），0x13 协议帧 | 查询响应 `[5]=电量`，`[6]&0x10=充电` |
| A5 Ultra 鼠标 | HID（VID\_3837 / PID\_100C，UsagePage 0xFF01） | `SetFeature(11 06)` 请求 → `GetFeature(0x11)` 读响应 `[11]=电量`，`[12]=充电` |
| G7 Pro 手柄 | `\\.\XboxGIP` 总线只读旁观 | 设备主动上报的 Status 消息，低 2 位为 0~3 档（0/33/66/100%），bit4 充电，断开显示 `--` |
| DualSense | USB HID（VID\_054C / PID\_0CE6，DS5Dongle 桥接） | Input Report 0x01 byte[53]：低 4 位档位 0~10（换算 5%~100%），高 4 位充电状态 |

## 刷新策略

- 键盘 / 鼠标：唤醒时查询一次，使用中每 5 秒刷新，闲置后停止查询；休眠时保持最后读数
- G7 Pro：纯被动监听设备上报，无消息时保持最后读数，90 秒无消息视为断开显示 `--`
- DualSense：每 2 秒检测一次设备连接（PnP 枚举），连接中每 60 秒被动读取一次电量，断开显示 `--`

查询时临时打开设备句柄，读完立即关闭，不影响设备休眠，也不与官方驱动 / 网页驱动争用。

## 安装

1. 从 Release 下载 `PeripheralBattery.dll`
2. 复制到 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的 `plugins` 目录
3. 重启 TrafficMonitor，在插件管理中勾选"外设电量显示"

## 编译

需要 Visual Studio（"使用 C++ 的桌面开发"工作负载）和 Windows SDK，然后运行：

```
build.bat
```

输出 `out\PeripheralBattery.dll`。

## 目录结构

```
src/
  common/                公共头文件（PluginInterface.h / HidApi.h / BatteryCommon.h）
  keyboard/              G87 键盘协议层与后台线程
  mouse/                 A5 Ultra 协议层与后台线程
  dualsense/             DualSense 协议层与后台线程
  g7/                    G7 Pro GIP 监听与后台线程
  plugin/                插件主类与显示项（Plugin.* / PeripheralItem.* / dllmain.cpp）
build.bat                一键编译脚本
```

## License

MIT
