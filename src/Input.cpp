#include "Input.h"

#include <windows.h>

void Input::Initialize()
{
    keys_.fill(false);
}

void Input::Frame()
{
}

void Input::KeyDown(unsigned int key)
{
    if (key < keys_.size())
    {
        keys_[key] = true;
    }
}

void Input::KeyUp(unsigned int key)
{
    if (key < keys_.size())
    {
        keys_[key] = false;
    }
}

bool Input::IsKeyDown(unsigned int key) const
{
    return key < keys_.size() && keys_[key];
}

bool Input::IsEscapePressed() const
{
    return IsKeyDown(VK_ESCAPE);
}

