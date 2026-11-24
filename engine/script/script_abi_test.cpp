#include "base/log.h"
#include "base/math.h"
#include "script/bind.h"
#include "script/script_host.h"

#include <string.h>
#include <string>

// Each function covers one shape of argument or return value: the places where a calling
// convention goes wrong are small structs, 64-bit values, floats, bools and method pointers.
namespace nx
{
    namespace
    {
        int addInt(int a, int b) { return a + b; }
        i64 addI64(i64 a, i64 b) { return a + b; }
        float addFloat(float a, float b) { return a + b; }
        double addDouble(double a, float b, double c) { return a + b + c; }
        bool notBool(bool a) { return !a; }
        u8 addU8(u8 a, u8 b) { return static_cast<u8>(a + b); }
        i32 subI16(i16 a, i16 b) { return a - b; }
        double mixed(int a, float b, double c, i64 d, u8 e, bool f, float g) { return a + b + c + static_cast<double>(d) + e + (f ? 1 : 0) + g; }
        Vec2 makeVec2(float x, float y) { return { x, y }; }
        Vec3 makeVec3(float x, float y, float z) { return { x, y, z }; }
        float sumVec2(Vec2 v) { return v.x + v.y; }
        float sumVec3(Vec3 v) { return v.x + v.y + v.z; }
        float sumRefs(const Vec2& a, const Vec3& b) { return a.x + a.y + b.x + b.y + b.z; }
        void writeOut(int& a, float& b) { a = 41; b = 2.5f; }
        std::string joinText(const std::string& a, int b, const std::string& c) { return a + std::to_string(b) + c; }
        std::string byValueText(std::string a) { return a + "!"; }

        struct Base
        {
            virtual ~Base() = default;
            virtual int kind() const { return 1; }
        };

        // Not at offset zero in Widget, so calling its methods needs the pointer adjusting.
        struct Named
        {
            std::string name = "widget";
            const std::string& getName() const { return name; }
            void setName(const std::string& value) { name = value; }
        };

        struct Widget : Base, Named
        {
            int value = 10;

            int kind() const override { return 2; }
            int add(int amount) { return value += amount; }
            float scale(float s) const { return value * s; }
            Vec2 pair() const { return { static_cast<float>(value), 0.5f }; }
            Vec3 triple() const { return { 1.0f, 2.0f, 3.0f }; }
            i64 big() const { return 0x1122334455667788LL; }
        };

        Widget g_widget;
        Widget* getWidget() { return &g_widget; }

        const char* const kScript = R"(
int failures = 0;
int checks = 0;

void expect(bool ok, const string &in what)
{
    checks++;
    if (!ok)
    {
        failures++;
        print("ABI FAIL: " + what);
    }
}

int run()
{
    expect(abi::addInt(2, 3) == 5, "int");
    expect(abi::addI64(0x100000000, 5) == 0x100000005, "int64");
    expect(abi::addFloat(1.5f, 2.25f) == 3.75f, "float");
    expect(abi::addDouble(1.5, 0.25f, 2.0) == 3.75, "double");
    expect(abi::notBool(false), "bool true");
    expect(!abi::notBool(true), "bool false");
    expect(abi::addU8(200, 100) == 44, "uint8");
    expect(abi::subI16(-5, 10) == -15, "int16");
    expect(abi::mixed(1, 0.5f, 2.25, 1000000000000, 7, true, 0.25f) == 1000000000012.0, "mixed");

    Vec2 a = abi::makeVec2(1, 2);
    expect(a.x == 1 && a.y == 2, "return Vec2");
    Vec3 b = abi::makeVec3(1, 2, 3);
    expect(b.x == 1 && b.y == 2 && b.z == 3, "return Vec3");
    expect(abi::sumVec2(a) == 3, "Vec2 by value");
    expect(abi::sumVec3(b) == 6, "Vec3 by value");
    expect(abi::sumRefs(a, b) == 9, "references");
    expect(abi::sumVec3(abi::makeVec3(4, 5, 6)) == 15, "nested call");

    int outInt; float outFloat;
    abi::writeOut(outInt, outFloat);
    expect(outInt == 41 && outFloat == 2.5f, "out references");

    expect(abi::joinText("a", 5, "b") == "a5b", "return string");
    expect(abi::byValueText("hey") == "hey!", "string by value");

    // Bugs 24 and 25 in the old engine: == and scaling.
    expect(Vec2(1, 2) == Vec2(1, 2) && Vec2(1, 2) != Vec2(2, 1), "Vec2 ==");
    expect(Vec3(1, 2, 3) == Vec3(1, 2, 3), "Vec3 ==");
    expect(Vec2(1, 2) * 2.0f == Vec2(2, 4) && 2.0f * Vec2(1, 2) == Vec2(2, 4), "Vec2 scale");
    expect(Vec3(2, 4, 6) / 2.0f == Vec3(1, 2, 3) && 2.0f * Vec3(1, 2, 3) == Vec3(2, 4, 6), "Vec3 scale");
    expect(-Vec2(1, 2) == Vec2(-1, -2) && (Vec3(1, 2, 3) - Vec3(1, 1, 1)) == Vec3(0, 1, 2), "negate and subtract");
    expect(Vec3(1, 0, 0).cross(Vec3(0, 1, 0)) == Vec3(0, 0, 1) && Vec3(3, 4, 0).length() == 5, "cross and length");

    Widget@ w = abi::getWidget();
    expect(w.add(5) == 15 && w.value == 15, "method");
    expect(w.scale(2.0f) == 30, "method float");
    expect(w.kind() == 2, "virtual method");
    expect(w.pair() == Vec2(15, 0.5f), "method returns Vec2");
    expect(w.triple() == Vec3(1, 2, 3), "method returns Vec3");
    expect(w.big() == 0x1122334455667788, "method returns int64");
    expect(w.name == "widget", "second base getter");
    w.name = "renamed";
    expect(w.name == "renamed", "second base setter");

    array<Vec3> list;
    for (int i = 0; i < 100; i++)
        list.insertLast(Vec3(i, i, i));
    float total = 0;
    foreach (Vec3 v : list)
        total += v.x;
    expect(total == 4950, "array and foreach");
    return failures;
}

int count() { return checks; }
)";
    }

    int runScriptAbiTests(ScriptHost& host, int& checks)
    {
        asIScriptEngine* e = host.engine();
        bool ok = true;
        const auto global = [&](const char* declaration, const asSFuncPtr& fn, asDWORD convention)
        {
            ok = host.check(e->RegisterGlobalFunction(declaration, fn, convention), declaration) && ok;
        };
        const auto method = [&](const char* declaration, const asSFuncPtr& fn, asDWORD convention)
        {
            ok = host.check(e->RegisterObjectMethod("Widget", declaration, fn, convention), declaration) && ok;
        };

        g_widget = Widget();

        ok = host.check(e->RegisterObjectType("Widget", 0, asOBJ_REF | asOBJ_NOCOUNT), "Widget") && ok;
        method("int add(int)", NX_METHOD(Widget, add));
        method("float scale(float) const", NX_METHOD(Widget, scale));
        method("int kind() const", NX_METHOD(Widget, kind));
        method("Vec2 pair() const", NX_METHOD(Widget, pair));
        method("Vec3 triple() const", NX_METHOD(Widget, triple));
        method("int64 big() const", NX_METHOD(Widget, big));
        method("const string &get_name() const property", NX_METHOD(Widget, getName));
        method("void set_name(const string &in) property", NX_METHOD(Widget, setName));
        ok = host.check(e->RegisterObjectProperty("Widget", "int value", asOFFSET(Widget, value)), "value") && ok;

        e->SetDefaultNamespace("abi");
        global("int addInt(int, int)", NX_FN(addInt));
        global("int64 addI64(int64, int64)", NX_FN(addI64));
        global("float addFloat(float, float)", NX_FN(addFloat));
        global("double addDouble(double, float, double)", NX_FN(addDouble));
        global("bool notBool(bool)", NX_FN(notBool));
        global("uint8 addU8(uint8, uint8)", NX_FN(addU8));
        global("int subI16(int16, int16)", NX_FN(subI16));
        global("double mixed(int, float, double, int64, uint8, bool, float)", NX_FN(mixed));
        global("Vec2 makeVec2(float, float)", NX_FN(makeVec2));
        global("Vec3 makeVec3(float, float, float)", NX_FN(makeVec3));
        global("float sumVec2(Vec2)", NX_FN(sumVec2));
        global("float sumVec3(Vec3)", NX_FN(sumVec3));
        global("float sumRefs(const Vec2 &in, const Vec3 &in)", NX_FN(sumRefs));
        global("void writeOut(int &out, float &out)", NX_FN(writeOut));
        global("string joinText(const string &in, int, const string &in)", NX_FN(joinText));
        global("string byValueText(string)", NX_FN(byValueText));
        global("Widget@ getWidget()", NX_FN(getWidget));
        e->SetDefaultNamespace("");

        int failures = -1;
        if (!ok || !host.compile("abi", "abi.as", kScript, strlen(kScript)) || !host.callInt(host.find("abi", "int run()"), failures))
        {
            return -1;
        }
        host.callInt(host.find("abi", "int count()"), checks);
        host.discard("abi");
        return failures;
    }
}
