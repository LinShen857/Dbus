#include "Dbus.hpp"

#include "timebase.hpp"

Dbus* Dbus::instance_ = nullptr;

Dbus::Dbus(LibXR::UART& uart)
    : uart_(uart), cb_wait_(), cb_read_(), op_wait_(), op_read_(), rc_ctrl_ {}, rc_status_ {}
{
  cb_wait_ = LibXR::Callback<LibXR::ErrorCode>::CreateGuarded(WaitCallback, this);
  cb_read_ = LibXR::Callback<LibXR::ErrorCode>::CreateGuarded(ReadCallback, this);
  op_wait_ = LibXR::ReadOperation(cb_wait_);
  op_read_ = LibXR::ReadOperation(cb_read_);
  (void)uart_.Read({nullptr, 0}, op_wait_, false);
  instance_ = this;
}

Dbus* Dbus::Instance()
{
  return instance_;
}

void Dbus::WaitCallback(bool in_isr, Dbus* self, LibXR::ErrorCode ec)
{
  if (ec != LibXR::ErrorCode::OK)
  {
    self->error_count_ = self->error_count_ + 1;
  }

  const size_t avail = self->uart_.read_port_->Size();
  self->rx_len_ = avail > rx_buf_length ? rx_buf_length : avail;
  (void)self->uart_.Read({self->rx_buf_, self->rx_len_}, self->op_read_, in_isr);
}

void Dbus::ReadCallback(bool in_isr, Dbus* self, LibXR::ErrorCode ec)
{
  if (ec != LibXR::ErrorCode::OK)
  {
    self->error_count_ = self->error_count_ + 1;
  }

  self->frame_pending_ = true;
  self->frame_sem_.PostFromCallback(in_isr);
}

uint8_t Dbus::DecodeFrame(const uint8_t* f)
{
  const uint8_t s1 = static_cast<uint8_t>((f[5] >> 4) & 0x03U);
  const uint8_t s2 = static_cast<uint8_t>((f[5] >> 6) & 0x03U);

  if (s1 == 0U || s2 == 0U)
  {
    ++rc_status_.invalid_frame_count;
    return 0U;
  }

  rc_ctrl_.rc.ch0 = static_cast<int16_t>((f[0] | (f[1] << 8)) & 0x07FF) - ch_value_mid;
  rc_ctrl_.rc.ch1 = static_cast<int16_t>(((f[1] >> 3) | (f[2] << 5)) & 0x07FF) - ch_value_mid;
  rc_ctrl_.rc.ch2 = static_cast<int16_t>(
                        ((f[2] >> 6) | (f[3] << 2) | (f[4] << 10)) & 0x07FF) -
                    ch_value_mid;
  rc_ctrl_.rc.ch3 = static_cast<int16_t>(((f[4] >> 1) | (f[5] << 7)) & 0x07FF) - ch_value_mid;
  rc_ctrl_.rc.ch4 = static_cast<int16_t>((f[16] | (f[17] << 8)) & 0x07FF) - ch_value_mid;

  rc_ctrl_.rc.s1 = s1;
  rc_ctrl_.rc.s2 = s2;

  rc_ctrl_.mouse.x = static_cast<int16_t>(f[6] | (f[7] << 8));
  rc_ctrl_.mouse.y = static_cast<int16_t>(f[8] | (f[9] << 8));
  rc_ctrl_.mouse.z = static_cast<int16_t>(f[10] | (f[11] << 8));
  rc_ctrl_.mouse.press_l = f[12];
  rc_ctrl_.mouse.press_r = f[13];

  rc_ctrl_.key.v = static_cast<uint16_t>(f[14] | (f[15] << 8));

  rc_ctrl_.update_tick = static_cast<uint32_t>(LibXR::Timebase::GetMilliseconds());
  ++rc_status_.valid_frame_count;
  return 1U;
}

void Dbus::Process()
{
  if (!frame_pending_)
  {
    return;
  }
  frame_pending_ = false;

  const size_t size = rx_len_;

  ++rc_status_.rx_event_count;
  rc_status_.last_rx_size = static_cast<uint16_t>(size);

  if (size >= frame_length)
  {
    const size_t offset = size - (size % frame_length) - frame_length;
    (void)DecodeFrame(&rx_buf_[offset]);
  }

  (void)uart_.Read({nullptr, 0}, op_wait_, false);
}

Dbus::rc_ctrl_s Dbus::Snapshot() const
{
  return rc_ctrl_;
}

Dbus::rc_status_s Dbus::Status() const
{
  rc_status_s status = rc_status_;
  status.error_count = error_count_;
  return status;
}

void Dbus::SetLinkTimeout(bool timeout_)
{
  link_timeout_ = timeout_;
}

bool Dbus::IsLinkTimeout() const
{
  return link_timeout_;
}

bool Dbus::IsHoldRequested() const
{
  const rc_ctrl_s snapshot = Snapshot();
  return (snapshot.rc.s1 == sw_down) && (snapshot.rc.s2 == sw_down);
}

bool Dbus::IsEStopRequested() const
{
  if (Status().valid_frame_count == 0U)
  {
    return true;
  }
  return IsHoldRequested() || link_timeout_;
}

LibXR::ErrorCode Dbus::WaitFrame(uint32_t timeout)
{
  return frame_sem_.Wait(timeout);
}
