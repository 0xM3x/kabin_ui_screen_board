#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "lv_conf.h"
#include <TFT_eSPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "ui.h"  

typedef struct {
  float gaz;
  float nem;
  int doluluk;
  bool show_press_screen;
} struct_message;

struct_message incomingData;

lv_obj_t* currentScreen = NULL;             // tracks current screen
unsigned long lastScreenSwitchTime = 0;     // tracks last time we switched

TFT_eSPI tft = TFT_eSPI();

#define LVGL_TICK_PERIOD 5
const unsigned long pressScreenDuration = 20000; // 20 seconds
bool animatingBar = false;


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1,
                    area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1);
  tft.pushColors((uint16_t *)&color_p->full,
                 (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1), true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void animate_arc_value(int new_value) {
  static int current_val = 0;

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui_Arc2);
  lv_anim_set_values(&a, current_val, new_value);
  lv_anim_set_time(&a, 500); // duration in milliseconds
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_arc_set_value((lv_obj_t*)obj, v);
  });

  lv_anim_start(&a);
  current_val = new_value;
}

void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&incomingData, data, len);

    // Update arc
    int distance = constrain(incomingData.doluluk, 25, 110);
    int percentage = map(distance, 25, 110, 100, 0);
    animate_arc_value(percentage);
    lv_obj_invalidate(ui_Arc2);

    // If message requests screen switch
    if (incomingData.show_press_screen && currentScreen != ui_ScreenPress) {
      lv_scr_load(ui_ScreenPress);
      currentScreen = ui_ScreenPress;
      lastScreenSwitchTime = millis();
      animatingBar = true;
      lv_bar_set_value(ui_Pressbar, 0, LV_ANIM_OFF); // reset
    }
  }
}



void setup() {
  Serial.begin(115200); 
  tft.begin();
  tft.setRotation(1);  // Adjust based on CrowPanel orientation

  lv_init();

  static lv_disp_draw_buf_t draw_buf;
  static lv_color_t buf1[320 * 10];
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  ui_init();
  lv_scr_load(ui_Screenrate);  // Only load Screenrate
  currentScreen = ui_Screenrate;
  

  WiFi.mode(WIFI_STA);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  wifi_second_chan_t second;
  uint8_t primary;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("📡 Channel: %d\n", primary);
  esp_wifi_set_promiscuous(false);


  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  Serial.print("🧠 Screen MAC: ");
  Serial.println(WiFi.macAddress());

  esp_now_register_recv_cb(onDataReceive);

  Serial.println("✅ Screenrate UI ready and listening via ESP-NOW...");
}

void loop() {
  lv_timer_handler();
  lv_tick_inc(LVGL_TICK_PERIOD);
  delay(LVGL_TICK_PERIOD);


  if (currentScreen == ui_ScreenPress) {
    unsigned long elapsed = millis() - lastScreenSwitchTime;

    if (elapsed < pressScreenDuration && animatingBar) {
      int barVal = map(elapsed, 0, pressScreenDuration, 0, 100);
      lv_bar_set_value(ui_Pressbar, barVal, LV_ANIM_ON);
    }

    if (elapsed >= pressScreenDuration) {
      lv_scr_load(ui_Screenrate);
      currentScreen = ui_Screenrate;
      animatingBar = false;
    }
  }

}
