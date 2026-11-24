#pragma once

#include "base/handle.h"
#include "base/math.h"
#include "base/types.h"

// The GPU interface. There are no state setters: a pipeline is immutable, and every draw says
// everything it needs, so nothing can leak from one draw to the next.
namespace nx::gpu
{
    struct Color
    {
        float r = 0, g = 0, b = 0, a = 1;
    };

    // Bytes in memory are r, g, b, a on every platform. Backends convert at upload if needed.
    struct Vertex
    {
        float x, y, z;
        u8 r, g, b, a;
        float u, v;
    };
    static_assert(sizeof(Vertex) == 24);

    enum class Topology : u8 { Triangles, Lines };
    enum class Blend : u8 { None, Alpha, Additive };
    enum class Cull : u8 { None, Back };
    enum class Filter : u8 { Linear, Nearest };

    struct PipelineDesc
    {
        Topology topology = Topology::Triangles;
        Blend blend = Blend::None;
        Cull cull = Cull::None;  // front faces are counter-clockwise
        bool depthTest = false;
        bool depthWrite = false;
    };

    using Buffer = Handle<struct BufferTag>;
    using Pipeline = Handle<struct PipelineTag>;
    using Texture = Handle<struct TextureTag>;

    // Pixels from the top left. Empty means nothing is drawn.
    struct Scissor
    {
        bool enabled = false;
        i32 x = 0, y = 0, width = 0, height = 0;
    };

    struct Draw
    {
        Pipeline pipeline;
        Buffer vertices;  // none: the vertices come from stream()
        u32 first = 0;
        u32 count = 0;
        Texture texture;  // none: plain white. Multiplied by the vertex colour.
        Filter filter = Filter::Linear;
        Scissor scissor;
        Mat4 transform;  // model to clip space
    };

    enum class PixelFormat : u8
    {
        Rgba8,  // 4 bytes a pixel
        Bc1,    // half a byte a pixel, no alpha (DXT1)
        Bc3     // one byte a pixel (DXT5)
    };

    inline size_t pixelBytes(PixelFormat format, u32 width, u32 height)
    {
        const size_t blocks = static_cast<size_t>((width + 3) / 4) * ((height + 3) / 4);
        return format == PixelFormat::Rgba8 ? static_cast<size_t>(width) * height * 4 : blocks * (format == PixelFormat::Bc1 ? 8 : 16);
    }

    // The image is the top left of what is stored, which may be larger.
    struct TextureData
    {
        u32 width = 0, height = 0;
        u32 fullWidth = 0, fullHeight = 0;
        PixelFormat format = PixelFormat::Rgba8;
        const u8* pixels = nullptr;  // top row first
    };

    struct TextureInfo
    {
        u32 width = 0;   // of the image
        u32 height = 0;
        float uMax = 1;  // the image may sit in the corner of a larger texture
        float vMax = 1;
        size_t bytes = 0;
    };

    struct Stats
    {
        u32 draws = 0;     // last finished frame
        u32 vertices = 0;  // last finished frame
        u32 rejected = 0;  // last finished frame: draws with a bad handle or range
        size_t bufferBytes = 0;
        size_t textureBytes = 0;
    };

    // Vertices that stream() can take in one frame.
    constexpr u32 kStreamVertices = 65536;
    // Largest texture side that every target supports.
    constexpr u32 kMaxTextureSize = 2048;
    constexpr u32 kStreamFull = 0xFFFFFFFFu;

    class Device
    {
    public:
        virtual ~Device() = default;

        virtual const char* name() const = 0;
        virtual void size(u32& width, u32& height) const = 0;

        virtual Buffer createVertexBuffer(const Vertex* vertices, u32 count) = 0;
        virtual void destroyBuffer(Buffer buffer) = 0;

        virtual Pipeline createPipeline(const PipelineDesc& desc) = 0;
        virtual void destroyPipeline(Pipeline pipeline) = 0;

        // Compressed sizes must be multiples of four and, on the original Xbox, powers of two.
        virtual Texture createTexture(const TextureData& data) = 0;
        virtual void destroyTexture(Texture texture) = 0;
        virtual TextureInfo textureInfo(Texture texture) = 0;

        // False when there is nothing to draw to this frame (a minimised window); skip the frame.
        virtual bool beginFrame(const Color& clear) = 0;
        // Copies vertices that last for this frame only. Returns where they start, for Draw::first,
        // or kStreamFull.
        virtual u32 stream(const Vertex* vertices, u32 count) = 0;
        virtual void draw(const Draw& draw) = 0;
        virtual void endFrame() = 0;

        // Copies the last finished frame as RGBA8 rows, top row first. False if unsupported.
        virtual bool capture(u8* pixels, size_t size) = 0;

        virtual Stats stats() const = 0;
    };

    // The platform's own GPU. The window is Platform::nativeWindow().
    // Both return nullptr on failure, after logging why. Release with nx::destroy.
    Device* createDevice(void* nativeWindow);
    Device* createNullDevice(u32 width, u32 height);

    inline bool validTextureSize(u32 width, u32 height)
    {
        return width > 0 && height > 0 && width <= kMaxTextureSize && height <= kMaxTextureSize;
    }

    inline bool validTexture(const TextureData& t)
    {
        return t.pixels && t.width > 0 && t.height > 0 && t.width <= t.fullWidth && t.height <= t.fullHeight && validTextureSize(t.fullWidth, t.fullHeight) &&
               (t.format == PixelFormat::Rgba8 || (t.fullWidth % 4 == 0 && t.fullHeight % 4 == 0));
    }

    inline TextureData rgbaTexture(const u8* pixels, u32 width, u32 height)
    {
        return { width, height, width, height, PixelFormat::Rgba8, pixels };
    }
}
