#pragma once
/* LVGL 9.x Zusatzmodul fuer das offizielle Elecrow DIS07050H V3.0 Beispiel.
 * Einbindung: #include "filament_temp_ui.h" nach #include <lvgl.h>
 * In setup() nach LVGL-Displayregistrierung: filament_ui_begin();
 * In loop() VOR lv_timer_handler(): filament_ui_poll();
 * Wichtig: GPIO43/44 sind UART0 ueber CH340, Core Debug-Ausgaben deaktivieren.
 */
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

static HardwareSerial filament_uart(0);
static lv_obj_t *filament_value[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *filament_connection = nullptr;
static uint32_t filament_last_valid = 0;
static char filament_rx[96] = {};
static size_t filament_rxlen = 0;

static void filament_label(lv_obj_t *parent, const char *text, int x, int y,
                           uint32_t color, const lv_font_t *font) {
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
  if (font) lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
  lv_obj_set_pos(lbl, x, y);
}

static void filament_ui_begin() {
  filament_uart.begin(115200, SERIAL_8N1, 44, 43);
  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101B2A), LV_PART_MAIN);
  filament_label(screen, "FILAMENT RECYCLER", 26, 22, 0xFFFFFF, &lv_font_montserrat_24);
  filament_connection = lv_label_create(screen);
  lv_obj_set_pos(filament_connection, 588, 28);
  lv_label_set_text(filament_connection, "USB OFFLINE");
  lv_obj_set_style_text_color(filament_connection, lv_color_hex(0xFF7272), LV_PART_MAIN);
  const char *titles[3] = {"EINGANG", "MITTE", "DUESE"};
  const char *setpoints[3] = {"Soll: 175 C*", "Soll: 190 C*", "Soll: 205 C*"};
  const uint32_t colors[3] = {0x56C7FF, 0xFFC266, 0xFF766E};
  for (int i = 0; i < 3; ++i) {
    int x = 25 + i * 258;
    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_pos(card, x, 115);
    lv_obj_set_size(card, 242, 240);
    lv_obj_set_style_radius(card, 15, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x203249), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    filament_label(card, titles[i], 12, 15, 0xB5CBDD, nullptr);
    filament_value[i] = lv_label_create(card);
    lv_label_set_text(filament_value[i], "--.- C");
    lv_obj_set_pos(filament_value[i], 12, 79);
    lv_obj_set_style_text_font(filament_value[i], &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(filament_value[i], lv_color_hex(colors[i]), LV_PART_MAIN);
    filament_label(card, setpoints[i], 12, 172, 0xB5CBDD, nullptr);
  }
  filament_label(screen, "LIVE-MESSUNG | Heizungen gesperrt | * Sollwerte nur Referenz", 26, 415, 0x9FB5C8, nullptr);
}

static void filament_ui_message(char *line) {
  if (strcmp(line, "OFFLINE") == 0) return;
  if (strncmp(line, "TEMP,", 5) != 0) return;
  char *saveptr = nullptr;
  char *token = strtok_r(line + 5, ",", &saveptr);
  char values[3][24];
  for (int i = 0; i < 3; ++i) {
    if (!token) return;
    if (!strcmp(token, "ERR")) strcpy(values[i], "SENSOR ERR");
    else {
      char *end = nullptr;
      float v = strtof(token, &end);
      if (!end || *end || !isfinite(v) || v < -20 || v > 300) return;
      snprintf(values[i], sizeof(values[i]), "%.1f C", v);
    }
    token = strtok_r(nullptr, ",", &saveptr);
  }
  if (token != nullptr) return;
  for (int i = 0; i < 3; ++i) lv_label_set_text(filament_value[i], values[i]);
  filament_last_valid = millis();
}

static void filament_ui_poll() {
  while (filament_uart.available()) {
    const char c = (char)filament_uart.read();
    if (c == '\n') {
      filament_rx[filament_rxlen] = 0;
      filament_ui_message(filament_rx);
      filament_rxlen = 0;
    } else if (c != '\r') {
      if (filament_rxlen < sizeof(filament_rx) - 1) filament_rx[filament_rxlen++] = c;
      else filament_rxlen = 0;
    }
  }
  bool connected = filament_last_valid && millis() - filament_last_valid <= 1800;
  static bool previous = false;
  if (connected != previous) {
    previous = connected;
    lv_label_set_text(filament_connection, connected ? "USB ONLINE" : "USB OFFLINE");
    lv_obj_set_style_text_color(filament_connection,
        lv_color_hex(connected ? 0x65DB9A : 0xFF7272), LV_PART_MAIN);
    if (!connected) for (int i = 0; i < 3; ++i) lv_label_set_text(filament_value[i], "--.- C");
  }
}
