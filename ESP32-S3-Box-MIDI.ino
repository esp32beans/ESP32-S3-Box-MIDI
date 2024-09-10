/*
 * MIT License
 *
 * Copyright (c) 2024 esp32beans@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*
 * For ESP32 S3 Box and ESP32 S3 Box 3
 * Modify ESP_Panel_conf.h to switch between the original Box and the Box 3.
 * Convert capacitive touch screen (x,y) to USB MIDI CC.
 */

#define CC_Modulation (1)

#define CC14_MODULATION_ENABLE (1)

#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
#endif

#include <ESP_Panel_Library.h>
#include <lvgl.h>
#include "lvgl_port_v8.h"
#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

ESP_Panel *panel = nullptr;
ESP_PanelLcd *lcd = nullptr;
ESP_PanelTouch *touch = nullptr;
ESP_PanelBacklight *backlight = nullptr;

const int SCREEN_X_MAX = 319;
const int SCREEN_Y_MAX = 239;

static lv_obj_t * label;

void CC14(uint8_t control, uint16_t cc14_value, uint8_t chan) {
  MIDI.controlChange(CC_Modulation, (uint8_t)((cc14_value >> 7) & 0x7F), chan);
  MIDI.controlChange(CC_Modulation+32, (uint8_t)(cc14_value & 0x7F), chan);
}

static void slider_event_cb(lv_event_t * e) {
  static int32_t pitch_last;
  int32_t pitch;
  lv_obj_t * slider = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    /* Get MIDI pitch bend */
    pitch = lv_slider_get_value(slider);
  } else {  /* LV_EVENT_RELEASED */
    pitch = 8192;
    lv_slider_set_value(slider, pitch, LV_ANIM_OFF);
  }
  if (pitch != pitch_last) {
    pitch_last = pitch;
#if CC14_MODULATION_ENABLE
    CC14(CC_Modulation, pitch, 1);
#else
    MIDI.pitchBend((uint16_t)pitch);
#endif
    /* Refresh the text */
    lv_label_set_text_fmt(label, "%" LV_PRId32, pitch);
    lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 0, -15);    /*Align top of the slider*/
  }
}

/**
 * Create a slider and write its value on a label.
 */
void lv_create_slider(void) {
  /* Create a big slider in the center of the display */
  lv_obj_t * slider = lv_slider_create(lv_scr_act());
  lv_obj_set_size(slider, SCREEN_X_MAX+1, (SCREEN_Y_MAX+1)/2);
  lv_obj_center(slider);  /* Align to the center of the parent (screen) */
  lv_slider_set_range(slider, 0, 16383);
  lv_slider_set_value(slider, 8192, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);     /*Assign an event function*/
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_RELEASED, NULL);     /*Assign an event function*/

  /* Create a label above the slider */
  label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "8192");
  lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 0, -15);    /*Align top of the slider*/
}

void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() < 4000)) delay(10);
  Serial.println("USB MIDI start");

  panel = new ESP_Panel();

  /* Initialize bus and device of panel */
  panel->init();
#if LVGL_PORT_AVOID_TEAR
  // When avoid tearing function is enabled, configure the RGB bus according to the LVGL configuration
  ESP_PanelBus_RGB *rgb_bus = static_cast<ESP_PanelBus_RGB *>(panel->getLcd()->getBus());
  rgb_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
  rgb_bus->configRgbBounceBufferSize(LVGL_PORT_RGB_BOUNCE_BUFFER_SIZE);
#endif
  /* Start panel */
  panel->begin();

  lcd = panel->getLcd();
  if (lcd == nullptr) {
    Serial.println("LCD is not available");
  }
  touch = panel->getTouch();
  if (touch == nullptr) {
    Serial.println("Touch is not available");
  }
  backlight = panel->getBacklight();
  if (backlight != nullptr) {
    Serial.println("Turn off the backlight");
    backlight->off();
  } else {
    Serial.println("Backlight is not available");
  }

  Serial.println("Initialize LVGL");
  lvgl_port_init(lcd, touch);

  Serial.println("Create UI");
  if (backlight != nullptr) {
    backlight->setBrightness(100);
  } else {
    Serial.println("Backlight is not available");
  }

  /* Lock the mutex due to the LVGL APIs are not thread-safe */
  lvgl_port_lock(-1);
  lv_create_slider();
  /* Release the mutex */
  lvgl_port_unlock();

  MIDI.begin();
  USB.begin();
}

void loop() {
  midiEventPacket_t midi_packet_in = {0};

  if (MIDI.readPacket(&midi_packet_in)) {
    // Ignore incoming MIDI messages
  }
}
