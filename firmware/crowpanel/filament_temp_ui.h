#pragma once
// FilamentRecycler CrowPanel V3.0 - LVGL 9 Temperaturanzeige (nur lesen)
// UART0 -> CH340 -> USB -> Raspberry Pi, 115200 8N1.
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static HardwareSerial fr_uart(0);
static lv_obj_t *fr_value[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *fr_status = nullptr;
static uint32_t fr_last_good = 0;
static char fr_rx[96] = {};
static size_t fr_rxlen = 0;
static bool fr_connected = false;

static const lv_font_t *fr_font_large() {
#if LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#else
  return LV_FONT_DEFAULT;
#endif
}

static lv_obj_t *fr_label(lv_obj_t *parent, const char *txt, int x, int y,
                           uint32_t color, const lv_font_t *font = nullptr) {
  lv_obj_t *obj = lv_label_create(parent);
  lv_label_set_text(obj, txt);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN);
  if (font) lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
  return obj;
}

static void filament_ui_begin() {
  // CH340/USB ist direkt an ESP32-S3 UART0 (GPIO44 RX, GPIO43 TX).
  fr_uart.begin(115200, SERIAL_8N1, 44, 43);
  lv_obj_t *scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101B2A), LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  fr_label(scr, "FILAMENT RECYCLER", 24, 25, 0xFFFFFF, fr_font_large());
  fr_label(scr, "TEMPERATUR-MONITOR V0.3", 26, 66, 0x9FB5C8);
  fr_status = fr_label(scr, "USB OFFLINE", 642, 32, 0xFF7272);
  const char *titles[3] = {"EINGANG", "MITTE", "DUESE"};
  const uint32_t colors[3] = {0x56C7FF, 0xFFC266, 0xFF766E};
  for (int i = 0; i < 3; ++i) {
    int x = 24 + i * 258;
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_pos(card, x, 127);
    lv_obj_set_size(card, 242, 240);
    lv_obj_set_style_radius(card, 15, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x203249), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    fr_label(card, titles[i], 17, 25, 0xB5CBDD, fr_font_large());
    fr_value[i] = fr_label(card, "--.- C", 17, 105, colors[i], fr_font_large());
    fr_label(card, "ISTTEMPERATUR", 17, 203, 0xB5CBDD);
  }
  fr_label(scr, "RAMPS 1.6  |  3x NTC  |  HEIZUNGEN GESPERRT", 25, 415, 0x9FB5C8);
  fr_label(scr, "Messintervall: 500 ms", 610, 446, 0x9FB5C8);
}

static void fr_set_online(bool on) {
  if (fr_connected == on) return;
  fr_connected = on;
  lv_label_set_text(fr_status, on ? "USB ONLINE" : "USB OFFLINE");
  lv_obj_set_style_text_color(fr_status,
      lv_color_hex(on ? 0x65DB9A : 0xFF7272), LV_PART_MAIN);
  if (!on) {
    for (int i = 0; i < 3; ++i) lv_label_set_text(fr_value[i], "--.- C");
  }
}

static void fr_parse(char *line) {
  if (strcmp(line, "OFFLINE") == 0) { fr_set_online(false); fr_last_good = 0; return; }
  if (strncmp(line, "TEMP,", 5) != 0) return;
  char values[3][24];
  char *context = nullptr;
  char *tok = strtok_r(line + 5, ",", &context);
  for (int i = 0; i < 3; ++i) {
    if (!tok) return;
    if (!strcmp(tok, "ERR")) {
      strcpy(values[i], "SENSOR ERR");
    } else {
      char *end = nullptr;
      float v = strtof(tok, &end);
      if (end == tok || *end != '\0' || !isfinite(v) || v < -20 || v > 300) return;
      snprintf(values[i], sizeof(values[i]), "%.1f C", v);
    }
    tok = strtok_r(nullptr, ",", &context);
  }
  if (tok != nullptr) return;
  for (int i = 0; i < 3; ++i) lv_label_set_text(fr_value[i], values[i]);
  fr_last_good = millis();
  fr_set_online(true);
}

static void filament_ui_poll() {
  // Begrenzte Arbeit pro LVGL-Runde, damit das Display responsiv bleibt.
  int budget = 128;
  while (budget-- > 0 && fr_uart.available()) {
    char c = static_cast<char>(fr_uart.read());
    if (c == '\n') {
      fr_rx[fr_rxlen] = 0;
      fr_parse(fr_rx);
      fr_rxlen = 0;
    } else if (c != '\r') {
      if (fr_rxlen < sizeof(fr_rx) - 1) fr_rx[fr_rxlen++] = c;
      else fr_rxlen = 0;
    }
  }
  if (fr_connected && (uint32_t)(millis() - fr_last_good) > 1800) {
    fr_set_online(false);
  }
}
