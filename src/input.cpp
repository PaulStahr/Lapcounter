#include "input.h"
    
#include <algorithm>
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

void InputHandler::register_callback(std::function<void (InputEvent const &)> callback)
{
    callbacks.push_back(callback);
}

bool InputHandler::is_valid()
{
    return _is_valid;
}

void InputHandler::unregister_callback(std::function<void (InputEvent const &)> callback)
{
    callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
}

InputEvent InputHandler::wait_for_event()
{
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck);//TODO could be waked spontanously
    return last_event;
}

void InputHandler::call(InputEvent const & event)
{
    std::unique_lock<std::mutex> lck(mtx);
    last_event = event;
    cv.notify_all();
    for (std::function<void (InputEvent const &)> & callback : callbacks)
    {
        callback(event);
    }
}

void InputHandler::destroy()
{
    _is_valid = false;
}
