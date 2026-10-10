#pragma once
#include "SensorReader.h"

namespace lap_timer {
// setup() only; reader must outlive every task.
bool startTimerTasks(SensorReader& reader);
}  // namespace lap_timer
