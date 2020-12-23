#ifndef MAIN_H
#define MAIN_H

#include "input.h"

struct led_stripe_t;

ALLEGRO_DISPLAY* init(InputHandler &);
int splash_screen(InputHandler &, led_stripe_t &);
int menu(InputHandler &, led_stripe_t &);
int fast_race(InputHandler &, led_stripe_t &);
int tournee(InputHandler &);
int racing_loop (uint8_t runden, std::vector<bool> const & activated, led_stripe_t &, InputHandler & handler);
int options(InputHandler & input);
int tournee_race(tournee_plan* tp);
void print_time(size_t format, nanotime_t time, std::ostream & out);
void draw_time(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
void draw_ptime(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
int show_tournee_plan(tournee_plan &tp, tournee_plan_creator* tpc);

#endif
