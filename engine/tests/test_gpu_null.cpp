#include "test.h"

#include "gpu/gpu.h"

using namespace nx;

NX_TEST(nullDeviceValidatesDraws)
{
    const size_t before = memStatsTotal().count;
    gpu::Device* device = gpu::createNullDevice(640, 480);
    NX_EXPECT(device != nullptr);
    const gpu::Vertex vertices[3] = {};
    const gpu::Buffer buffer = device->createVertexBuffer(vertices, 3);
    const gpu::Pipeline pipeline = device->createPipeline({});
    NX_EXPECT(buffer && pipeline);
    NX_EXPECT(!device->createVertexBuffer(nullptr, 3));

    gpu::Draw good;
    good.pipeline = pipeline;
    good.vertices = buffer;
    good.count = 3;
    gpu::Draw outOfRange = good;
    outOfRange.first = 1;
    gpu::Draw overflow = good;
    overflow.first = 0xFFFFFFFFu;
    overflow.count = 4;

    device->beginFrame({});
    device->draw(good);
    device->draw(outOfRange);
    device->draw(overflow);
    device->destroyBuffer(buffer);
    device->draw(good);  // stale handle
    device->endFrame();

    const gpu::Stats stats = device->stats();
    NX_EXPECT(stats.draws == 1 && stats.vertices == 3 && stats.rejected == 3 && stats.bufferBytes == 0);
    destroy(device);
    NX_EXPECT(memStatsTotal().count == before);
}
