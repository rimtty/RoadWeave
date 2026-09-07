#pragma once
#include "ui_screens.h"

/* Pure, deterministic sample data shared by ESP-IDF and the PC simulator. */
void ui_simulate(ui_model_t *model, float seconds);
void ui_simulate_dot(ui_model_t *model, float seconds);
