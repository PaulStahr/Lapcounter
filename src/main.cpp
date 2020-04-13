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
#include "server.h"
#include "loghtml.h"
#include "tournee_plan.h"
#include "tournee_plan_approximation_creator.h"
#include "tournee_plan_creator.h"
#include "data.h"
#include "performance_counter.h"
#include "firework.h"
#ifdef RASPBERRY_PI
extern "C" {
#include <wiringPi.h>
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
ALLEGRO_SAMPLE *sample;
ALLEGRO_SAMPLE *sample2;
ALLEGRO_SAMPLE *sample3;
ALLEGRO_SAMPLE *sample4;

ALLEGRO_COLOR BACKGROUND_COLOR;
ALLEGRO_COLOR FOREGROUND_COLOR;
ALLEGRO_COLOR SELECTION_COLOR;
ALLEGRO_COLOR ERROR_COLOR;
ALLEGRO_COLOR COLOR_RED;
ALLEGRO_COLOR COLOR_GRAY;
ALLEGRO_COLOR COLOR_GREEN;
ALLEGRO_COLOR COLOR_YELLOW;
WebServer server;


int tournee_race(tournee_plan* tp);
void print_time(size_t format, nanotime_t time, std::ostream & out);
void draw_time(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
void draw_ptime(ALLEGRO_FONT const *font, ALLEGRO_COLOR const & color, float x, float y, int flags, size_t format, nanotime_t time);
nanotime_t frequency;
int display_height;
int display_width;
int show_tournee_plan(tournee_plan &tp, tournee_plan_creator* tpc);
//size_t input_pins[4]={0,2,3,4};
size_t input_pins[4]={4,3,2,0};


enum Destination {DLEFT, DRIGHT, DUP, DDOWN, DENTER, DBACK, DSENSOR0, DSENSOR1, DSENSOR2, DSENSOR3, DNSENSOR0, DNSENSOR1, DNSENSOR2, DNSENSOR3, DUNDEF};

class InputEvent
{
public:
    Destination _dest;
    nanotime_t _when;
private:
    
    public:
        InputEvent(Destination dest_, nanotime_t when_) : _dest(dest_), _when(when_){}
};

    
template<typename T, typename... U>
size_t getAddress(std::function<T(U...)> f) {
    typedef T(fnType)(U...);
    fnType ** fnPointer = f.template target<fnType*>();
    if (fnPointer == nullptr)
    {
        return 0;
    }
    return (size_t) *fnPointer;
}

bool operator == (std::function<void (InputEvent const &)> f, std::function<void (InputEvent const &)> g)
{
    return getAddress(f) == getAddress(g);
}

class InputHandler
{
private:
    std::mutex mtx;
    bool _is_valid;
    std::condition_variable cv;
    std::vector<std::function<void (InputEvent const &)> > callbacks;
    InputEvent last_event;
public:
    InputHandler() : _is_valid(true),last_event(DUNDEF, 0){}
    
    void register_callback(std::function<void (InputEvent const &)> callback)
    {
        callbacks.push_back(callback);
    }
    
    bool is_valid()
    {
        return _is_valid;
    }
    
    void unregister_callback(std::function<void (InputEvent const &)> callback)
    {
        callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
    }
    
    InputEvent wait_for_event()
    {
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck);
        return last_event;
    }
    
    void call(InputEvent const & event)
    {
        std::unique_lock<std::mutex> lck(mtx);
        last_event = event;
        cv.notify_all();
        for (std::function<void (InputEvent const &)> & callback : callbacks)
        {
            callback(event);
        }
    }
    
    void destroy()
    {
        _is_valid = false;
    }
};

ALLEGRO_DISPLAY* init(InputHandler &);
int menu(InputHandler &);
int fast_race(InputHandler &);
int tournee(InputHandler &);
int anzeige (uint8_t runden, uint8_t spieler, InputHandler & handler);
int options(InputHandler & input);

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
            bool current = digitalRead(input_pins[i])==1;
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

void allegro_input_task(InputHandler *handler)
{
    ALLEGRO_EVENT_QUEUE *event_queue = al_create_event_queue();
    al_register_event_source(event_queue, al_get_keyboard_event_source());
    al_register_event_source(event_queue, al_get_mouse_event_source());
    al_register_event_source(event_queue, al_get_joystick_event_source());
    //float lastaxispos[10];
    while (handler->is_valid())
    {
        ALLEGRO_EVENT ev;
        al_wait_for_event(event_queue, &ev);
        Destination dest = DUNDEF;
        size_t buttonSize = 40;
        std::cout << ev.type << std::endl;
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
            //std::cout << ev.joystick.stick * 2 + ev.joystick.axis << ' ' << ev.joystick.pos;
            //lastaxispos[ev.joystick.stick * 2 + ev.joystick.axis] = ev.joystick.pos;
        }
        if (ev.type == ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN)
        {
            std::cout << ev.joystick.button << std::endl;
            switch (ev.joystick.button)
            {
                case 0:  dest = DENTER;break;
                case 1:    dest = DBACK;break;
                case 2:  dest = DLEFT;break;
                case 3: dest = DRIGHT;break;
                case 4:dest = DBACK;break;
                case 5: dest = DENTER;break;
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
            //ev.key_shifts;
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
                /*std::cout << ev.keyboard.modifiers << std::endl;
                if (ev.keyboard.modifiers & ALLEGRO_KEYMOD_SHIFT)
                {
                    dest = static_cast<Destination>(dest + DNSENSOR0 - DSENSOR0);
                }*/
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
        pinMode(input_pins[i], INPUT);
    }
    global_input = &input;
    void (*interrupt_functions[4])(void) = {&sensor_interrupt_input_task<0>,&sensor_interrupt_input_task<1>,&sensor_interrupt_input_task<2>,&sensor_interrupt_input_task<3>};
    for (size_t i = 0; i < 4; ++i)
    {
        if (wiringPiISR(input_pins[i], INT_EDGE_RISING, interrupt_functions[i]) < 0)
        {
            std::cout << "No interrupt setup, activating polling fallback" << std::endl;
            boost::thread polling_thread(sensor_poll_input_task, &input);
            break;
        }
    }
#endif
    menu(input);
    input.destroy();
    al_destroy_display(display);
    server.stop();
    return 0;
}

race_item *global_race;

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
    if (!al_reserve_samples(5)){
        fprintf(stderr, "failed to reserve samples!\n");
        return nullptr;
    }
    al_init_image_addon();
    al_set_new_display_option(ALLEGRO_VSYNC,0, ALLEGRO_SUGGEST);
#ifdef RASPBERRY_PI
    al_set_new_display_flags(ALLEGRO_FULLSCREEN);
#endif
    frequency = QueryPerformanceFrequency();

    ALLEGRO_MONITOR_INFO info;
    al_get_monitor_info(0, &info);
    //ALLEGRO_DISPLAY *display = al_create_display(info.x2 - info.x1, info.y2 - info.y1);
    ALLEGRO_DISPLAY *display = al_create_display(800, 480);
    font_small = al_load_ttf_font("fonts/courbd.ttf",30,0 );
    font = al_load_ttf_font("fonts/courbd.ttf",50,0 );
    font_big = al_load_ttf_font("fonts/courbd.ttf",70,0 );
    font_control = al_load_ttf_font("fonts/courbd.ttf",50,0 );
    display_height = al_get_display_height(display);
    display_width = al_get_display_width(display);
    FOREGROUND_COLOR = al_map_rgb(255,255,255);
    BACKGROUND_COLOR = al_map_rgb(0,0,0);
    SELECTION_COLOR = al_map_rgb(255,255,0);
    ERROR_COLOR = al_map_rgb(255,0,0);
    COLOR_RED = al_map_rgb(255,0,0);
    COLOR_GRAY = al_map_rgb(128,128,128);
    COLOR_GREEN = al_map_rgb(0,255,0);
    COLOR_YELLOW = al_map_rgb(255,255,0);
    

  //If user runs it from somewhere else...
  ALLEGRO_PATH* path = al_get_standard_path(ALLEGRO_RESOURCES_PATH);
  al_change_directory(al_path_cstr(path,ALLEGRO_NATIVE_PATH_SEP));
    
    for (size_t i = 0; i < 4; ++i)
    {
        std::string str = "images/car"+std::to_string(i)+ ".png";
        car_bitmaps[i]=al_load_bitmap(str.c_str());
        assert(car_bitmaps);
    }
    
    sample  = al_load_sample( "sounds/beep.wav" );
    sample2 = al_load_sample( "sounds/beep2.wav" );
    sample3 = al_load_sample( "sounds/beep3.wav" );
    sample4 = al_load_sample( "sounds/beep4.wav" );

    
    if (!font){
      std::cerr<<"Could not load font."<<std::endl;
      return nullptr;
   }
   server.setCommandExecutor([&handler](std::string str, std::ostream & stream)
        {
            //std::cout << str << std::endl;
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
            
            if (str.find("bleft") != std::string::npos) {
                InputEvent event(DLEFT, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bright") != std::string::npos) {
                InputEvent event(DRIGHT, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bup") != std::string::npos) {
                InputEvent event(DUP, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bdown") != std::string::npos) {
                InputEvent event(DDOWN, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("benter") != std::string::npos) {
                InputEvent event(DENTER, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bescape") != std::string::npos) {
                InputEvent event(DBACK, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bsensor1") != std::string::npos) {
                InputEvent event(DSENSOR0, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bsensor2") != std::string::npos) {
                InputEvent event(DSENSOR1, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bsensor3") != std::string::npos) {
                InputEvent event(DSENSOR2, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bsensor4") != std::string::npos) {
                InputEvent event(DSENSOR3, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bnsensor1") != std::string::npos) {
                InputEvent event(DNSENSOR0, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bnsensor2") != std::string::npos) {
                InputEvent event(DNSENSOR1, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bnsensor3") != std::string::npos) {
                InputEvent event(DNSENSOR2, QueryPerformanceCounter());
                handler.call(event);
            }
            if (str.find("bnsensor4") != std::string::npos) {
                InputEvent event(DNSENSOR3, QueryPerformanceCounter());
                handler.call(event);
            }
        }
);
   server.start();
   return display;
}


int menu(InputHandler & input){
    int selected = 0;
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        al_draw_text(font, selected == 0 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 100,ALLEGRO_ALIGN_CENTRE, "Schnelles Rennen");
        al_draw_text(font, selected == 1 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 200,ALLEGRO_ALIGN_CENTRE, "Tournier");
        al_draw_text(font, selected == 2 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 300,ALLEGRO_ALIGN_CENTRE, "Options");
        al_draw_text(font, selected == 3 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 400,ALLEGRO_ALIGN_CENTRE, "Shutdown");
        draw_screen_keyboard();
        al_flip_display();
        
        InputEvent event = input.wait_for_event();
        switch(event._dest)
        {
            case DUP:  selected = (selected + 3) % 4;break;
            case DDOWN:  selected = (selected + 1) % 4; break;
            case DENTER:
            {
                switch (selected){
                    case 0: fast_race(input);break;
                    case 1: tournee(input);break;
                    case 2: options(input);break;
                    case 3: system("systemctl poweroff");break;
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
                    case 0: player ++;break;
                    case 1: slots ++;break;
                    case 2: rounds ++;break;
                    case 3: equal_slots ++;break;
                }break;}
            case DLEFT:{
                switch(selected){
                    case 0: player = std::max(0,player-1);break;
                    case 1: slots = std::max(0,slots-1);break;
                    case 2: rounds = std::max(0,rounds-1);break;
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
            al_draw_textf(font, selected == i ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, row_height * (i + 1),ALLEGRO_ALIGN_CENTRE, "InputPin0 %u", static_cast<uint32_t>(input_pins[i]));
        }
        draw_screen_keyboard();
        al_flip_display();
        InputEvent event = input.wait_for_event();
        switch (event._dest)
        {
            case DUP:  selected = (selected + 4) % 5; break;
            case DDOWN: selected = (selected + 1) % 5;break;
            case DRIGHT:
            {
                if (selected < 4)
                {
                    ++input_pins[selected];
                }
                break;
            }case DLEFT:
            {
                if (selected < 4)
                {
                    --input_pins[selected];
                }
                break;                    
            }case DBACK:
            {
                return 0;
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
        if (al_key_down(&key_state, ALLEGRO_KEY_ESCAPE)){
            return 0;
        }
    }
}

/*int tournee_race(tournee_plan tp){
    
}*/

int fast_race(InputHandler & input){
    int selected = 0;
    int player = 2, rounds = 3;
      
    while (true){
        al_clear_to_color(BACKGROUND_COLOR);
        al_draw_textf(font, selected == 0 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 100,ALLEGRO_ALIGN_CENTRE, "Spieler: %d", player);
        al_draw_textf(font, selected == 1 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 200,ALLEGRO_ALIGN_CENTRE, "Runden:  %d", rounds);
        al_draw_text(font, selected == 2 ? SELECTION_COLOR : FOREGROUND_COLOR,display_width/2, 300,ALLEGRO_ALIGN_CENTRE, "START");
        draw_screen_keyboard();
        al_flip_display();
        InputEvent event = input.wait_for_event();
        switch(event._dest)
        {
            case DUP:selected = (selected + 2) % 3;break;
            case DDOWN:selected = (selected + 1) % 3;break;
            case DRIGHT:
            {
                switch(selected){
                    case 0: player++;break;
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
                    case 2: while (anzeige(rounds, player, input) == 1);break;
                    default: break;
                }
                break;
            }
            case DBACK:
            {
                return 0;
            }
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

/*
ALLEGRO_BITMAP *my_bmp = al_create_bitmap(1000,1000);
al_set_target_bitmap(my_bmp);
load_bitmap_onto_target("test.png");
// perhaps restore the target bitmap to the back buffer, or continue
// to modify the my_bmp with more drawing operations.
*/

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
       // al_draw_filled_circle(rocket._rocket._position[0] >> 16, 480 - (rocket._rocket._position[1] >> 16),2,COLOR_YELLOW);
        varrayl.push_back(ALLEGRO_VERTEX({static_cast<float>(rocket._rocket._position[0] >> 16), static_cast<float>(480 - (rocket._rocket._position[1] >> 16)), 0, 0, 0, COLOR_YELLOW}));
       
        //draws the trails.
        for (std::array<int32_t, 2> const & particle : rocket._trail)
        {
            //std::cout << "trail " << (particle[0] >> 16) << ' ' << (particle[1] >> 16) << std::endl;
            int16_t x = particle[0] >> 16, y = 480 - (particle[1] >> 16);
            if (x >= 0 && x <= 800 && y >= 0 && y <= 480)
            {
               // al_draw_filled_circle(x, y,1,COLOR_YELLOW);
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
               // al_draw_filled_circle(x, y,2,col);
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
    
    /*ALLEGRO_VERTEX_BUFFER* vbuf = al_create_vertex_buffer(NULL , varrayl.data() , varrayl.size(), ALLEGRO_PRIM_BUFFER_DYNAMIC);
    glPointSize(2);
    al_draw_vertex_buffer(vbuf , NULL , 0 , varrayl.size(), ALLEGRO_PRIM_POINT_LIST);
    al_destroy_vertex_buffer(vbuf);
    vbuf = al_create_vertex_buffer(NULL , varrays.data() , varrays.size(), ALLEGRO_PRIM_BUFFER_DYNAMIC);
    glPointSize(1);
    al_draw_vertex_buffer(vbuf , NULL , 0 , varrays.size(), ALLEGRO_PRIM_POINT_LIST);
    al_destroy_vertex_buffer(vbuf);*/
}
};

int anzeige (uint8_t runden, uint8_t spieler, InputHandler &input){
    performance_counter perfc(500);
    //uint8_t eingabepin[2]={128,32};
    race_item race(runden);
    global_race = &race;
    std::vector<player> pl(spieler);
    for (int i=0;i<spieler;i++)
        race.members.emplace_back(race_data_item(pl[i]));
    uint16_t top_border = 0;
    uint16_t bottom_border = 0;
    uint16_t table_top = 120;
    uint16_t row_height = (display_height-table_top-bottom_border)/5;//Todo
    uint16_t left_border = 5;
    uint16_t right_border = 5;
    uint16_t column_width = (display_width-left_border-right_border)/(spieler);
    table_top += 30;
    
    size_t visible_lights = 0;
    size_t time_format = 1;
    race.race_start_time = QueryPerformanceCounter() + frequency * 7;
    uint8_t control = 0;
    
    std::function<void(InputEvent const & event)> callback = [&race, &control](InputEvent const & event){
        if (event._dest >= DSENSOR0 && event._dest < DSENSOR0 + race.member_count())
        {
            bool race_started = event._when > race.race_start_time;
            if (!race_started)
            {
                al_play_sample(sample4, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);                
            }
            else
            {
                race.add_time(race.members[event._dest - DSENSOR0],event._when);
                al_play_sample(sample3, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
            }
        }
        if (event._dest >= DNSENSOR0 && event._dest < DSENSOR0 + race.member_count())
        {
            race.members[event._dest - DNSENSOR0].pop_time();
        }
        else if (event._dest == DBACK)
        {
            control = 2;
        }
    };
    input.register_callback(callback);
    
    std::bernoulli_distribution create_new_rocket(0.2);
    //std::cout << al_get_display_refresh_rate(al_get_current_display()) << std::endl;
    //refresh_rate = al_get_display_refresh_rate(al_get_current_display());
    uint8_t refresh_rate = 30;
    firework_t firework(800<<16, 480<<16, 1600 / refresh_rate);
    std::vector<bool> finished_last_frame(race.member_count(), false);
    // al_draw_text(font, al_map_rgb( 255, 0, 0), 200, 100, ALLEGRO_ALIGN_LEFT,"CONGRATULATIONS" );
    firework_drawer_t draw_firework;
    while (true){
        al_flip_display();
        al_clear_to_color(BACKGROUND_COLOR);
        nanotime_t current_time = QueryPerformanceCounter();
        bool race_started = current_time > race.race_start_time;
        
        if (control)
        {
            break;
        }

        /*al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 1,ALLEGRO_ALIGN_LEFT, "Place:");
        al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 2,ALLEGRO_ALIGN_LEFT, "Lap:");
        al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 3,ALLEGRO_ALIGN_LEFT, "All:");
        al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 4,ALLEGRO_ALIGN_LEFT, "Time:");
        al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 5,ALLEGRO_ALIGN_LEFT, "Last:");
        al_draw_textf(font, FOREGROUND_COLOR, left_border, top_border + row_height * 6,ALLEGRO_ALIGN_LEFT, "Best:");*/
        draw_firework(firework);
        firework();
        if (race.all_finished())
        {
            if (create_new_rocket(firework._gen))
            {
                firework.create_rocket();
            }    
        }
       
        int ylightpos = 50 - std::max(0,static_cast<int>((current_time - race.race_start_time - 2000000) / 20000));
        for (size_t i=0;i<race.member_count();i++)
        {
            race_data_item &race_member = race.members[i];
            int round = race.get_round(race_member);
            size_t column_pos = left_border+column_width*(i) + column_width / 2;
            bool finished = race.has_finished(race_member);
            if (finished && !finished_last_frame[i])
            {
                finished_last_frame[i] = true;
                firework._explosions.emplace_back(300, std::array<int32_t, 2>({0x10000*static_cast<int32_t>(column_pos + column_width / 2),  0x10000*(480 - (table_top + 30))}), std::array<int32_t, 2>({0,0}), 10000, firework._timestep, firework._gen);
            }
            {
            //al_draw_bitmap(car_bitmaps[i], column_pos,40,0);
                int current_place = race_member.round_times.size() == 0 ? race.member_count() : race.get_current_place(race_member);
                float w = al_get_bitmap_width(car_bitmaps[i]), h =al_get_bitmap_height(car_bitmaps[i]); 
                float sh = column_width * h * 0.9 / w;
                al_draw_scaled_bitmap(car_bitmaps[i], 0, 0, w,h, column_pos - column_width * 0.45, current_place * 40 - sh/2+5, column_width * 0.9, sh, 0);
            }
            al_draw_text(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 0,  ALLEGRO_ALIGN_LEFT, race_member.belongs_to_player->first_name.c_str());
            al_draw_textf(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 1, ALLEGRO_ALIGN_RIGHT, "%d",round);
            al_draw_textf(font_small, FOREGROUND_COLOR, column_pos, table_top + row_height * 1+20, ALLEGRO_ALIGN_LEFT, "/%d",runden);

            /*if (!race_started){
                draw_time(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 2, time_format, std::numeric_limits<nanotime_t>::max());
            }else{
                draw_time(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 2, time_format, (finished ? race_member.get_absolut_last_time() : current_time) - race.race_start_time);
            }*/
            
            //draw_time(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 3, ALLEGRO_ALIGN_CENTER, time_format, round < 1 || finished ? std::numeric_limits<nanotime_t>::max() : (current_time - race_member.get_absolut_last_time()));
            
            draw_time(font, finished || race_member.get_best_time_index() + 1== race_member.round_times.size() ? SELECTION_COLOR : FOREGROUND_COLOR, column_pos, table_top + row_height * 2, ALLEGRO_ALIGN_CENTER, time_format, finished ? race_member.get_best_time() : race_member.get_relative_last_time());
            draw_time(font, FOREGROUND_COLOR, column_pos, table_top + row_height * 3, ALLEGRO_ALIGN_CENTER, time_format, race_member.get_absolut_last_time() == std::numeric_limits<nanotime_t>::max() ? std::numeric_limits<nanotime_t>::max() : (race_member.get_absolut_last_time() - race.race_start_time));
            
            draw_ptime(font_small, FOREGROUND_COLOR, column_pos+10, table_top + row_height * 4, ALLEGRO_ALIGN_CENTER, time_format, race.timediff_to_first(race_member));
            //draw_time(FOREGROUND_COLOR, column_pos, table_top + row_height * 5, time_format, race_member.get_best_time());
            
            /**/
            if (finished)
            {
                int place = race.get_place(race_member);
                if (place != -1){
                    al_draw_filled_circle(column_pos, table_top + row_height * 0+30, 30, al_map_rgb(200,200,200));
                    al_draw_textf(font_big, COLOR_RED, column_pos, table_top + row_height * 0, ALLEGRO_ALIGN_CENTRE, "%d",place);
                }
            }
        }
        if (ylightpos >= -50)
        {
            al_draw_filled_rectangle (display_width/2-250,0,display_width/2+250,ylightpos + 50, FOREGROUND_COLOR);
            size_t tmp = 7 + divf(current_time - race.race_start_time,frequency);
            if (visible_lights != tmp)
            {
                if (tmp < 6)
                {
                    al_play_sample(sample, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
                }
                else if (tmp == 7)
                {
                    al_play_sample(sample2, 1.0, 0.0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
                }
            }
            visible_lights = tmp;
            
            if (race_started){
                for (int i=0;i<5;i++){
                    al_draw_filled_circle(100*i+display_width/2-200, ylightpos, 40, COLOR_GREEN);
                }
            }
            else{
                for (size_t i=0;i<5;i++)
                {
                    al_draw_filled_circle(100*i+display_width/2-200, ylightpos, 40, visible_lights > i ? COLOR_RED : FOREGROUND_COLOR); 
                }
            }
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
