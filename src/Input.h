#pragma once

#include <array>

class Input
{
public:
    void Initialize();
    void Frame();
    void KeyDown(unsigned int key);
    void KeyUp(unsigned int key);
    bool IsKeyDown(unsigned int key) const;
    bool IsEscapePressed() const;

private:
    static constexpr unsigned int kKeyCount = 256;
    std::array<bool, kKeyCount> keys_ = {};
};

