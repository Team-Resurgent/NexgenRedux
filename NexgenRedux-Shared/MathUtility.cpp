#include "MathUtility.h"

#include <ctime>

#if defined NEXGEN_WIN || defined NEXGEN_UWP || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <random>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <xtl.h>
#endif

using namespace NexgenRedux;

// Vec2D

MathUtility::Vec2D::Vec2D() : x(0), y(0) 
{

} 

MathUtility::Vec2D::Vec2D(const double& x, const double& y) : x(x), y(y) 
{

}  

MathUtility::Vec2D::Vec2D(const Vec2D& other) 
{
    for (uint32_t i = 0; i < 2; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Vec2D MathUtility::Vec2D::operator/(const MathUtility::Vec2D& other) const 
{ 
    return MathUtility::Vec2D(x / other.x, y / other.y); 
}  

MathUtility::Vec2D MathUtility::Vec2D::operator*(const MathUtility::Vec2D& other) const 
{ 
    return MathUtility::Vec2D(x * other.x, y * other.y); 
}  

MathUtility::Vec2D MathUtility::Vec2D::operator+(const MathUtility::Vec2D& other) const 
{ 
    return MathUtility::Vec2D(x + other.x, y + other.y); 
}  

MathUtility::Vec2D MathUtility::Vec2D::operator-(const MathUtility::Vec2D& other) const 
{ 
    return MathUtility::Vec2D(x - other.x, y - other.y); 
}

void MathUtility::Vec2D::operator/=(const MathUtility::Vec2D& other)
{
    MathUtility::Vec2D result = *this / other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2D::operator*=(const MathUtility::Vec2D& other)
{
    MathUtility::Vec2D result = *this * other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2D::operator+=(const MathUtility::Vec2D& other)
{
    MathUtility::Vec2D result = *this + other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2D::operator-=(const MathUtility::Vec2D& other)
{
    MathUtility::Vec2D result = *this - other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Vec2D::operator==(const MathUtility::Vec2D& other) const 
{ 
    for (uint32_t i = 0; i < 2; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Vec2D::operator!=(const MathUtility::Vec2D& other) const 
{ 
    return !(*this == other); 
}

// Vec2F

MathUtility::Vec2F::Vec2F() : x(0), y(0) 
{

} 

MathUtility::Vec2F::Vec2F(const float& x, const float& y) : x(x), y(y) 
{

}  

MathUtility::Vec2F::Vec2F(const Vec2F& other) 
{
    for (uint32_t i = 0; i < 2; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Vec2F MathUtility::Vec2F::operator/(const MathUtility::Vec2F& other) const 
{ 
    return MathUtility::Vec2F(x / other.x, y / other.y); 
}  

MathUtility::Vec2F MathUtility::Vec2F::operator*(const MathUtility::Vec2F& other) const 
{ 
    return MathUtility::Vec2F(x * other.x, y * other.y); 
}  

MathUtility::Vec2F MathUtility::Vec2F::operator+(const MathUtility::Vec2F& other) const 
{ 
    return MathUtility::Vec2F(x + other.x, y + other.y); 
}  

MathUtility::Vec2F MathUtility::Vec2F::operator-(const MathUtility::Vec2F& other) const 
{ 
    return MathUtility::Vec2F(x - other.x, y - other.y); 
}

void MathUtility::Vec2F::operator/=(const MathUtility::Vec2F& other)
{
    MathUtility::Vec2F result = *this / other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2F::operator*=(const MathUtility::Vec2F& other)
{
    MathUtility::Vec2F result = *this * other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2F::operator+=(const MathUtility::Vec2F& other)
{
    MathUtility::Vec2F result = *this + other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec2F::operator-=(const MathUtility::Vec2F& other)
{
    MathUtility::Vec2F result = *this - other;
    for (uint32_t i = 0; i < 2; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Vec2F::operator==(const MathUtility::Vec2F& other) const 
{ 
    for (uint32_t i = 0; i < 2; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Vec2F::operator!=(const MathUtility::Vec2F& other) const 
{ 
    return !(*this == other); 
}

// Vec3F

MathUtility::Vec3F::Vec3F() : x(0), y(0), z(0)
{

} 

MathUtility::Vec3F::Vec3F(const float& x, const float& y, const float& z) : x(x), y(y), z(z) 
{

}  

MathUtility::Vec3F::Vec3F(const Vec3F& other) 
{
    for (uint32_t i = 0; i < 3; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Vec3F MathUtility::Vec3F::operator/(const MathUtility::Vec3F& other) const 
{ 
    return MathUtility::Vec3F(x / other.x, y / other.y, z / other.z); 
}  

MathUtility::Vec3F MathUtility::Vec3F::operator*(const MathUtility::Vec3F& other) const 
{ 
    return MathUtility::Vec3F(x * other.x, y * other.y, z * other.z); 
}  

MathUtility::Vec3F MathUtility::Vec3F::operator+(const MathUtility::Vec3F& other) const 
{ 
    return MathUtility::Vec3F(x + other.x, y + other.y, z + other.z); 
}  

MathUtility::Vec3F MathUtility::Vec3F::operator-(const MathUtility::Vec3F& other) const 
{ 
    return MathUtility::Vec3F(x - other.x, y - other.y, z - other.z); 
}

void MathUtility::Vec3F::operator/=(const MathUtility::Vec3F& other)
{
    MathUtility::Vec3F result = *this / other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec3F::operator*=(const MathUtility::Vec3F& other)
{
    MathUtility::Vec3F result = *this * other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec3F::operator+=(const MathUtility::Vec3F& other)
{
    MathUtility::Vec3F result = *this + other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec3F::operator-=(const MathUtility::Vec3F& other)
{
    MathUtility::Vec3F result = *this - other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Vec3F::operator==(const MathUtility::Vec3F& other) const 
{ 
    for (uint32_t i = 0; i < 3; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Vec3F::operator!=(const MathUtility::Vec3F& other) const 
{ 
    return !(*this == other); 
}

float MathUtility::Vec3F::Dot(const Vec3F& other) 
{ 
    return x * other.x + y * other.y + z * other.z; 
}

MathUtility::Vec3F MathUtility::Vec3F::Cross(const MathUtility::Vec3F& other) 
{ 
    Vec3F result;
    result.values[0] = values[1] * other.values[2] - values[2] * other.values[1];
    result.values[1] = values[2] * other.values[0] - values[0] * other.values[2];
    result.values[2] = values[0] * other.values[1] - values[1] * other.values[0];		
    return result; 
}

MathUtility::Vec3F MathUtility::Vec3F::Normal()
{
    float length = (float)sqrt(x * x + y * y + z * z);
    return Vec3F(x / length, y / length, z / length);
}

// Vec4F

MathUtility::Vec4F::Vec4F() : x(0), y(0), z(0), w(0)
{

} 

MathUtility::Vec4F::Vec4F(const float& x, const float& y, const float& z, const float& w) : x(x), y(y), z(z), w(w) 
{

}  

MathUtility::Vec4F::Vec4F(const Vec4F& other) 
{
    for (uint32_t i = 0; i < 4; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Vec4F MathUtility::Vec4F::operator/(const MathUtility::Vec4F& other) const 
{ 
    return MathUtility::Vec4F(x / other.x, y / other.y, z / other.z, w / other.w); 
}  

MathUtility::Vec4F MathUtility::Vec4F::operator*(const MathUtility::Vec4F& other) const 
{ 
    return MathUtility::Vec4F(x * other.x, y * other.y, z * other.z, w * other.w); 
}  

MathUtility::Vec4F MathUtility::Vec4F::operator+(const MathUtility::Vec4F& other) const 
{ 
    return MathUtility::Vec4F(x + other.x, y + other.y, z + other.z, w + other.w); 
}  

MathUtility::Vec4F MathUtility::Vec4F::operator-(const MathUtility::Vec4F& other) const 
{ 
    return MathUtility::Vec4F(x - other.x, y - other.y, z - other.z, w - other.w); 
}

void MathUtility::Vec4F::operator/=(const MathUtility::Vec4F& other)
{
    MathUtility::Vec4F result = *this / other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec4F::operator*=(const MathUtility::Vec4F& other)
{
    MathUtility::Vec4F result = *this * other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec4F::operator+=(const MathUtility::Vec4F& other)
{
    MathUtility::Vec4F result = *this + other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Vec4F::operator-=(const MathUtility::Vec4F& other)
{
    MathUtility::Vec4F result = *this - other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Vec4F::operator==(const MathUtility::Vec4F& other) const 
{ 
    for (uint32_t i = 0; i < 4; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Vec4F::operator!=(const MathUtility::Vec4F& other) const 
{ 
    return !(*this == other); 
}

// SizeI

MathUtility::SizeI::SizeI() : width(0), height(0) 
{

}  

MathUtility::SizeI::SizeI(const int32_t& width, const int32_t& height) : width(width), height(height)
{

}  

MathUtility::SizeI::SizeI(const MathUtility::SizeI& other) 
{
    for (uint32_t i = 0; i < 2; i++)
    {
        values[i] = other.values[i];
    }
} 

bool MathUtility::SizeI::operator==(const MathUtility::SizeI& other) const 
{ 
    for (uint32_t i = 0; i < 2; i++) 
    {
        if (abs(values[i] - other.values[i]) > 0) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::SizeI::operator!=(const MathUtility::SizeI& other) const 
{ 
    return !(*this == other); 
}

// SizeF

MathUtility::SizeF::SizeF() : width(0), height(0) 
{

}  

MathUtility::SizeF::SizeF(const float& width, const float& height) : width(width), height(height)
{

}  

MathUtility::SizeF::SizeF(const MathUtility::SizeF& other) 
{
    for (uint32_t i = 0; i < 2; i++)
    {
        values[i] = other.values[i];
    }
} 

bool MathUtility::SizeF::operator==(const MathUtility::SizeF& other) const 
{ 
    for (uint32_t i = 0; i < 2; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-6f) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::SizeF::operator!=(const MathUtility::SizeF& other) const 
{ 
    return !(*this == other); 
}

// RectI

MathUtility::RectI::RectI() : x(0), y(0), width(0), height(0) 
{

}  

MathUtility::RectI::RectI(const int32_t& x, const int32_t& y, const int32_t& width, const int32_t& height) : x(x), y(y), width(width), height(height)
{

}  

MathUtility::RectI::RectI(const MathUtility::RectI& other) 
{
    for (uint32_t i = 0; i < 4; i++)
    {
        values[i] = other.values[i];
    }
} 

bool MathUtility::RectI::operator==(const MathUtility::RectI& other) const 
{ 
    for (uint32_t i = 0; i < 4; i++) 
    {
        if (abs(values[i] - other.values[i]) > 0) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::RectI::operator!=(const MathUtility::RectI& other) const 
{ 
    return !(*this == other); 
}

// RectF

MathUtility::RectF::RectF() : x(0), y(0), width(0), height(0) 
{

}  

MathUtility::RectF::RectF(const float& x, const float& y, const float& width, const float& height) : x(x), y(y), width(width), height(height)
{

}  

MathUtility::RectF::RectF(const MathUtility::RectF& other) 
{
    for (uint32_t i = 0; i < 4; i++)
    {
        values[i] = other.values[i];
    }
} 

bool MathUtility::RectF::operator==(const MathUtility::RectF& other) const 
{ 
    for (uint32_t i = 0; i < 4; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-6f) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::RectF::operator!=(const MathUtility::RectF& other) const 
{ 
    return !(*this == other); 
}

// Color3F

MathUtility::Color3F::Color3F() : r(0), g(0), b(0)
{

} 

MathUtility::Color3F::Color3F(const float& r, const float& g, const float& b) : r(r), g(g), b(b) 
{

}  

MathUtility::Color3F::Color3F(const Color3F& other) 
{
    for (uint32_t i = 0; i < 3; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Color3F MathUtility::Color3F::operator/(const MathUtility::Color3F& other) const 
{ 
    return MathUtility::Color3F(r / other.r, g / other.g, b / other.b); 
}  

MathUtility::Color3F MathUtility::Color3F::operator*(const MathUtility::Color3F& other) const 
{ 
    return MathUtility::Color3F(r * other.r, g * other.g, b * other.b); 
}  

MathUtility::Color3F MathUtility::Color3F::operator+(const MathUtility::Color3F& other) const 
{ 
    return MathUtility::Color3F(r + other.r, g + other.g, b + other.b); 
}  

MathUtility::Color3F MathUtility::Color3F::operator-(const MathUtility::Color3F& other) const 
{ 
    return MathUtility::Color3F(r - other.r, g - other.g, b - other.b); 
}

void MathUtility::Color3F::operator/=(const MathUtility::Color3F& other)
{
    MathUtility::Color3F result = *this / other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color3F::operator*=(const MathUtility::Color3F& other)
{
    MathUtility::Color3F result = *this * other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color3F::operator+=(const MathUtility::Color3F& other)
{
    MathUtility::Color3F result = *this + other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color3F::operator-=(const MathUtility::Color3F& other)
{
    MathUtility::Color3F result = *this - other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Color3F::operator==(const MathUtility::Color3F& other) const 
{ 
    for (uint32_t i = 0; i < 3; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Color3F::operator!=(const MathUtility::Color3F& other) const 
{ 
    return !(*this == other); 
}

// Color4F

MathUtility::Color4F::Color4F() : r(0), g(0), b(0), a(0)
{

} 

MathUtility::Color4F::Color4F(const float& r, const float& g, const float& b, const float& a) : r(r), g(g), b(b), a(a) 
{

}  

MathUtility::Color4F::Color4F(const Color4F& other) 
{
    for (uint32_t i = 0; i < 4; i++)
    {
        values[i] = other.values[i];
    }
}

MathUtility::Color4F MathUtility::Color4F::operator/(const MathUtility::Color4F& other) const 
{ 
    return MathUtility::Color4F(r / other.r, g / other.g, b / other.b, a / other.a); 
}  

MathUtility::Color4F MathUtility::Color4F::operator*(const MathUtility::Color4F& other) const 
{ 
    return MathUtility::Color4F(r * other.r, g * other.g, b * other.b, a * other.a); 
}  

MathUtility::Color4F MathUtility::Color4F::operator+(const MathUtility::Color4F& other) const 
{ 
    return MathUtility::Color4F(r + other.r, g + other.g, b + other.b, a + other.a); 
}  

MathUtility::Color4F MathUtility::Color4F::operator-(const MathUtility::Color4F& other) const 
{ 
    return MathUtility::Color4F(r - other.r, g - other.g, b - other.b, a - other.a); 
}

void MathUtility::Color4F::operator/=(const MathUtility::Color4F& other)
{
    MathUtility::Color4F result = *this / other;
    for (uint32_t i = 0; i < 3; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color4F::operator*=(const MathUtility::Color4F& other)
{
    MathUtility::Color4F result = *this * other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color4F::operator+=(const MathUtility::Color4F& other)
{
    MathUtility::Color4F result = *this + other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

void MathUtility::Color4F::operator-=(const MathUtility::Color4F& other)
{
    MathUtility::Color4F result = *this - other;
    for (uint32_t i = 0; i < 4; i++)
    {
        this->values[i] = result.values[i];
    }
}

bool MathUtility::Color4F::operator==(const MathUtility::Color4F& other) const 
{ 
    for (uint32_t i = 0; i < 4; i++) 
    {
        if (fabs(values[i] - other.values[i]) > 1e-9) 
        {
            return false;
        }
    }
    return true;
}

bool MathUtility::Color4F::operator!=(const MathUtility::Color4F& other) const 
{ 
    return !(*this == other); 
}

// Methods

float MathUtility::DegreesToRadians(float const degrees)
{
    return degrees * (3.14159265358979323846f / 180.0f);
}

void MathUtility::SeedRandom()
{
    srand((uint32_t)time(NULL));
}

void MathUtility::SeedRandomWithValue(uint32_t value)
{
    srand(value);
}

double MathUtility::GetRandomDouble()
{
    return (double)rand() / RAND_MAX;
}

int32_t MathUtility::GetRandomIntInRange(int32_t start, int32_t end)
{
    int first = start > end ? end : start;
    int last =  start > end ? start : end;
    return (rand() % (last - first)) + first;
}

void MathUtility::ConvertEndian(uint32_t& value) 
{
    value = ((value << 24) & 0xFF000000) | ((value <<  8) & 0x00FF0000) | ((value >>  8) & 0x0000FF00) | ((value >> 24) & 0x000000FF);
}

void MathUtility::ConvertEndian(uint16_t& value) 
{
    value = ((value << 8) & 0xFF00) | ((value >> 8) & 0x00FF);
}

void MathUtility::ConvertEndian(float& value) 
{
    float retVal;
    char *floatToConvert = (char*)&value;
    char *returnFloat = (char*)&retVal;
    returnFloat[0] = floatToConvert[3];
    returnFloat[1] = floatToConvert[2];
    returnFloat[2] = floatToConvert[1];
    returnFloat[3] = floatToConvert[0];
    value = retVal;
}

bool MathUtility::IsPowerOf2(uint32_t value)
{
    return (value & (value - 1)) == 0;
}