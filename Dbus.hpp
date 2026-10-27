#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: DJI DBUS 遥控接收（18 字节定长帧）
depends: []
=== END MANIFEST === */
// clang-format on

#include <cstdint>

#include "libxr.hpp"
#include "semaphore.hpp"
#include "uart.hpp"

class Dbus
{
 public:
  static constexpr uint16_t frame_length = 18;
  static constexpr uint16_t rx_buf_length = 36;
  static constexpr uint8_t sw_up = 1;
  static constexpr uint8_t sw_mid = 3;
  static constexpr uint8_t sw_down = 2;
  static constexpr int16_t ch_value_mid = 1024;

  struct rc_ctrl_s
  {
    struct
    {
      int16_t ch0;
      int16_t ch1;
      int16_t ch2;
      int16_t ch3;
      int16_t ch4;
      uint8_t s1;
      uint8_t s2;
    } rc;

    struct
    {
      int16_t x;
      int16_t y;
      int16_t z;
      uint8_t press_l;
      uint8_t press_r;
    } mouse;

    struct
    {
      uint16_t v;
    } key;

    uint32_t update_tick;
  };

  struct rc_status_s
  {
    uint32_t rx_event_count;
    uint32_t valid_frame_count;
    uint32_t invalid_frame_count;
    uint32_t error_count;
    uint16_t last_rx_size;
  };

  explicit Dbus(LibXR::UART& uart);

  /// 处理一帧积压数据，在任务上下文调用 / Consume one pending frame in task context.
  void Process();

  rc_ctrl_s Snapshot() const;
  rc_status_s Status() const;

  void SetLinkTimeout(bool timeout);
  bool IsLinkTimeout() const;
  bool IsHoldRequested() const;
  bool IsEStopRequested() const;

  /// 等待新帧到达 / Wait for the next frame.
  LibXR::ErrorCode WaitFrame(uint32_t timeout = UINT32_MAX);

  static Dbus* Instance();

 private:
  static void WaitCallback(bool in_isr, Dbus* self, LibXR::ErrorCode ec);
  static void ReadCallback(bool in_isr, Dbus* self, LibXR::ErrorCode ec);
  uint8_t DecodeFrame(const uint8_t* f);

  LibXR::UART& uart_;
  LibXR::Callback<LibXR::ErrorCode> cb_wait_;
  LibXR::Callback<LibXR::ErrorCode> cb_read_;
  LibXR::ReadOperation op_wait_;
  LibXR::ReadOperation op_read_;
  LibXR::Semaphore frame_sem_ {0};

  alignas(32) uint8_t rx_buf_[rx_buf_length] {};
  volatile size_t rx_len_ = 0;
  volatile bool frame_pending_ = false;
  rc_ctrl_s rc_ctrl_ {};
  rc_status_s rc_status_ {};
  volatile uint32_t error_count_ = 0;
  volatile bool link_timeout_ = false;

  static Dbus* instance_;
};
