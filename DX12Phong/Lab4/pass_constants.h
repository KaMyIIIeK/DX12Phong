#ifndef PASS_CONSTANTS_
#define PASS_CONSTANTS_

#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

struct PassConstants
{
    Matrix ViewProj = Matrix::Identity;
    Vector4 LightDir = Vector4(0.577f, -0.577f, 0.577f, 0.0f);
    Vector4 EyePosW = Vector4(0.f, 0.f, -5.f, 1.0f);

    float AmbientStrength = 0.20f;
    float SpecularStrength = 0.60f;
    float SpecularPower = 32.0f;
    float Padding = 0.0f;
};

#endif // PASS_CONSTANTS_