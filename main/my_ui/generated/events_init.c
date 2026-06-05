#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

void events_init_home(lv_ui *ui)
{
    (void)ui;
}

void events_init(lv_ui *ui)
{
    (void)ui;
}
