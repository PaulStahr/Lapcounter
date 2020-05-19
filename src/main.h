#ifndef MAIN_H
#define MAIN_H

#include "input.h"

ALLEGRO_DISPLAY* init(InputHandler &);
int splash_screen(InputHandler & );
int menu(InputHandler &);
int fast_race(InputHandler &);
int tournee(InputHandler &);
int anzeige (uint8_t runden, uint8_t spieler, InputHandler & handler);
int options(InputHandler & input);
int tournee_race(tournee_plan* tp);
void print_time(size_t format, nanotime_t time, std::ostream & out);
void draw_time(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
void draw_ptime(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
int show_tournee_plan(tournee_plan &tp, tournee_plan_creator* tpc);

#endif
