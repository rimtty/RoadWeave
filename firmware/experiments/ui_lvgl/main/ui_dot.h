#pragma once
#include "ui_screens.h"
lv_obj_t *ui_dot_create(void);
void ui_dot_show(unsigned design);
void ui_dot_update(const ui_model_t *model);
void ui_dot_cancel_touch(void);
bool ui_dot_is_screen(ui_screen_t screen);
