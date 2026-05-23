#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_TIME = 2,
    SCREEN_ID_WEIGHT = 3,
    SCREEN_ID_WEIGHT_SET = 4,
    SCREEN_ID_KOUGAN = 5,
    SCREEN_ID_MIXER = 6,
    _SCREEN_ID_LAST = 6
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *time;
    lv_obj_t *weight;
    lv_obj_t *weight_set;
    lv_obj_t *kougan;
    lv_obj_t *mixer;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *jiao_zi;
    lv_obj_t *jiaozi;
    lv_obj_t *mianbao;
    lv_obj_t *obj2;
    lv_obj_t *mantou;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *hour;
    lv_obj_t *minute;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *back_time;
    lv_obj_t *fanhui;
    lv_obj_t *ok_time;
    lv_obj_t *fanhui_1;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *set_200g;
    lv_obj_t *set_300g;
    lv_obj_t *set_500g;
    lv_obj_t *set_1000g;
    lv_obj_t *weight_self;
    lv_obj_t *back_weight;
    lv_obj_t *fanhui_4;
    lv_obj_t *ok_weight;
    lv_obj_t *fanhui_8;
    lv_obj_t *select_weight;
    lv_obj_t *dough_add;
    lv_obj_t *dough_sub;
    lv_obj_t *back_weight_set;
    lv_obj_t *fanhui_5;
    lv_obj_t *ok_weight_set;
    lv_obj_t *fanhui_9;
    lv_obj_t *ruan_nuo;
    lv_obj_t *shi_zhong;
    lv_obj_t *jiao_jin;
    lv_obj_t *back_kougan;
    lv_obj_t *fanhui_6;
    lv_obj_t *ok_kougan;
    lv_obj_t *fanhui_10;
    lv_obj_t *star_mixer;
    lv_obj_t *stop;
    lv_obj_t *back_mixer;
    lv_obj_t *fanhui_7;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_time();
void tick_screen_time();

void create_screen_weight();
void tick_screen_weight();

void create_screen_weight_set();
void tick_screen_weight_set();

void create_screen_kougan();
void tick_screen_kougan();

void create_screen_mixer();
void tick_screen_mixer();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/