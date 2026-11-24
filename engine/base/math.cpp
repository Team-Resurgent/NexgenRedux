#include "base/math.h"

#include <math.h>

namespace nx
{
    float Vec3::length() const
    {
        return sqrtf(dot(*this));
    }

    Vec3 Vec3::normalized() const
    {
        const float l = length();
        return l > 0.0f ? *this / l : Vec3();
    }

    Mat4 Mat4::operator*(const Mat4& o) const
    {
        Mat4 r;
        for (int c = 0; c < 4; c++)
        {
            for (int row = 0; row < 4; row++)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++)
                {
                    sum += m[k * 4 + row] * o.m[c * 4 + k];
                }
                r.m[c * 4 + row] = sum;
            }
        }
        return r;
    }

    Vec3 Mat4::transformPoint(const Vec3& p) const
    {
        return {
            m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]
        };
    }

    Mat4 Mat4::translation(const Vec3& t)
    {
        Mat4 r;
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    Mat4 Mat4::scale(const Vec3& s)
    {
        Mat4 r;
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        return r;
    }

    Mat4 Mat4::rotationZ(float radians)
    {
        const float c = cosf(radians);
        const float s = sinf(radians);
        Mat4 r;
        r.m[0] = c;
        r.m[1] = s;
        r.m[4] = -s;
        r.m[5] = c;
        return r;
    }

    Mat4 Mat4::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
    {
        Mat4 r;
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = 1.0f / (zFar - zNear);
        r.m[12] = (left + right) / (left - right);
        r.m[13] = (top + bottom) / (bottom - top);
        r.m[14] = zNear / (zNear - zFar);
        return r;
    }
}
