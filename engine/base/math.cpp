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

    Mat4 Mat4::perspective(float angle, float aspect, float zNear, float zFar)
    {
        const float f = 1.0f / tanf(angle * 0.5f);
        Mat4 r;
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = zFar / (zNear - zFar);
        r.m[11] = -1;
        r.m[14] = zNear * zFar / (zNear - zFar);
        r.m[15] = 0;
        return r;
    }

    Mat4 Mat4::lookAt(const Vec3& from, const Vec3& to, const Vec3& up)
    {
        Vec3 z = (from - to).normalized();
        if (z.dot(z) < 0.5f)
        {
            z = { 0, 0, 1 };
        }
        Vec3 x = up.cross(z).normalized();
        if (x.dot(x) < 0.5f)
        {
            x = Vec3(0, 0, 1).cross(z).normalized();  // straight up or down
            x = x.dot(x) < 0.5f ? Vec3(1, 0, 0) : x;
        }
        const Vec3 y = z.cross(x);
        Mat4 r;
        r.m[0] = x.x;
        r.m[1] = y.x;
        r.m[2] = z.x;
        r.m[4] = x.y;
        r.m[5] = y.y;
        r.m[6] = z.y;
        r.m[8] = x.z;
        r.m[9] = y.z;
        r.m[10] = z.z;
        r.m[12] = -x.dot(from);
        r.m[13] = -y.dot(from);
        r.m[14] = -z.dot(from);
        return r;
    }

    Vec3 Mat4::transformDirection(const Vec3& d) const
    {
        return { m[0] * d.x + m[4] * d.y + m[8] * d.z, m[1] * d.x + m[5] * d.y + m[9] * d.z, m[2] * d.x + m[6] * d.y + m[10] * d.z };
    }

    Mat4 Mat4::inverse() const
    {
        // Gauss-Jordan, with the largest pivot each time.
        float a[4][8];
        for (int row = 0; row < 4; row++)
        {
            for (int c = 0; c < 4; c++)
            {
                a[row][c] = m[c * 4 + row];
                a[row][c + 4] = row == c ? 1.0f : 0.0f;
            }
        }
        for (int c = 0; c < 4; c++)
        {
            int best = c;
            for (int row = c + 1; row < 4; row++)
            {
                best = fabsf(a[row][c]) > fabsf(a[best][c]) ? row : best;
            }
            if (fabsf(a[best][c]) < 1e-12f)
            {
                return {};
            }
            for (int k = 0; k < 8; k++)
            {
                const float t = a[c][k];
                a[c][k] = a[best][k];
                a[best][k] = t;
            }
            const float inverse = 1 / a[c][c];
            for (int k = 0; k < 8; k++)
            {
                a[c][k] *= inverse;
            }
            for (int row = 0; row < 4; row++)
            {
                const float f = row == c ? 0 : a[row][c];
                for (int k = 0; k < 8; k++)
                {
                    a[row][k] -= f * a[c][k];
                }
            }
        }
        Mat4 r;
        for (int row = 0; row < 4; row++)
        {
            for (int c = 0; c < 4; c++)
            {
                r.m[c * 4 + row] = a[row][c + 4];
            }
        }
        return r;
    }

    Quat Quat::operator*(const Quat& o) const
    {
        return { w * o.x + x * o.w + y * o.z - z * o.y, w * o.y - x * o.z + y * o.w + z * o.x, w * o.z + x * o.y - y * o.x + z * o.w, w * o.w - x * o.x - y * o.y - z * o.z };
    }

    Quat Quat::normalized() const
    {
        const float l = sqrtf(x * x + y * y + z * z + w * w);
        return l > 1e-12f ? Quat(x / l, y / l, z / l, w / l) : Quat();
    }

    Vec3 Quat::rotate(const Vec3& v) const
    {
        const Vec3 q(x, y, z);
        const Vec3 t = q.cross(v) * 2.0f;
        return v + t * w + q.cross(t);
    }

    Quat Quat::axisAngle(const Vec3& axis, float radians)
    {
        const Vec3 n = axis.normalized();
        const float s = sinf(radians * 0.5f);
        return { n.x * s, n.y * s, n.z * s, cosf(radians * 0.5f) };
    }

    Quat Quat::fromAngles(const Vec3& a)
    {
        constexpr float half = 3.14159265f / 360.0f;
        const Quat pitch(sinf(a.x * half), 0, 0, cosf(a.x * half));
        const Quat yaw(0, sinf(a.y * half), 0, cosf(a.y * half));
        const Quat roll(0, 0, sinf(a.z * half), cosf(a.z * half));
        return yaw * pitch * roll;
    }

    Vec3 Quat::angles() const
    {
        constexpr float degrees = 180.0f / 3.14159265f;
        const float sinPitch = 2 * (w * x - y * z);
        const float pitch = sinPitch >= 1 ? 90.0f : sinPitch <= -1 ? -90.0f : asinf(sinPitch) * degrees;
        const float yaw = atan2f(2 * (w * y + x * z), 1 - 2 * (x * x + y * y)) * degrees;
        const float roll = atan2f(2 * (w * z + x * y), 1 - 2 * (x * x + z * z)) * degrees;
        return { pitch, yaw, roll };
    }

    Quat Quat::slerp(const Quat& a, const Quat& b, float t)
    {
        float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        const float sign = d < 0 ? -1.0f : 1.0f;  // the short way round
        d *= sign;
        float fa = 1 - t, fb = t;
        if (d < 0.9995f)
        {
            const float angle = acosf(d), s = sinf(angle);
            fa = sinf((1 - t) * angle) / s;
            fb = sinf(t * angle) / s;
        }
        fb *= sign;
        return Quat(a.x * fa + b.x * fb, a.y * fa + b.y * fb, a.z * fa + b.z * fb, a.w * fa + b.w * fb).normalized();
    }

    Mat4 compose(const Vec3& place, const Quat& q, const Vec3& size)
    {
        const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        Mat4 r;
        r.m[0] = (1 - 2 * (yy + zz)) * size.x;
        r.m[1] = 2 * (xy + wz) * size.x;
        r.m[2] = 2 * (xz - wy) * size.x;
        r.m[4] = 2 * (xy - wz) * size.y;
        r.m[5] = (1 - 2 * (xx + zz)) * size.y;
        r.m[6] = 2 * (yz + wx) * size.y;
        r.m[8] = 2 * (xz + wy) * size.z;
        r.m[9] = 2 * (yz - wx) * size.z;
        r.m[10] = (1 - 2 * (xx + yy)) * size.z;
        r.m[12] = place.x;
        r.m[13] = place.y;
        r.m[14] = place.z;
        return r;
    }
}
