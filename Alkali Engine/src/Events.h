#pragma once

#include "KeyCodes.h"

// Base class for all event args
class EventArgs
{
public:
    EventArgs()
    {}

};

class ResizeEventArgs : public EventArgs
{
public:
    typedef EventArgs base;
    ResizeEventArgs(int width, int height)
        : Width(width)
        , Height(height)
    {}

    // The new width of the window
    int Width;
    // The new height of the window.
    int Height;

};

class UserEventArgs : public EventArgs
{
public:
    typedef EventArgs base;
    UserEventArgs(int code, void* data1, void* data2)
        : Code(code)
        , Data1(data1)
        , Data2(data2)
    {}

    int     Code;
    void* Data1;
    void* Data2;
};