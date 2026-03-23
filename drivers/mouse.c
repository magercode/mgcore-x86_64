#include "mgcore/mouse.h"

#include "mgcore/console.h"
#include "mgcore/io.h"

#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_CMD_PORT 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02

static uint8_t g_packet[4];
static uint8_t g_packet_index;
static uint8_t g_packet_size = 4;
static int g_mouse_ready;

static void ps2_wait_input_clear(void) {
  int spin;
  for (spin = 0; spin < 100000; ++spin) {
    if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
      return;
    }
  }
}

static int ps2_wait_output_full(void) {
  int spin;
  for (spin = 0; spin < 100000; ++spin) {
    if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0) {
      return 1;
    }
  }
  return 0;
}

static void ps2_write_cmd(uint8_t value) {
  ps2_wait_input_clear();
  io_out8(PS2_CMD_PORT, value);
}

static void ps2_write_data(uint8_t value) {
  ps2_wait_input_clear();
  io_out8(PS2_DATA_PORT, value);
}

static uint8_t ps2_read_data(void) {
  if (!ps2_wait_output_full()) {
    return 0;
  }
  return io_in8(PS2_DATA_PORT);
}

static void mouse_write(uint8_t value) {
  ps2_write_cmd(0xD4);
  ps2_write_data(value);
}

static uint8_t mouse_read_ack(void) {
  return ps2_read_data();
}

static void mouse_set_sample_rate(uint8_t rate) {
  mouse_write(0xF3);
  (void)mouse_read_ack();
  mouse_write(rate);
  (void)mouse_read_ack();
}

void mouse_init(void) {
  uint8_t status;
  uint8_t mouse_id;

  g_mouse_ready = 0;
  g_packet_index = 0;

  ps2_write_cmd(0xA8); /* Enable auxiliary device. */
  ps2_write_cmd(0x20);
  status = ps2_read_data();
  status |= 0x02;  /* IRQ12 enable. */
  status &= (uint8_t)~0x20; /* Enable clock for mouse device. */
  ps2_write_cmd(0x60);
  ps2_write_data(status);

  mouse_write(0xF6); /* Set defaults. */
  (void)mouse_read_ack();

  /* IntelliMouse wheel sequence. */
  mouse_set_sample_rate(200);
  mouse_set_sample_rate(100);
  mouse_set_sample_rate(80);

  mouse_write(0xF2); /* Get device ID. */
  (void)mouse_read_ack();
  mouse_id = ps2_read_data();
  if (mouse_id == 3) {
    g_packet_size = 4;
  } else {
    g_packet_size = 3;
  }

  mouse_write(0xF4); /* Enable data reporting. */
  (void)mouse_read_ack();

  g_mouse_ready = 1;
  console_printf("mouse: PS/2 ready packet=%u-byte wheel=%s\n",
                 (unsigned)g_packet_size,
                 g_packet_size == 4 ? "yes" : "no");
}

void mouse_handle_irq(void) {
  int8_t wheel;

  if (!g_mouse_ready) {
    if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0) {
      (void)io_in8(PS2_DATA_PORT);
    }
    return;
  }

  if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0) {
    return;
  }

  g_packet[g_packet_index++] = io_in8(PS2_DATA_PORT);
  if (g_packet_index < g_packet_size) {
    return;
  }

  g_packet_index = 0;
  if ((g_packet[0] & 0x08U) == 0) {
    return;
  }

  if (g_packet_size == 4) {
    wheel = (int8_t)g_packet[3];
    if (wheel > 0) {
      console_scroll_view((int)wheel * 3);
    } else if (wheel < 0) {
      console_scroll_view((int)wheel * 3);
    }
  }
}
