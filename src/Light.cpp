#include "Light.h"

void Light::SetDiffuseColor(float red, float green, float blue, float alpha)
{
    diffuseColor_ = {red, green, blue, alpha};
}

void Light::SetDirection(float x, float y, float z)
{
    direction_ = {x, y, z};
}

