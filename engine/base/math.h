#pragma once

namespace nx
{
    struct Vec2
    {
        float x = 0, y = 0;

        Vec2() = default;
        Vec2(float x_, float y_) : x(x_), y(y_) {}

        Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
        Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
        Vec2 operator*(float s) const { return { x * s, y * s }; }
        Vec2 operator/(float s) const { return { x / s, y / s }; }
        Vec2 operator-() const { return { -x, -y }; }
        bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
        float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    };

    struct Vec3
    {
        float x = 0, y = 0, z = 0;

        Vec3() = default;
        Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
        Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
        Vec3 operator-() const { return { -x, -y, -z }; }
        bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
        float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
        Vec3 cross(const Vec3& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
        float length() const;
        Vec3 normalized() const;
    };

    // Column vectors, columns stored together (so m[12..14] is the translation). That is the same
    // memory layout as Direct3D's row vectors with row storage. Clip-space depth is 0 to 1.
    struct Mat4
    {
        float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        Mat4 operator*(const Mat4& o) const;
        Vec3 transformPoint(const Vec3& p) const;

        static Mat4 translation(const Vec3& t);
        static Mat4 scale(const Vec3& s);
        static Mat4 rotationZ(float radians);
        // Looking along -z from the origin, y up: a right-handed camera. The angle is from top to bottom.
        static Mat4 perspective(float angle, float aspect, float zNear, float zFar);
        static Mat4 lookAt(const Vec3& from, const Vec3& to, const Vec3& up);
        // Whatever can be undone; the identity if it can't.
        Mat4 inverse() const;
        Vec3 transformDirection(const Vec3& d) const;
        static Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);
    };

    // A rotation. Always of length 1.
    struct Quat
    {
        float x = 0, y = 0, z = 0, w = 1;

        Quat() = default;
        Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

        Quat operator*(const Quat& o) const;
        Quat conjugate() const { return { -x, -y, -z, w }; }
        Quat normalized() const;
        Vec3 rotate(const Vec3& v) const;

        static Quat axisAngle(const Vec3& axis, float radians);
        // Turned about y first, then tipped about x, then rolled about z; degrees, as a script gives them.
        static Quat fromAngles(const Vec3& pitchYawRoll);
        Vec3 angles() const;
        static Quat slerp(const Quat& a, const Quat& b, float t);
    };

    // The same as translation * rotation * scale.
    Mat4 compose(const Vec3& place, const Quat& turn, const Vec3& size);
}
