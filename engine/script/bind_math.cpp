#include "base/math.h"
#include "script/bind.h"
#include "script/script_host.h"

#include <stddef.h>

namespace nx
{
    namespace
    {
        Vec2 scaleVec2(float s, const Vec2& v)
        {
            return v * s;
        }

        Vec3 scaleVec3(float s, const Vec3& v)
        {
            return v * s;
        }

        Vec2& addVec2(const Vec2& o, Vec2& v) { return v = v + o; }
        Vec2& subtractVec2(const Vec2& o, Vec2& v) { return v = v - o; }
        Vec2& multiplyVec2(float s, Vec2& v) { return v = v * s; }
        Vec3& addVec3(const Vec3& o, Vec3& v) { return v = v + o; }
        Vec3& subtractVec3(const Vec3& o, Vec3& v) { return v = v - o; }
        Vec3& multiplyVec3(float s, Vec3& v) { return v = v * s; }
    }

    bool bindMath(ScriptHost& host)
    {
        asIScriptEngine* e = host.engine();
        bool ok = true;

        const auto method = [&](const char* type, const char* declaration, const asSFuncPtr& fn, asDWORD convention)
        {
            ok = host.check(e->RegisterObjectMethod(type, declaration, fn, convention), declaration) && ok;
        };
        const auto behaviour = [&](const char* type, asEBehaviours kind, const char* declaration, const asSFuncPtr& fn, asDWORD convention)
        {
            ok = host.check(e->RegisterObjectBehaviour(type, kind, declaration, fn, convention), declaration) && ok;
        };
        const auto property = [&](const char* type, const char* declaration, size_t offset)
        {
            ok = host.check(e->RegisterObjectProperty(type, declaration, static_cast<int>(offset)), declaration) && ok;
        };

        ok = host.check(e->RegisterObjectType("Vec2", sizeof(Vec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<Vec2>()), "Vec2") && ok;
        property("Vec2", "float x", offsetof(Vec2, x));
        property("Vec2", "float y", offsetof(Vec2, y));
        behaviour("Vec2", asBEHAVE_CONSTRUCT, "void f()", NX_CONSTRUCTOR(Vec2, ()));
        behaviour("Vec2", asBEHAVE_CONSTRUCT, "void f(float, float)", NX_CONSTRUCTOR(Vec2, (float, float)));
        method("Vec2", "Vec2 opAdd(const Vec2 &in) const", NX_METHOD_PR(Vec2, operator+, (const Vec2&) const, Vec2));
        method("Vec2", "Vec2 opSub(const Vec2 &in) const", NX_METHOD_PR(Vec2, operator-, (const Vec2&) const, Vec2));
        method("Vec2", "Vec2 opMul(float) const", NX_METHOD_PR(Vec2, operator*, (float) const, Vec2));
        method("Vec2", "Vec2 opMul_r(float) const", NX_OBJ_LAST(scaleVec2));
        method("Vec2", "Vec2 opDiv(float) const", NX_METHOD_PR(Vec2, operator/, (float) const, Vec2));
        method("Vec2", "Vec2& opAddAssign(const Vec2 &in)", NX_OBJ_LAST(addVec2));
        method("Vec2", "Vec2& opSubAssign(const Vec2 &in)", NX_OBJ_LAST(subtractVec2));
        method("Vec2", "Vec2& opMulAssign(float)", NX_OBJ_LAST(multiplyVec2));
        method("Vec2", "Vec2 opNeg() const", NX_METHOD_PR(Vec2, operator-, () const, Vec2));
        method("Vec2", "bool opEquals(const Vec2 &in) const", NX_METHOD_PR(Vec2, operator==, (const Vec2&) const, bool));
        method("Vec2", "float dot(const Vec2 &in) const", NX_METHOD(Vec2, dot));

        ok = host.check(e->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<Vec3>()), "Vec3") && ok;
        property("Vec3", "float x", offsetof(Vec3, x));
        property("Vec3", "float y", offsetof(Vec3, y));
        property("Vec3", "float z", offsetof(Vec3, z));
        behaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", NX_CONSTRUCTOR(Vec3, ()));
        behaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", NX_CONSTRUCTOR(Vec3, (float, float, float)));
        method("Vec3", "Vec3 opAdd(const Vec3 &in) const", NX_METHOD_PR(Vec3, operator+, (const Vec3&) const, Vec3));
        method("Vec3", "Vec3 opSub(const Vec3 &in) const", NX_METHOD_PR(Vec3, operator-, (const Vec3&) const, Vec3));
        method("Vec3", "Vec3 opMul(float) const", NX_METHOD_PR(Vec3, operator*, (float) const, Vec3));
        method("Vec3", "Vec3 opMul_r(float) const", NX_OBJ_LAST(scaleVec3));
        method("Vec3", "Vec3 opDiv(float) const", NX_METHOD_PR(Vec3, operator/, (float) const, Vec3));
        method("Vec3", "Vec3& opAddAssign(const Vec3 &in)", NX_OBJ_LAST(addVec3));
        method("Vec3", "Vec3& opSubAssign(const Vec3 &in)", NX_OBJ_LAST(subtractVec3));
        method("Vec3", "Vec3& opMulAssign(float)", NX_OBJ_LAST(multiplyVec3));
        method("Vec3", "Vec3 opNeg() const", NX_METHOD_PR(Vec3, operator-, () const, Vec3));
        method("Vec3", "bool opEquals(const Vec3 &in) const", NX_METHOD_PR(Vec3, operator==, (const Vec3&) const, bool));
        method("Vec3", "float dot(const Vec3 &in) const", NX_METHOD(Vec3, dot));
        method("Vec3", "Vec3 cross(const Vec3 &in) const", NX_METHOD(Vec3, cross));
        method("Vec3", "float length() const", NX_METHOD(Vec3, length));
        method("Vec3", "Vec3 normalized() const", NX_METHOD(Vec3, normalized));
        return ok;
    }
}
