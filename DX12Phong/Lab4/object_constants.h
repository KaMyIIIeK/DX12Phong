#ifndef OBJECT_CONSTANTS_
#define OBJECT_CONSTANTS_

#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

struct ObjectConstants
{
    Matrix World = Matrix::Identity;
    Vector4 Color = Vector4(1.f, 1.f, 1.f, 1.f);
};

#endif // OBJECT_CONSTANTS_