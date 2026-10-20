# Mchose\_G87\_A5Ultra

迈从（MCHOSE）G87 键盘和 A5 Ultra 鼠标的 2.4G 电量显示插件，用于 TrafficMonitor。



* `键盘:` G87 2.4G 键盘电量（VID\_41E4 / PID\_2001）

* `鼠标:` A5 Ultra 2.4G 鼠标电量（VID\_3837 / PID\_100C）

## 功能



* 任务栏显示键盘 / 鼠标电量百分比

* 充电时显示 `99%⚡`，悬停提示会标注 "充电中"

* 只在需要时查询，不做周期轮询：


  * 插件启动后查一次

  * 设备闲置 120 秒后，首次按键 / 移动时查一次（视为唤醒）

  * 使用中（活动后 30 秒内）每 5 秒刷新一次，停止操作 30 秒后不再查询

* 查询时临时打开 HID 句柄，读完立即关闭，不与官方驱动 / 网页驱动争用

* 设备休眠时保持最后读数，唤醒后自动刷新

* 接收器断开后自动重连，不误判离线

## 电量读取



| 设备          | 通道                                                           | 协议                                                                                                                           |
| ----------- | ------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| G87 键盘      | VID\_41E4 / PID\_2001，UsagePage 0xFF02，20 字节帧，Report ID 0x13 | 开会话 `13 05 01` → 查询 `13 4A 01` → 响应 `[5]=电量 [6]&0x10=充电`；自发推送 `[1]=0x0A [5]=0x05` 为电量推送（`[6]=电量 [7]=充电`）。校验为前 19 字节求和 & 0xFF |
| A5 Ultra 鼠标 | VID\_3837 / PID\_100C，UsagePage 0xFF01，feature report 65 字节  | `SetFeature(0x11, 0x06^0xFF, 0xFF×19)` 请求设备信息 → `GetFeature(0x11, 65B)` 读响应：电量 = resp \[11]（反码帧则 ^0xFF），充电 = resp \[12] bit0 |

鼠标协议来自官方 M HUB 网页驱动（[www.mchose.com.cn](https://www.mchose.com.cn) assets 中的 theme /purify JS）。A5 Ultra 三态 PID：有线 11111、蓝牙 18274、2.4G 4108。读取电量必须先 SetFeature 写 "请求设备信息" 命令（命令字节逐位取反），再 GetFeature 读响应。

## 安装



1. 从 Release 下载 `MchoseBattery.dll`

2. 复制到 TrafficMonitor 的 `plugins` 目录

3. 重启 TrafficMonitor，在插件管理中勾选 "外设电量显示"

4. 任务栏显示 `键盘: 99% | 鼠标: 99%`（离线 / 未检测到的设备显示 `--%`）

## 编译

需要 Visual Studio（"使用 C++ 的桌面开发" 工作负载）和 Windows SDK。



```
build.bat
```

输出 `out\MchoseBattery.dll`。

## 目录结构



```
src/
  PluginInterface.h          TrafficMonitor 插件接口
  HidApi.h                   HID API 声明（免 SDK hidsdi.h/hidpi.h）
  BatteryCommon.h            键盘 / 鼠标共用数据结构
  MchoseKeyboardDevice.*     G87 键盘 0x13 协议层
  MchoseKeyboardReader.*     键盘后台线程（按需查询 + 按键检测 + 重连）
  MchoseMouseDevice.*        A5 Ultra SetFeature+GetFeature 协议层
  MchoseMouseReader.*        鼠标后台线程（按需查询 + 活动检测 + 重连）
  MchosePeripheralItem.*     外设显示项（IPluginItem，组合键盘 / 鼠标）
  MchosePlugin.*             插件主类（GetItem(0)=外设电量显示）
  dllmain.cpp                DLL 入口
build.bat                    一键编译脚本
```

## License

MIT