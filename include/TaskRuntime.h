#pragma once
#include "SensorReader.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
namespace lap_timer {
struct TaskRuntime {
  SensorReader* reader = nullptr;
  QueueHandle_t displayQueue = nullptr;
  QueueHandle_t ackQueue = nullptr;
  EventGroupHandle_t startGate = nullptr;
  TaskHandle_t measurement = nullptr;
  TaskHandle_t input = nullptr;
  TaskHandle_t presentation = nullptr;
};
extern TaskRuntime runtime;
TickType_t taskTicks(std::uint32_t ms);
void waitForTaskStart();
bool takeACommand();
bool configureModeTasks();
void measurementTask(void*);
void presentationTask(void*);
}  // namespace lap_timer
