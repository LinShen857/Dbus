# Dbus

## 1. 模块作用

DJI DBUS 遥控接收：18 字节定长帧的 DMA 接收、通道/开关/鼠标/键盘解析、帧计数与链路超时判断。帧到达通过内部信号量通知，使用方在自己的任务里等待并处理。

DJI DBUS receiver with 18-byte frame decoding, status counters and a link-timeout flag.

## 2. 构造接口

```cpp
Dbus(LibXR::UART& uart);
```

依赖参数 / Dependencies:

- `LibXR::UART& uart` — 接收机所在串口（波特率 100000，8E1）

## 3. 公开接口

- `LibXR::ErrorCode WaitFrame(uint32_t timeout = UINT32_MAX)` — 等一帧到达
- `void Process()` — 处理积压帧（任务上下文）
- `rc_ctrl_s Snapshot()` / `rc_status_s Status()`
- `SetLinkTimeout(bool)` / `IsLinkTimeout()` / `IsHoldRequested()` / `IsEStopRequested()`
- 常量：`sw_up` / `sw_mid` / `sw_down` / `ch_value_mid`
- `static Dbus* Instance()`

## 4. 配置示例

```yaml
modules:
  - module: LinShen857/Dbus
    id: dbus
    args:
      - uart: uart5
```

配合的任务循环：

```cpp
Dbus* dbus = Dbus::Instance();
if (dbus != nullptr)
{
  (void)dbus->WaitFrame();
  dbus->Process();
}
```

## 5. 依赖与硬件

模块依赖 / Module dependencies:

无 / None

硬件 / Hardware:

- `LibXR::UART`
- RX 引脚建议开内部上拉，避免接收机冷启动期间的假起始位
