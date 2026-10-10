#include <Arduino.h>
#include <M5Unified.h>
#include "SensorReader.h"
#include "TimerTasks.h"

namespace {
lap_timer::SensorReader sensor;
}
void setup() {
  M5.begin();
  Serial.begin(115200);
  sensor.initialize();
  if (!lap_timer::startTimerTasks(sensor)) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("SYSTEM_RESOURCE");
    M5.Display.println("Reset device.");
    Serial.println("ERROR=SYSTEM_RESOURCE");
  }
}
void loop() {
  delay(1000);
}
