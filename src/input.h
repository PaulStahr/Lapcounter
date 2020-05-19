#ifndef INPUT_H
#define INPUT_H

#include <mutex>
#include <condition_variable>
#include <functional>
#include "performance_counter.h"

enum Destination {DLEFT, DRIGHT, DUP, DDOWN, DENTER, DBACK, DSENSOR0, DSENSOR1, DSENSOR2, DSENSOR3, DNSENSOR0, DNSENSOR1, DNSENSOR2, DNSENSOR3, DUNDEF};

class InputEvent
{
public:
    Destination _dest;
    nanotime_t _when;
private:
    
    public:
        InputEvent(Destination dest_, nanotime_t when_) : _dest(dest_), _when(when_){}
        InputEvent(Destination dest_) : _dest(dest_), _when(QueryPerformanceCounter()){}
};

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
    
    void register_callback(std::function<void (InputEvent const &)> callback);
    
    bool is_valid();
    
    void unregister_callback(std::function<void (InputEvent const &)> callback);
    
    InputEvent wait_for_event();
    
    void call(InputEvent const & event);
    
    void destroy();
};

bool operator == (std::function<void (InputEvent const &)> f, std::function<void (InputEvent const &)> g);

#endif
