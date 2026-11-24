#include "test.h"

#include "base/endian.h"
#include "base/handle.h"
#include "base/math.h"
#include "base/memory.h"

#include <math.h>

using namespace nx;

namespace
{
    bool near(float a, float b) { return fabsf(a - b) < 1e-5f; }

    struct Counted
    {
        static int live;
        int value = 0;

        explicit Counted(int v) : value(v) { live++; }
        Counted(Counted&& o) noexcept : value(o.value) { live++; }
        ~Counted() { live--; }
    };
    int Counted::live = 0;

    struct CountedTag;
}

NX_TEST(memoryIsTaggedAndAligned)
{
    const MemStats before = memStats(MemTag::UI);
    void* a = memAlloc(100, MemTag::UI);
    void* b = memAlloc(1, MemTag::UI, 256);
    NX_EXPECT(a && reinterpret_cast<uintptr_t>(a) % 16 == 0);
    NX_EXPECT(b && reinterpret_cast<uintptr_t>(b) % 256 == 0);
    NX_EXPECT(memStats(MemTag::UI).bytes == before.bytes + 101);
    NX_EXPECT(memStats(MemTag::UI).count == before.count + 2);
    memFree(a);
    memFree(b);
    memFree(nullptr);
    NX_EXPECT(memStats(MemTag::UI).bytes == before.bytes);
    NX_EXPECT(memStats(MemTag::UI).count == before.count);
    NX_EXPECT(memStats(MemTag::UI).peak >= before.bytes + 101);
}

NX_TEST(memoryBudgetIsEnforced)
{
    memSetBudget(MemTag::Audio, memStats(MemTag::Audio).bytes + 1000);
    void* a = memAlloc(800, MemTag::Audio);
    void* b = memAlloc(300, MemTag::Audio);
    NX_EXPECT(a != nullptr);
    NX_EXPECT(b == nullptr);
    memFree(a);
    b = memAlloc(300, MemTag::Audio);
    NX_EXPECT(b != nullptr);
    memFree(b);
    memSetBudget(MemTag::Audio, 0);
    NX_EXPECT(memAlloc(SIZE_MAX - 4, MemTag::Audio) == nullptr);
    NX_EXPECT(memStats(MemTag::Audio).bytes == 0);
}

NX_TEST(arenaResetsAndRefusesWhenFull)
{
    Arena arena;
    NX_EXPECT(arena.init(256, MemTag::Frame));
    u8* a = static_cast<u8*>(arena.alloc(10, 1));
    u8* b = static_cast<u8*>(arena.alloc(16));
    NX_EXPECT(a && b && b >= a + 10 && reinterpret_cast<uintptr_t>(b) % 16 == 0);
    NX_EXPECT(arena.alloc(1000) == nullptr);
    NX_EXPECT(arena.alloc(SIZE_MAX) == nullptr);
    arena.reset();
    NX_EXPECT(arena.used() == 0 && arena.peak() == 32);
    NX_EXPECT(arena.alloc(256) == a);
}

NX_TEST(staleHandlesAreDetected)
{
    {
        HandlePool<Counted, CountedTag> pool;
        NX_EXPECT(pool.init(2, MemTag::Scene));
        const Handle<CountedTag> a = pool.add(Counted(1));
        const Handle<CountedTag> b = pool.add(Counted(2));
        NX_EXPECT(a && b && a != b);
        NX_EXPECT(!pool.add(Counted(3)));
        NX_EXPECT(pool.get(a)->value == 1 && pool.get(b)->value == 2);

        NX_EXPECT(pool.remove(a));
        NX_EXPECT(pool.get(a) == nullptr);
        NX_EXPECT(!pool.remove(a));

        const Handle<CountedTag> c = pool.add(Counted(4));
        NX_EXPECT(c != a && pool.get(a) == nullptr && pool.get(c)->value == 4);
        NX_EXPECT(pool.get({}) == nullptr && pool.get({ 0xFFFFFFFFu }) == nullptr);
        NX_EXPECT(pool.count() == 2 && Counted::live == 2);
    }
    NX_EXPECT(Counted::live == 0);
    NX_EXPECT(memStats(MemTag::Scene).bytes == 0);
}

NX_TEST(handlePoolReusesWithoutAllocating)
{
    HandlePool<Counted, CountedTag> pool;
    NX_EXPECT(pool.init(16, MemTag::Scene));
    const size_t before = memStatsTotal().totalCount;
    Handle<CountedTag> last;
    bool unique = true;
    for (int i = 0; i < 100000; i++)
    {
        const Handle<CountedTag> h = pool.add(Counted(i));
        unique = unique && h && h != last;
        pool.remove(h);
        last = h;
    }
    NX_EXPECT(unique);
    NX_EXPECT(memStatsTotal().totalCount == before);
}

NX_TEST(filesReadTheSameOnAnyByteOrder)
{
    const u8 bytes[] = { 0x01, 0x02, 0x03, 0x84, 0x05, 0x06, 0x07, 0x88 };
    NX_EXPECT(readU16(bytes) == 0x0201);
    NX_EXPECT(readU32(bytes) == 0x84030201u);
    NX_EXPECT(readU64(bytes) == 0x8807060584030201ull);
    NX_EXPECT(readU32(bytes + 1) == 0x05840302u);  // unaligned
    u8 out[8];
    writeU64(out, 0x8807060584030201ull);
    bool same = true;
    for (int i = 0; i < 8; i++)
    {
        same = same && out[i] == bytes[i];
    }
    NX_EXPECT(same);
}

NX_TEST(matrices)
{
    const Vec3 p = (Mat4::translation({ 10, 20, 30 }) * Mat4::scale({ 2, 2, 2 })).transformPoint({ 1, 2, 3 });
    NX_EXPECT(near(p.x, 12) && near(p.y, 24) && near(p.z, 36));
    const Vec3 r = Mat4::rotationZ(3.14159265f / 2).transformPoint({ 1, 0, 0 });
    NX_EXPECT(near(r.x, 0) && near(r.y, 1));
    const Mat4 o = Mat4::ortho(0, 640, 480, 0, 0, 1);
    const Vec3 tl = o.transformPoint({ 0, 0, 0 });
    const Vec3 br = o.transformPoint({ 640, 480, 1 });
    NX_EXPECT(near(tl.x, -1) && near(tl.y, 1) && near(tl.z, 0));
    NX_EXPECT(near(br.x, 1) && near(br.y, -1) && near(br.z, 1));
}
