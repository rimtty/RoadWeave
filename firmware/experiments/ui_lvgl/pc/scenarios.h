#pragma once
#include "ui_screens.h"

bool scenario_valid(const char *name);
void scenario_fill(const char *name, float seconds, ui_model_t *model);
ui_screen_t scenario_screen(const char *name);
int test_interactions(void);
int test_scenario(const char *name, const ui_model_t *model);
void sim_action(const char *action, uint32_t arg);
void sim_apply_controls(ui_model_t *model);
void sim_reset_controls(void);
