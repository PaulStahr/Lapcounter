/*
Copyright (c) 2019 Paul Stahr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <allegro5/allegro.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_opengl.h>
#include <boost/thread.hpp>
#include <cstdint>
#include <iostream>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable>
#include <vector>
#include <fstream>
#include <chrono>
//#include <thread>
#include "server.h"
#include "loghtml.h"
#include "tournee_plan.h"
#include "tournee_plan_approximation_creator.h"
#include "tournee_plan_creator.h"
#include "data.h"
#include "options.h"
#include "performance_counter.h"
#include "firework.h"
#include "main.h"
#include "input.h"
#ifdef RASPBERRY_PI
extern "C" {
#include <wiringPi.h>
}
#endif
#ifdef LEDSTRIPE
extern "C" {
#include "ws2811.h"
}
#endif
// #port1 378
// #port2 379
// #port3 37A

ALLEGRO_FONT *font_small;
ALLEGRO_FONT *font;
ALLEGRO_FONT *font_big;
ALLEGRO_FONT *font_control;
ALLEGRO_BITMAP* car_bitmaps[4];
ALLEGRO_BITMAP* splash_bitmap;
ALLEGRO_SAMPLE *sample;
ALLEGRO_SAMPLE *sample2;
ALLEGRO_SAMPLE *sample3;
ALLEGRO_SAMPLE *sample4;
ALLEGRO_SAMPLE *sample_applause;
ALLEGRO_SAMPLE *sample_start0;
ALLEGRO_SAMPLE *sample_start1;

ALLEGRO_COLOR BACKGROUND_COLOR;
ALLEGRO_COLOR FOREGROUND_COLOR;
ALLEGRO_COLOR SELECTION_COLOR;
ALLEGRO_COLOR ERROR_COLOR;
ALLEGRO_COLOR COLOR_RED;
ALLEGRO_COLOR COLOR_GRAY;
ALLEGRO_COLOR COLOR_GREEN;
ALLEGRO_COLOR COLOR_YELLOW;
WebServer server;


nanotime_t frequency;
int display_height;
int display_width;
options_t opt;

#ifdef RASPBERRY_PI
InputHandler *global_input = nullptr;
template <uint8_t pin>
void sensor_interrupt_input_task()
{
    if (global_input != nullptr)
    {
        InputEvent event(static_cast<Destination>(DSENSOR0 + pin), QueryPerformanceCounter());
        global_input->call(event);
    }
}

void sensor_poll_input_task(InputHandler *handler)
{
    bool active[4];
    while (handler->is_valid())
    {
        for (size_t i = 0; i < 4; ++i)
        {
            bool current = digitalRead(opt._input_pin[i])==1;
            if (current && !active[i]){
                InputEvent event(static_cast<Destination>(DSENSOR0 + i), QueryPerformanceCounter());
                handler->call(event);
            }
            active[i] = current;
        }
    }
}
#endif


struct overlay_button
{
    uint16_t _x,_y,_w,_h;
    const char *_str;
    Destination _dest;
    overlay_button(uint16_t x_, uint16_t y_, uint16_t w_, uint16_t h_, const char *str_, Destination dest_): _x(x_), _y(y_), _w(w_), _h(h_), _str(str_), _dest(dest_){}
};

#ifdef LEDSTRIPE
static ws2811_t *ws2811;
struct led_stripe_t
{
    bool _dirty = false;
    uint32_t * begin()              {return ws2811->channel[0].leds;}
    uint32_t * end()                {return ws2811->channel[0].leds + ws2811->channel[0].count;}
    uint32_t const * begin() const  {return ws2811->channel[0].leds;}
    uint32_t const * end()   const  {return ws2811->channel[0].leds + ws2811->channel[0].count;}
    size_t size() const             {return ws2811->channel[0].count;}
    void render()                   {if (_dirty){ws2811_render(ws2811);_dirty = false;}}
    uint32_t        & operator[] (size_t index)      {return ws2811->channel[0].leds[index];}
    uint32_t const  & operator[] (size_t index) const{return ws2811->channel[0].leds[index];}
};
#else
struct led_stripe_t
{
    bool _dirty;
    std::vector<uint32_t> _leds = std::vector<uint32_t>(10);
    uint32_t * begin()              {return _leds.data();}
    uint32_t * end()                {return _leds.data() + _leds.size();}
    uint32_t const * begin() const  {return _leds.data();}
    uint32_t const * end()   const  {return _leds.data() + _leds.size();}
    size_t size() const             {return _leds.size();}
    void render(){_dirty = false;}
    uint32_t        & operator[] (size_t index)     {return _leds[index];}
    uint32_t const  & operator[] (size_t index)const{return _leds[index];}
};
#endif

static const overlay_button buttons[6] = {
    overlay_button(0,1,1,1,"\u2190",DLEFT),
    overlay_button(1,0,1,1,"\u2191",DUP),
    overlay_button(2,1,1,1,"\u2192",DRIGHT),
    overlay_button(1,2,1,1,"\u2193",DDOWN),
    overlay_button(0,0,1,1,"\u25C4",DBACK),
  //  overlay_button(1,1,1,1,"\u2B24",DENTER),
  //  overlay_button(1,1,1,1,"\u25CF",DENTER),
    overlay_button(1,1,1,1,"\u2022",DENTER)
};

struct debug_overlay_t
{
    void draw(led_stripe_t const & led_stripe){
        for(size_t i = 0; i < led_stripe.size(); ++i)
        {
            uint32_t elem = led_stripe[i];
            al_draw_filled_circle(i*800/led_stripe.size() + 30, 400, 30, al_map_rgb(elem&0xFF,(elem >> 8) & 0xFF,(elem >> 16) & 0xFF));
        }
    }
};

void allegro_input_task(InputHandler *handler)
{
    ALLEGRO_EVENT_QUEUE *event_queue = al_create_event_queue();
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_mouse_event_source());
    al_register_event_source(event_queue, al_get_joystick_event_source());
    while (handler->is_valid())
    {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);
        Destination dest = DUNDEF;
        size_t buttonSize = 40;
        if (ev.type == ALLEGRO_EVENT_JOYSTICK_AXIS)
        {
            switch (ev.joystick.stick * 2 + ev.joystick.axis)
            {
                case 6: if (ev.joystick.pos < -0.5){dest = DLEFT;}
                        if (ev.joystick.pos > 0.5){dest = DRIGHT;}
                    break;
                case 7: if (ev.joystick.pos < -0.5){dest = DUP;}
                        if (ev.joystick.pos > 0.5){dest = DDOWN;}
                    break;
            }
        }
        if (ev.type == ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN)
        {
            switch (ev.joystick.button)
            {
                case 0:     dest = DENTER;break;
                case 1:     dest = DBACK;break;
                case 2:     dest = DLEFT;break;
                case 3:     dest = DRIGHT;break;
                case 4:     dest = DBACK;break;
                case 5:     dest = DENTER;break;
                case 6:     dest = DSENSOR0;break;
                case 7:     dest = DSENSOR1;break;
                case 8:     dest = DSENSOR2;break;
                case 9:     dest = DSENSOR3;break;
            }
        }
        if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN)
        {
            int x = ev.mouse.x / buttonSize, y = ev.mouse.y / buttonSize;
            for (overlay_button const & bt : buttons)
            {
                
                if (bt._x <= x && bt._y <= y && bt._x + bt._w > x && bt._y + bt._h > y)
                {
                    dest = bt._dest;
                    break;
                }
            }
        }
        else if (ev.type == ALLEGRO_EVENT_KEY_DOWN)
        {
            switch(ev.keyboard.keycode) {
                case ALLEGRO_KEY_DOWN:  dest = DDOWN;break;
                case ALLEGRO_KEY_UP:    dest = DUP;break;
                case ALLEGRO_KEY_LEFT:  dest = DLEFT;break;
                case ALLEGRO_KEY_RIGHT: dest = DRIGHT;break;
                case ALLEGRO_KEY_ESCAPE:dest = DBACK;break;
                case ALLEGRO_KEY_ENTER: dest = DENTER;break;
                case ALLEGRO_KEY_1:     dest = DSENSOR0;break;
                case ALLEGRO_KEY_2:     dest = DSENSOR1;break;
                case ALLEGRO_KEY_3:     dest = DSENSOR2;break;
                case ALLEGRO_KEY_4:     dest = DSENSOR3;break;
            }
            if (dest >= DSENSOR0 && dest <= DSENSOR3)
            {
                ALLEGRO_KEYBOARD_STATE key_state;
                al_get_keyboard_state(&key_state);
                if (al_key_down(&key_state, ALLEGRO_KEY_LSHIFT) || al_key_down(&key_state, ALLEGRO_KEY_RSHIFT)){
                    dest = static_cast<Destination>(dest + DNSENSOR0 - DSENSOR0);
                }
            }
        }
        if (dest != DUNDEF)
        {
            handler->call(InputEvent(dest, QueryPerformanceCounter()));
        }
    }
    al_destroy_event_queue(event_queue);
}

void draw_screen_keyboard()
{
    for (overlay_button const & bt : buttons)
    {
        int x = bt._x * 40 + 5;
        int y = bt._y * 40 + 5;
        al_draw_filled_rectangle(x,y,x+30,y+30, COLOR_GRAY);
        al_draw_text(font, FOREGROUND_COLOR, x + 15, y-20,  ALLEGRO_ALIGN_CENTER, bt._str);   
    }
}

uint32_t mix_col(uint32_t a, uint32_t b, uint8_t fade)
{
    return(((fade * ((b >> 0)  & 0xFF) + (255 - fade) * (0xFF & (a >> 0 ))) / 255) << 0)
        | (((fade * ((b >> 8)  & 0xFF) + (255 - fade) * (0xFF & (a >> 8 ))) / 255) << 8)
        | (((fade * ((b >> 16) & 0xFF) + (255 - fade) * (0xFF & (a >> 16))) / 255) << 16);
}

int main(int argc, const char* argv[]){
    std::string print_plan ("--print_plan");
    std::string help ("--help");
    for (int i=0;i<argc;++i){
        if (print_plan.compare(argv[i]) == 0){
            int player = atoi(argv[i+1]);
            int slots = atoi(argv[i+2]);
            int rounds = atoi(argv[i+3]);
            int equal_slots = atoi(argv[i+4]);
            if ((rounds*slots)%player != 0)
                std::cout<<"WARNUNG: Nicht alle Spieler koennen gleich oft spielen"<<std::endl;
            if (slots%equal_slots != 0)
                std::cout<<"WARNUNG: Bahnen muessen vielfaches von gleichschweren Bahnen sein"<<std::endl;
            approximate_tournee_plan_creator creator(player, slots, rounds, equal_slots);
            tournee_plan* plan = creator.get_plan();
            creator.start_creation();
            while (!creator.is_solved()){
                boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
            }
            plan->print();
            std::ofstream myfile;
            myfile.open (argv[i+5]);
            plan->print_html(myfile);
            myfile.close();

            return 0;
        }
        if (help.compare(argv[i]) == 0){
            std::cout<<"--print_plan <player> <slots> <rounds> <equal_slots>"<<std::endl;
        }
    }
    /*tournee_plan_creator* creator = new approximate_tournee_plan_creator(12, 4, 21, 2);
    creator->create_plan();
    tournee_plan* plan = creator->get_plan();
    plan->print();
    return 0;*/
    InputHandler input;
    ALLEGRO_DISPLAY* display = init(input);
    if (!display)
        return -1;
    boost::thread t1(allegro_input_task, &input);
#ifdef RASPBERRY_PI
    if (wiringPiSetup() == -1)
        throw std::runtime_error("No wiring pi");
    for (size_t i = 0; i < 4; ++i)
    {
        pinMode(opt._input_pin[i], INPUT);
    }
    global_input = &input;
    void (*interrupt_functions[4])(void) = {&sensor_interrupt_input_task<0>,&sensor_interrupt_input_task<1>,&sensor_interrupt_input_task<2>,&sensor_interrupt_input_task<3>};
    for (size_t i = 0; i < 4; ++i)
    {
        if (wiringPiISR(opt._input_pin[i], INT_EDGE_RISING, interrupt_functions[i]) < 0)
        {
            std::cout << "No interrupt setup, activating polling fallback" << std::endl;
            boost::thread polling_thread(sensor_poll_input_task, &input);
            break;
        }
    }
#endif
    led_stripe_t led_stripe;
    splash_screen(input, led_stripe);
    input.destroy();
    al_destroy_display(display);
    server.stop();
    return 0;
}

race_item *global_race;

ALLEGRO_COLOR al_map_rgb(size_t color)
{
    return al_map_rgb(color >> 16, color >> 8, color);
}

void print_table(race_item const & race, std::ostream & out)
{
    nanotime_t current_time = QueryPerformanceCounter();
    out << "Place";
    int time_format = 1;
    for (race_data_item const &race_member : race.members)
    {
        out << '\t';
        int place = race.get_place(race_member);
        if (place != -1){
            out << place;
        }
    }
    out << "\nRound";
    for (race_data_item const &race_member : race.members)
    {
        out << '\t' << race.get_round(race_member);
    }
    out << "\nAll";
    for (race_data_item const &race_member : race.members)
    {
        out << '\t';
        bool race_started = current_time > race.race_start_time;

        if (race_started){
            print_time(time_format, (race.has_finished(race_member) ? race_member.get_absolut_last_time() : current_time) - race.race_start_time, out);
        }else{
            print_time(time_format, std::numeric_limits<nanotime_t>::max(), out);
        }
    }
    out << "\nTime";
    for (race_data_item const &race_member : race.members)
    {
        out << '\t';
        if (race.get_round(race_member) < 1 || race.has_finished(race_member)){
            print_time( time_format, std::numeric_limits<nanotime_t>::max(), out);
        }else{
            print_time( time_format, current_time - race_member.get_absolut_last_time(),out);
        }
    }
    out << "\nLast";
    for (race_data_item const &race_member : race.members)
    {
        print_time( time_format, race_member.get_relative_last_time(), out << '\t');
    }
    out << "\nBest";
    for (race_data_item const &race_member : race.members)
    {
        print_time( time_format, race_member.get_best_time(),out << '\t');
    }
}

ALLEGRO_DISPLAY * init(InputHandler & handler){
    //OpenPortTalk();
    if(!al_init()) {
         std::cerr<<"failed to initialize allegro!"<<std::endl;
         return nullptr;
    }
    al_init_primitives_addon();
    al_init_font_addon();
    al_init_ttf_addon();
    al_install_keyboard();
    al_install_joystick();
    al_install_mouse();
    if(!al_install_audio()){
        fprintf(stderr, "failed to initialize audio!\n");
        return nullptr;
    }
    if(!al_init_acodec_addon()){
        fprintf(stderr, "failed to initialize audio codecs!\n");
        return nullptr;
    }
    if (!al_reserve_samples(20)){
        fprintf(stderr, "failed to reserve samples!\n");
        return nullptr;
    }
    al_init_image_addon();
    al_set_new_display_option(ALLEGRO_VSYNC,0, ALLEGRO_SUGGEST);
#ifdef RASPBERRY_PI
    al_set_new_display_flags(ALLEGRO_FULLSCREEN);
#endif
    frequency = QueryPerformanceFrequency();

    //ALLEGRO_MONITOR_INFO info;
    //al_get_monitor_info(0, &info);
    //ALLEGRO_DISPLAY *display = al_create_display(info.x2 - info.x1, info.y2 - info.y1);
    ALLEGRO_DISPLAY *display = al_create_display(800, 480);
    font_small = al_load_ttf_font("fonts/courbd.ttf",30,0 );
    font = al_load_ttf_font("fonts/courbd.ttf",50,0 );
    font_big = al_load_ttf_font("fonts/courbd.ttf",70,0 );
    font_control = al_load_ttf_font("fonts/courbd.ttf",50,0 );
    display_height = al_get_display_height(display);
    display_width = al_get_display_width(display);
    std::ifstream optionfile("options.txt");
    optionfile >> opt;
    optionfile.close();
    FOREGROUND_COLOR    = al_map_rgb(opt._color_foreground);
    BACKGROUND_COLOR    = al_map_rgb(opt._color_background);
    SELECTION_COLOR     = al_map_rgb(opt._color_selection);
    ERROR_COLOR         = al_map_rgb(opt._color_error);
    COLOR_RED           = al_map_rgb(255,0,0);
    COLOR_GRAY          = al_map_rgb(128,128,128);
    COLOR_GREEN         = al_map_rgb(0,255,0);
    COLOR_YELLOW        = al_map_rgb(255,255,0);

    ALLEGRO_PATH* path = al_get_standard_path(ALLEGRO_RESOURCES_PATH);
    al_change_directory(al_path_cstr(path,ALLEGRO_NATIVE_PATH_SEP));
    
    for (size_t i = 0; i < 4; ++i)
    {
        std::string str = "images/car"+std::to_string(i)+ ".png";
        car_bitmaps[i]=al_load_bitmap(str.c_str());
        assert(car_bitmaps);
    }
    
    splash_bitmap = al_load_bitmap("images/splash.png");
    
    sample          = al_load_sample( "sounds/beep.wav" );
    sample2         = al_load_sample( "sounds/beep2.wav" );
    sample3         = al_load_sample( "sounds/beep3.wav" );
    sample4         = al_load_sample( "sounds/beep4.wav" );
    sample_applause = al_load_sample( "sounds/beep4.wav");
    sample_start0   = al_load_sample( "sounds/start.wav" );
    sample_start1   = al_load_sample( "sounds/start2.wav" );

    
    if (!font){
        std::cerr<<"Could not load font."<<std::endl;
        return nullptr;
    }
    server.setCommandExecutor([&handler](std::string str, std::ostream & stream)
    {
        stream << "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-type: text/html\r\n\r\n";
        HTML::LogHtml log(stream);
        stream << "<head><meta http-equiv=\"refresh\" content=\"1\" /></head>" << std::endl;
        stream << "<head><meta http-equiv=\"refresh\" content=\"1;url=/home\"></head>" << std::endl;
        stream << "<font size=\"7\">"<<std::endl;
        if (global_race != nullptr)
        {
            log << HTML::begintable(1, '\t','\n',false);
            print_table(*global_race, log);
            log << HTML::endtable();
        }
        log << HTML::begintable(1, '\t','\n',false);
        
        log << HTML::beginlink("./bescape") << "escape";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bup") << "up";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./benter") << "enter";
        log << HTML::endlink() << '\n';
        log << HTML::beginlink("./bleft") << "left";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bdown") << "down";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bright") << "right";
        log << HTML::endlink() << '\n';
        log << HTML::endtable();
        
        
        log << HTML::begintable(1, '\t','\n',false);
        log << HTML::beginlink("./bsensor1") << "+";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bsensor2") << "+";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bsensor3") << "+";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bsensor4") << "+";
        log << HTML::endlink() << std::endl;
        log << HTML::beginlink("./bnsensor1") << "-";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bnsensor2") << "-";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bnsensor3") << "-";
        log << HTML::endlink() << '\t';
        log << HTML::beginlink("./bnsensor4") << "-";
        log << HTML::endtable();
        std::flush(log);
        stream << "</font>" << std::endl;
        
        if (str.find("bleft")     != std::string::npos) {handler.call(InputEvent(DLEFT));}
        if (str.find("bright")    != std::string::npos) {handler.call(InputEvent(DRIGHT));}
        if (str.find("bup")       != std::string::npos) {handler.call(InputEvent(DUP));}
        if (str.find("bdown")     != std::string::npos) {handler.call(InputEvent(DDOWN));}
        if (str.find("benter")    != std::string::npos) {handler.call(InputEvent(DENTER));}
        if (str.find("bescape")   != std::string::npos) {handler.call(InputEvent(DBACK));}
        if (str.find("bsensor1")  != std::string::npos) {handler.call(InputEvent(DSENSOR0));}
        if (str.find("bsensor2")  != std::string::npos) {handler.call(InputEvent(DSENSOR1));}
        if (str.find("bsensor3")  != std::string::npos) {handler.call(InputEvent(DSENSOR2));}
        if (str.find("bsensor4")  != std::string::npos) {handler.call(InputEvent(DSENSOR3));}
        if (str.find("bnsensor1") != std::string::npos) {handler.call(InputEvent(DNSENSOR0));}
        if (str.find("bnsensor2") != std::string::npos) {handler.call(InputEvent(DNSENSOR1));}
        if (str.find("bnsensor3") != std::string::npos) {handler.call(InputEvent(DNSENSOR2));}
        if (str.find("bnsensor4") != std::string::npos) {handler.call(InputEvent(DNSENSOR3));}
    });
    server.start();
   
#ifdef LEDSTRIPE
    ws2811 = new ws2811_t();
    ws2811->freq = WS2811_TARGET_FREQ;
    ws2811->dmanum = 10;
    ws2811->channel[0].gpionum = 18;
    ws2811->channel[0].count = 21;
    ws2811->channel[0].invert = 0;
    ws2811->channel[0].brightness = 255;
    ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
    ws2811->channel[1].gpionum = 0;
    ws2811->channel[1].count = 0;
    ws2811->channel[1].invert = 0;
    ws2811->channel[1].brightness = 0;
    ws2811_init(ws2811);
    std::fill(ws2811->channel[0].leds, ws2811->channel[0].leds + ws2811->channel[0].count, opt._led_stripe_white);
    ws2811_render(ws2811);
#endif
   return display;
}

int splash_screen(InputHandler & input, led_stripe_t & led_stripe){
    while (true)
    {
        if (splash_bitmap)
        {
            float w = al_get_bitmap_width(splash_bitmap), h =al_get_bitmap_height(splash_bitmap); 
            //float sh = column_width * h * 0.9 / w;
            al_draw_scaled_bitmap(splash_bitmap, 0, 0, w,h, 0, 0, display_width, display_height, 0);
            al_flip_display();
            InputEvent event = input.wait_for_event();
            switch(event._dest)
            {
                case DUP:  
                case DDOWN:
                case DLEFT:
                case DRIGHT:
                case DENTER:menu(input, led_stripe);break;
                case DBACK: return 0;
                default: break;
            }
        }
        else
        {
            menu(input, led_stripe);
        }
    }
}

int menu(InputHandler & input, led_stripe_t & led_stripe){
    int selected = 0;
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        al_draw_text(font, selected == 0 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 20,ALLEGRO_ALIGN_CENTRE, "Schnelles Rennen");
        al_draw_text(font, selected == 1 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 120,ALLEGRO_ALIGN_CENTRE, "Tournier");
        al_draw_text(font, selected == 2 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 220,ALLEGRO_ALIGN_CENTRE, "Options");
        al_draw_text(font, selected == 3 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 320,ALLEGRO_ALIGN_CENTRE, "Update");
        al_draw_text(font, selected == 4 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 420,ALLEGRO_ALIGN_CENTRE, "Shutdown");
        draw_screen_keyboard();
        al_flip_display();
        
        InputEvent event = input.wait_for_event();
        switch(event._dest)
        {
            case DUP:  selected = (selected + 4) % 5;break;
            case DDOWN:  selected = (selected + 1) % 5; break;
            case DENTER:
            {
                switch (selected){
                    case 0: fast_race(input, led_stripe);break;
                    case 1: tournee(input);break;
                    case 2: options(input);break;
                    case 3: if (system("./update.sh"))          {std::cerr<< "Result not null" << std::endl;};break;
                    case 4: if (system("systemctl poweroff"))   {std::cerr<< "Result not null" << std::endl;};break;
                } 
                break;
            }
            case DBACK: return 0;
            default: break;
        }
    }
}

int tournee(InputHandler & input){
    int selected = 0;
    int player = 14, slots = 4, rounds = 21, equal_slots = 2;
    size_t row_height = display_height / 6;
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        al_draw_textf(font, selected == 0 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height,    ALLEGRO_ALIGN_CENTRE, "Spieler %d", player);
        al_draw_textf(font, selected == 1 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * 2,ALLEGRO_ALIGN_CENTRE, "Bahnen %d", slots);
        al_draw_textf(font, selected == 2 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * 3,ALLEGRO_ALIGN_CENTRE, "Runden %d", rounds);
        al_draw_textf(font, selected == 3 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * 4,ALLEGRO_ALIGN_CENTRE, "Gleichschwere Bahnen %d", equal_slots);
        al_draw_text (font, selected == 4 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * 5,ALLEGRO_ALIGN_CENTRE, "Plan erstellen");
        if ((rounds*slots)%player != 0)
            al_draw_text(font, ERROR_COLOR, display_width/2, row_height * 6, ALLEGRO_ALIGN_CENTRE, "WARNUNG: Nicht alle Spieler koennen gleich oft spielen");
        else if (slots%equal_slots != 0)
            al_draw_text(font, ERROR_COLOR, display_width/2, row_height * 6, ALLEGRO_ALIGN_CENTRE, "WARNUNG: Bahnen muessen vielfaches von gleichschweren Bahnen sein");
        draw_screen_keyboard();
        al_flip_display();
        InputEvent event = input.wait_for_event();
        switch(event._dest)
        {
            case DUP: selected = (selected + 4) % 5; break;
            case DDOWN:  selected = (selected + 1) % 5;break;
            case DRIGHT: {
                switch(selected){
                    case 0: ++player;       break;
                    case 1: ++slots;        break;
                    case 2: ++rounds;       break;
                    case 3: ++equal_slots;  break;
                }break;}
            case DLEFT:{
                switch(selected){
                    case 0: player      = std::max(0, player-1);break;
                    case 1: slots       = std::max(0, slots -1);break;
                    case 2: rounds      = std::max(0, rounds-1);break;
                    case 3: equal_slots = std::max(1, equal_slots - 1);break;
                }break;
            }
            case DBACK:return 0;
            case DENTER: 
            {
                switch (selected){
                    case 4:
                        approximate_tournee_plan_creator creator(player, slots, rounds, equal_slots);
                        tournee_plan* plan = creator.get_plan();
                        creator.start_creation();
                        show_tournee_plan(*plan, &creator);
                        creator.stop_creation();
                        break;
                }
                break;
            }
            default: break;
        }
    }    
}

int options(InputHandler & input){
    size_t selected = 0;
    size_t row_height = display_height / 6;
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        for (size_t i = 0; i < 4; ++i)
        {
            al_draw_textf(font, selected == i ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * (i + 1),ALLEGRO_ALIGN_CENTRE, "InputPin %u %u", static_cast<uint32_t>(i), static_cast<uint32_t>(opt._input_pin[i]));
        }
        al_draw_textf(font, selected == 4 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * (5),ALLEGRO_ALIGN_CENTRE, "Save");
        
        draw_screen_keyboard();
        al_flip_display();
        InputEvent event = input.wait_for_event();
        switch (event._dest)
        {
            case DUP:   selected = (selected + 5) % 6; break;
            case DDOWN: selected = (selected + 1) % 6;break;
            case DRIGHT:if (selected < 4){++opt._input_pin[selected];}break;
            case DLEFT: if (selected < 4){--opt._input_pin[selected];}break;                    
            case DBACK:return 0;
            case DENTER:
            {
                if (selected == 4)
                {
                    std::ofstream optionfile("options.txt");
                    optionfile << opt;
                    optionfile.close();
                }
            }
            default: break;
        }
    }    
}

int show_tournee_plan(tournee_plan& tp, tournee_plan_creator* tpc){
    ALLEGRO_KEYBOARD_STATE key_state;
    uint16_t left_border=40;
    uint16_t right_border=40;
    uint16_t top_border=50;
    uint16_t bottom_border=20;
    uint16_t column_width = (display_width-left_border-right_border) / (tp.get_rounds());
    uint16_t row_height = (display_height-top_border-bottom_border) / (tp.get_slots()+1);
    for (int frame_count = 0;true;frame_count++){
        al_get_keyboard_state(&key_state);
        al_flip_display();
        al_clear_to_color(BACKGROUND_COLOR);
        
        if (tpc != NULL){
            al_draw_textf(font, FOREGROUND_COLOR, 0, 0,ALLEGRO_ALIGN_LEFT, "Fehler: %d", tpc->get_best_rating());
            if (tpc->is_running())
                al_draw_text(font, al_map_rgb(std::max(std::min(std::abs((frame_count*5)%1024-512), 255),0),0,0), 400, 0,ALLEGRO_ALIGN_LEFT, "Calculating");
        }
        for (uint32_t i=0;i<tp.get_rounds();i++){
            al_draw_textf(font, FOREGROUND_COLOR, left_border + column_width * (i+1), top_border,ALLEGRO_ALIGN_CENTRE, "%u", i+1);
            al_draw_line(left_border + column_width * (i+0.5f), top_border, left_border + column_width * (i+0.5f), display_height-bottom_border, FOREGROUND_COLOR, 1);
        }
        for (uint32_t i=0;i<tp.get_slots();i++){
            al_draw_line(left_border, top_border+row_height * (i+0.5f), display_width-right_border, top_border+row_height * (i+0.5f), FOREGROUND_COLOR, 1);
            al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border+row_height * (i+1),ALLEGRO_ALIGN_CENTRE, "%u", i+1);
            for (uint32_t j=0;j<tp.get_rounds();j++){
                al_draw_textf(font, FOREGROUND_COLOR, left_border + column_width * (j+1), top_border + row_height * (i+1),ALLEGRO_ALIGN_CENTRE, "%u", (uint32_t)tp.get_player_at(j, i)+1);
            }
        }
        if (al_key_down(&key_state, ALLEGRO_KEY_ESCAPE)){return 0;}
    }
}

/*int tournee_race(tournee_plan tp){
    
}*/

int fast_race(InputHandler & input, led_stripe_t & led_stripe){
    int selected = 0;
    std::vector<bool> activated_player(opt._max_player);
    uint8_t rounds = 3;
    uint8_t player = 0;
    
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        uint32_t display_width_half = display_width / 2;
        for (size_t i = 0; i < opt._max_player; ++i)
        {
            ALLEGRO_COLOR color = selected == 0 && i == player ? SELECTION_COLOR : COLOR_GRAY;
            if (activated_player[i]){al_draw_filled_circle((i * display_width + display_width_half) / (opt._max_player), 100, 30, color);}
            else                    {al_draw_circle       ((i * display_width + display_width_half) / (opt._max_player), 100, 30, color, 5);}
        }
        //al_draw_textf(font, selected == 0 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width_half, 100,ALLEGRO_ALIGN_CENTRE, "Spieler: %d", player);
        al_draw_textf(font, selected == 1 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width_half, 200,ALLEGRO_ALIGN_CENTRE, "Runden:  %d", rounds);
        al_draw_text(font, selected == 2 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width_half, 300,ALLEGRO_ALIGN_CENTRE, "START");
        draw_screen_keyboard();
        al_flip_display();
        InputEvent event = input.wait_for_event();
        switch(event._dest)
        {
            case DUP:   selected = (selected + 2) % 3;break;
            case DDOWN: selected = (selected + 1) % 3;break;
            case DRIGHT:
            {
                switch(selected){
                    case 0: player = std::min(static_cast<uint8_t>(opt._max_player - 1), static_cast<uint8_t>(player + 1));break;
                    case 1: rounds++;break;
                }
                break;
            }
            case DLEFT:
            {
                switch(selected){
                    case 0: player = std::max(0,player-1);break;
                    case 1: rounds = std::max(0,rounds-1);break;
                }
                break;
            }
            case DENTER:
            {
                switch (selected){
                    case 0: activated_player[player] = !activated_player[player];break;
                    case 2: while (racing_loop(rounds, activated_player, led_stripe, input) == 1);break;
                    default: break;
                }
                break;
            }
            case DBACK:{return 0;}
            default:break;
        }
    }
}

/*int div_floor(int a, int b)
{
    int res = a / b;
    int rem = a % b;
    // Correct division result downwards if up-rounding happened,
    // (for non-zero remainder of sign different than the divisor).
    int corr = (rem != 0 && ((rem < 0) != (b < 0)));
    return res - corr;
}*/

ALLEGRO_BITMAP *load_bitmap_at_size(const char *filename, int w, int h)
{
  ALLEGRO_BITMAP *resized_bmp, *loaded_bmp, *prev_target;

  // 1. create a temporary bitmap of size we want
  resized_bmp = al_create_bitmap(w, h);
  if (!resized_bmp) return NULL;

  // 2. load the bitmap at the original size
  loaded_bmp = al_load_bitmap(filename);
  if (!loaded_bmp)
  {
     al_destroy_bitmap(resized_bmp);
     return NULL;
  }

  // 3. set the target bitmap to the resized bmp
  prev_target = al_get_target_bitmap();
  al_set_target_bitmap(resized_bmp);

  // 4. copy the loaded bitmap to the resized bmp
  al_draw_scaled_bitmap(loaded_bmp,
     0, 0,                                // source origin
     al_get_bitmap_width(loaded_bmp),     // source width
     al_get_bitmap_height(loaded_bmp),    // source height
     0, 0,                                // target origin
     w, h,                                // target dimensions
     0                                    // flags
  );

  // 5. restore the previous target and clean up
  al_set_target_bitmap(prev_target);
  //al_destroy_loaded_bmp(loaded_bmp);

  return resized_bmp;      
}

void load_bitmap_onto_target(const char *filename)
{
  ALLEGRO_BITMAP *loaded_bmp;
  const int w = al_get_bitmap_width(al_get_target_bitmap());   
  const int h = al_get_bitmap_height(al_get_target_bitmap());

  // 1. load the bitmap at the original size
  loaded_bmp = al_load_bitmap(filename);
  if (!loaded_bmp) return;

  // 2. copy the loaded bitmap to the resized bmp
  al_draw_scaled_bitmap(loaded_bmp,
     0, 0,                                // source origin
     al_get_bitmap_width(loaded_bmp),     // source width
     al_get_bitmap_height(loaded_bmp),    // source height
     0, 0,                                // target origin
     w, h,                                // target dimensions
     0                                    // flags
  );

  // 3. cleanup
  al_destroy_bitmap(loaded_bmp);
}

int divf(int64_t n, uint64_t d) {
    return n >= 0 ? n / d : -1 - (-1 - n) / d;
}

struct colored_vertex_t
{
    float _x, _y;
    float _r, _g, _b;
};

#define BUFFER_OFFSET(i) ((void*)(i))
struct firework_drawer_t
{
    std::vector<ALLEGRO_VERTEX> varrays;
    std::vector<ALLEGRO_VERTEX> varrayl;
void operator() (firework_t const & firework)
{
    for (auto const & rocket : firework._rockets)
    {
        varrayl.push_back(ALLEGRO_VERTEX({static_cast<float>(rocket._rocket._position[0] >> 16), static_cast<float>(480 - (rocket._rocket._position[1] >> 16)), 0, 0, 0, COLOR_YELLOW}));
       
        //draws the trails.
        for (std::array<int32_t, 2> const & particle : rocket._trail)
        {
            int16_t x = particle[0] >> 16, y = 480 - (particle[1] >> 16);
            if (x >= 0 && x <= 800 && y >= 0 && y <= 480)
            {
                varrays.push_back(ALLEGRO_VERTEX({static_cast<float>(x), static_cast<float>(y), 0, 0, 0, COLOR_YELLOW}));
            }
        }
    }

    // draws the exploding particles.
    for (explosion_t const & explosion : firework._explosions)
    {
        ALLEGRO_COLOR col = al_map_rgb(explosion._color._r, explosion._color._g, explosion._color._b);
        for (particle_t const & particle : explosion._particles)
        {
            int16_t x = particle._position[0] >> 16, y = 480 - (particle._position[1] >> 16);
            if (x >= 0 && x <= 800 && y >= 0 && y <= 480)
            {
                varrayl.push_back(ALLEGRO_VERTEX({static_cast<float>(x), static_cast<float>(y), 0, 0, 0, col}));
            }
        }
    }
    
    glPointSize(2);
    glVertexPointer(2, GL_FLOAT,   sizeof(ALLEGRO_VERTEX), varrayl.data());
    glColorPointer(3, GL_FLOAT, sizeof(ALLEGRO_VERTEX), &varrayl.data()->color);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_POINTS, 0, varrayl.size());
    
    glPointSize(1);
    glVertexPointer(2, GL_FLOAT,   sizeof(ALLEGRO_VERTEX), varrays.data());
    glColorPointer(3, GL_FLOAT, sizeof(ALLEGRO_VERTEX), &varrays.data()->color);
    glDrawArrays(GL_POINTS, 0, varrays.size());
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    varrays.clear();
    varrayl.clear();
}
};

int racing_loop (uint8_t runden, std::vector<bool> const & activated_player, led_stripe_t & led_stripe, InputHandler &input){
    performance_counter perfc(500);
    race_item race(runden);
    global_race = &race;
    std::vector<uint8_t> slot_to_player(activated_player.size());
    std::vector<player_t> player(std::count(activated_player.begin(), activated_player.end(),true));
    if (player.size() == 0)
    {
        return 0;
    }
    for (size_t i = 0; i < activated_player.size(); ++i)
    {
        if (activated_player[i])
        {
            race.members.emplace_back(race_data_item(player[slot_to_player[i] = race.members.size()]));
            race.members.back()._slot = i;
        }
        else
        {
            slot_to_player[i] = std::numeric_limits<uint8_t>::max();
        }
    }
    uint16_t bottom_border = 0;
    uint16_t table_top = 120;
    uint16_t row_height = (display_height-table_top-bottom_border)/5;//Todo
    uint16_t left_border = 5;
    uint16_t right_border = 5;
    uint16_t column_width = (display_width-left_border-right_border)/(race.members.size());
    table_top += 30;
    debug_overlay_t debug_overlay;
    
    size_t visible_lights = 0;
    size_t time_format = 1;
    race.race_start_time = QueryPerformanceCounter() + frequency * 7;
    uint8_t control = 0;
    
    std::function<void(InputEvent const & event)> callback = [&race, &control, slot_to_player](InputEvent const & event){
        if (event._dest >= DSENSOR0 && event._dest < DSENSOR0 + slot_to_player.size() && slot_to_player[event._dest - DSENSOR0] != std::numeric_limits<uint8_t>::max())
        {
            bool race_started = event._when > race.race_start_time;
            if (!race_started)
            {
                al_play_sample(sample4, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);                
            }
            else
            {
                race_data_item & item = race.members[slot_to_player[event._dest - DSENSOR0]];
                if (!race.has_finished(item))
                {
                    race.add_time(item,event._when);
                    al_play_sample(race.has_finished(item) ? sample_applause : sample3, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
                }
            }
        }
        if (event._dest >= DNSENSOR0 && event._dest < DSENSOR0 + slot_to_player.size() && slot_to_player[event._dest - DNSENSOR0] != std::numeric_limits<uint8_t>::max())
        {
            race.members[slot_to_player[event._dest - DNSENSOR0]].pop_time();
        }
        else if (event._dest == DBACK)
        {
            control = 2;
        }
    };
    input.register_callback(callback);
    
    std::bernoulli_distribution create_new_rocket(0.2);
    uint8_t refresh_rate = 30;
    firework_t firework(800<<16, 480<<16, 1600 / refresh_rate);
    std::vector<bool> finished_last_frame(race.member_count(), false);
    // al_draw_text(font, al_map_rgb( 255, 0, 0), 200, 100, ALLEGRO_ALIGN_LEFT,"CONGRATULATIONS" );
    firework_drawer_t draw_firework;
    std::fill(led_stripe.begin(), led_stripe.end(), opt._led_stripe_prestart);
    led_stripe._dirty = true;
    std::vector<size_t> firework_ambient(led_stripe.size() * 3);
    while (true){
        al_flip_display();
        al_clear_to_color(BACKGROUND_COLOR);
        nanotime_t current_time = QueryPerformanceCounter();
        bool race_started = current_time > race.race_start_time;
        
        if (control){break;}
        draw_firework(firework);
        firework();
        if (race.all_finished())
        {
            if (create_new_rocket(firework._gen))
            {
                firework.create_rocket();
                al_play_sample(current_time % 2 ? sample_start0 : sample_start1, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
            }
            
            for (explosion_t & explosion : firework._explosions)
            {
                for (particle_t & particle : explosion._particles)
                {
                    int32_t led = (particle._position[0] >> 16) * led_stripe.size() / 800;
                    if (0 <= led && led < static_cast<int32_t>(led_stripe.size()))
                    {
                        for (size_t i = 0; i < 3; ++i)
                        {
                            firework_ambient[led * 3 + i] += explosion._color[i];
                        }        
                    }
                }
            }
            size_t max = *std::max_element(firework_ambient.begin(), firework_ambient.end());
            if (max != 0)
            {
                for (size_t i = 0; i < led_stripe.size(); ++i)
                {
                    led_stripe[i] = 0;
                    for (size_t j = 0; j < 3; ++j)
                    {
                        led_stripe[i] |= (firework_ambient[i * 3 + j] * 0xFF / max) << (j * 8);
                        firework_ambient[i * 3 + j] = 0;
                    }
                }
            }
            led_stripe._dirty = true;
        }
       
        int32_t ylightpos = 50 - std::max(0,static_cast<int32_t>((current_time - race.race_start_time - 2000000) / 20000));
        for (size_t i=0;i<race.member_count();i++)
        {
            race_data_item &race_member = race.members[i];
            int round = race.get_round(race_member);
            size_t column_pos = left_border+column_width*(i) + column_width / 2;
            bool finished = race.has_finished(race_member);
            if (!race.all_finished() && finished)
            {
                size_t blinks = (current_time - race_member.get_absolut_last_time())/1000000;
                if(blinks < 6)
                {
                    std::fill(led_stripe.begin() + race_member._slot * led_stripe.size() / activated_player.size(), led_stripe.begin() + (race_member._slot + 1) * led_stripe.size() / activated_player.size(), (blinks % 2) * opt._led_stripe_white);
                }
            }
            if (finished && !finished_last_frame[i])
            {
                finished_last_frame[i] = true;
                firework._explosions.emplace_back(300, std::array<int32_t, 2>({0x10000*static_cast<int32_t>(column_pos + column_width / 2),  0x10000*(480 - (table_top + 30))}), std::array<int32_t, 2>({0,0}), 10000, firework._timestep, firework._gen);
            }
            {
                ALLEGRO_BITMAP *bitmap = car_bitmaps[race_member._slot];
                int current_place = race_member.round_times.size() == 0 ? race.member_count() : race.get_current_place(race_member);
                float w = al_get_bitmap_width(bitmap), h =al_get_bitmap_height(bitmap); 
                float sh = 0.25 * display_width * h * 0.9 / w;
                al_draw_scaled_bitmap(bitmap, 0, 0, w,h, column_pos - display_width * 0.25 * 0.45, current_place * 40 - sh/2+5, display_width * 0.25 * 0.9, sh, 0);
            }
            al_draw_text(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 0,  ALLEGRO_ALIGN_LEFT, race_member.belongs_to_player->first_name.c_str());
            al_draw_textf(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 1, ALLEGRO_ALIGN_RIGHT, "%d",round);
            al_draw_textf(font_small, FOREGROUND_COLOR, column_pos, table_top + row_height * 1+20, ALLEGRO_ALIGN_LEFT, "/%d",runden);

            draw_time(font, finished || race_member.get_best_time_index() + 1== race_member.round_times.size() ? SELECTION_COLOR : FOREGROUND_COLOR, column_pos, table_top + row_height * 2, ALLEGRO_ALIGN_CENTER, time_format, finished ? race_member.get_best_time() : race_member.get_relative_last_time());
            draw_time(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 3, ALLEGRO_ALIGN_CENTER, time_format, race_member.get_absolut_last_time() == std::numeric_limits<nanotime_t>::max() ? std::numeric_limits<nanotime_t>::max() : (race_member.get_absolut_last_time() - race.race_start_time));
            
            draw_ptime(font_small, FOREGROUND_COLOR, column_pos + 10, table_top + row_height * 4, ALLEGRO_ALIGN_CENTER, time_format, race.timediff_to_first(race_member));
            //draw_time(FOREGROUND_COLOR, column_pos, table_top + row_height * 5, time_format, race_member.get_best_time());
            if (finished)
            {
                int place = race.get_place(race_member);
                if (place != -1){
                    al_draw_filled_circle(column_pos, table_top + row_height * 0+30, 30, al_map_rgb(200,200,200));
                    al_draw_textf(font_big, COLOR_RED, column_pos, table_top + row_height * 0, ALLEGRO_ALIGN_CENTRE, "%d",place);
                }
            }
        }
        int32_t color_fade = static_cast<int32_t>((current_time - race.race_start_time - 2000000) / 20000); 
        if (color_fade >= 0 && color_fade < 256)
        {
            std::fill(led_stripe.begin(), led_stripe.end(), mix_col(opt._led_stripe_start,  opt._led_stripe_white, color_fade));
            led_stripe._dirty = true;
        }
        if (ylightpos >= -50)
        {
            al_draw_filled_rectangle (display_width/2-250,0,display_width/2+250,ylightpos + 50, FOREGROUND_COLOR);
            size_t tmp = 7 + divf(current_time - race.race_start_time,frequency);
            if (visible_lights != tmp)
            {
                if (tmp == 7)
                {
                    std::fill(led_stripe.begin(), led_stripe.end(), opt._led_stripe_start);
                    led_stripe._dirty = true;
                }
                if (tmp < 6)        {al_play_sample(sample, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);}
                else if (tmp == 7)  {al_play_sample(sample2, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);}
            }
            visible_lights = tmp;
            for (size_t i=0;i<5;i++){
                al_draw_filled_circle(100*i+display_width/2-200, ylightpos, 40, race_started ? COLOR_GREEN : visible_lights > i ? COLOR_RED : FOREGROUND_COLOR);//TODO images?
            }
        }
        led_stripe.render();
        if (opt._debug_overlay)
        {    
            debug_overlay.draw(led_stripe);
        }
        perfc.poll();
        //al_draw_textf(font, FOREGROUND_COLOR, 10, 10, ALLEGRO_ALIGN_LEFT, "FPS:%d",perfc.frames_per_second());
    }
    input.unregister_callback(callback);
    global_race = nullptr;
    return control;
}

void print_time(size_t format, nanotime_t time, std::ostream & out)
{
    if (time == std::numeric_limits<nanotime_t>::max())
    {
        switch(format){
            case 0: out << "--:--.--";break;
            case 1: out << "--.--";break;
            case 2: out << "--.-";break;
        }
    }
    else
    {
        switch(format){
            case 0: out << std::setw(2) << std::setfill('0') << unsigned(time/frequency/60) << ':' << unsigned(time/frequency%60) << ':' << unsigned((time*100/frequency)%100);break;
            case 1: out << std::setw(2) << std::setfill('0') << unsigned(time/frequency) << '.' << unsigned((time*100/frequency)%100);break;
            case 2: out << std::setw(2) << std::setfill('0') << unsigned(time/frequency) << '.' << unsigned((time*10/frequency)%10);break;
        }
    }
}

void draw_time(ALLEGRO_FONT const * font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time){
    if (time == std::numeric_limits<nanotime_t>::max())
    {
        switch(format){
            case 0: al_draw_textf(font, color, x, y,flags, "--:--.--");break;
            case 1: al_draw_textf(font, color, x, y,flags, "--.--");break;
            case 2: al_draw_textf(font, color, x, y,flags, "--.-");break;
        }
    }
    else
    {
        switch(format){
            case 0: al_draw_textf(font, color, x, y, flags, "%02u:%02u.%02u ",int(time/(frequency*60)), int((time/frequency)%60), int((time*100/frequency)%100));break;
            case 1: al_draw_textf(font, color, x, y, flags, "%02u.%02u ", int(time/frequency), int((time*100/frequency)%100));break;
            case 2: al_draw_textf(font, color, x, y, flags, "%02u.%01u ", int(time/frequency), int((time*10/frequency)%10));break;
        }
    }
}

void draw_ptime(ALLEGRO_FONT const * font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time){
    if (time == std::numeric_limits<nanotime_t>::max())
    {
        switch(format){
            case 0: al_draw_textf(font, color, x, y,flags, "+--:--.--");break;
            case 1: al_draw_textf(font, color, x, y,flags, "+--.--");break;
            case 2: al_draw_textf(font, color, x, y,flags, "+--.-");break;
        }
    }
    else
    {
        switch(format){
            case 0: al_draw_textf(font, color, x, y, flags, "+%02u:%02u.%02u ",int(time/(frequency*60)), int((time/frequency)%60), int((time*100/frequency)%100));break;
            case 1: al_draw_textf(font, color, x, y, flags, "+%02u.%02u ", int(time/frequency), int((time*100/frequency)%100));break;
            case 2: al_draw_textf(font, color, x, y, flags, "+%02u.%01u ", int(time/frequency), int((time*10/frequency)%10));break;
        }
    }
}
