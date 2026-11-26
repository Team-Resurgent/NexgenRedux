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

    // Of a mesh. Laid out the same on every platform, like Vertex.
    struct MeshVertex
    {
        float x, y, z;
        float nx, ny, nz;
        u8 r, g, b, a;
        float u, v;
    };
    static_assert(sizeof(MeshVertex) == 36);

    using Mesh = Handle<struct MeshTag>;
    using Effect = Handle<struct EffectTag>;
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
        bool mips = false;           // smaller copies for when it is far away; Rgba8 only
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
        u32 triangles = 0; // last finished frame, of meshes
        u32 rejected = 0;  // last finished frame: draws with a bad handle or range
        size_t bufferBytes = 0;
        size_t textureBytes = 0;
    };

    // --- Three dimensions: y up, right-handed, depth 0 near to 1 far, front faces counter-clockwise ---

    constexpr u32 kLights = 16;        // in a view
    constexpr u32 kMeshLights = 4;     // on a mesh
    constexpr u32 kFieldShapes = 24;
    constexpr u32 kShadowSize = 2048;

    struct PointLight
    {
        Vec3 place;
        float range = 1;
        Color color;
    };

    // What everything in a picture has in common.
    struct View
    {
        Mat4 view;               // world to camera
        Mat4 projection;         // camera to clip space
        Vec3 eye, right, up, forward;  // of the camera; right and up reach the edges at distance 1
        Vec3 sunDirection = { 0, 1, 0 };  // towards it
        Color sunColor, ambientSky, ambientGround;
        Color fogColor, skyTop, skyHorizon;
        float fogStart = 1e6f, fogEnd = 2e6f;
        float time = 0;
        PointLight lights[kLights];
        u32 lightCount = 0;
        Mat4 shadows;            // world to the clip space that shadows were drawn in
        float shadowStrength = 0;  // 0: none
        Scissor area;            // where on the screen; not enabled: all of it
    };

    struct MeshDraw
    {
        Mesh mesh;
        u32 first = 0, count = 0;  // indices; count 0 is all from first
        Mat4 world;
        Texture texture;
        Color color = { 1, 1, 1, 1 };  // multiplies the vertices' and the texture's
        float glow = 0;          // 1 is as bright as its colour whatever the light
        float shine = 0;         // of a highlight from the sun
        Blend blend = Blend::None;
        Cull cull = Cull::Back;
        bool depthWrite = true;
        bool indoors = false;    // the sun doesn't reach it
        // The wind: how far each vertex moves, along x and z, for each metre of its v, at the strongest gust. Where
        // vertices can't be moved, it stays as it is.
        float bendX = 0, bendZ = 0;
        float cutout = 0;        // where the picture's alpha is less, nothing is drawn
        // Coloured, instead of by its picture, by a volume at where each part of it is in the world, times scale, plus
        // offset: for fog, smoke and the like.
        Texture volume;
        float volumeScale = 0;
        Vec3 volumeOffset;
        bool lines = false;      // only the edges of its triangles
        bool lit = true;
        bool fogged = true;
        bool repeat = false;     // the texture
        u8 lights[kMeshLights] = {};
        u8 lightCount = 0;
        Effect effect;             // how it is coloured, and moved; none: as it is
        const float* values = nullptr;  // sixteen, for the effect
    };

    enum class FieldKind : u8 { Sphere, Box, Torus, Capsule, Plane, Cylinder };
    enum class FieldOperation : u8 { Join, Cut, Keep };

    // One of the shapes in a picture drawn by marching along rays.
    struct FieldShape
    {
        FieldKind kind = FieldKind::Sphere;
        FieldOperation operation = FieldOperation::Join;
        Vec3 place;
        Vec3 size = { 1, 1, 1 };  // radius; half sizes; radii of ring and tube; radius and half height
        float soft = 0;          // how far it melts into what came before
        float turn = 0;          // radians about the vertical
        Color color;
    };

    struct Field
    {
        FieldShape shapes[kFieldShapes];
        u32 count = 0;
        u32 steps = 80;
        float farthest = 60;
        float repeat = 0;        // the whole thing again, every so far across and along; 0: not
        float shine = 0.3f;
        float reflection = 0;
    };

    // A shader that someone else has written: over a picture, or on a mesh.
    enum class EffectKind : u8 { Picture, Surface };

    // Light that spills from what is bright.
    struct Bloom
    {
        float strength = 0.8f;
        float threshold = 0.7f;   // how bright
        float spread = 1;
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
        // A cube of pixels, r, g, b, a, size on each side (a power of two), that repeats. Destroyed as a texture. None
        // where there are none.
        virtual Texture createVolume(const u8* pixels, u32 size) { (void)pixels; (void)size; return {}; }
        virtual void destroyTexture(Texture texture) = 0;
        virtual TextureInfo textureInfo(Texture texture) = 0;

        // False when there is nothing to draw to this frame (a minimised window); skip the frame.
        virtual bool beginFrame(const Color& clear) = 0;
        // Copies vertices that last for this frame only. Returns where they start, for Draw::first,
        // or kStreamFull.
        virtual u32 stream(const Vertex* vertices, u32 count) = 0;
        virtual void draw(const Draw& draw) = 0;
        virtual void endFrame() = 0;

        // --- Three dimensions, between beginFrame and endFrame. The 2D draws can come before and after. ---

        // Up to 65536 vertices. A dynamic one's vertices can be replaced, once a frame.
        virtual Mesh createMesh(const MeshVertex* vertices, u32 vertexCount, const u16* indices, u32 indexCount, bool dynamic) = 0;
        virtual void updateMesh(Mesh mesh, const MeshVertex* vertices, u32 vertexCount) = 0;
        virtual void destroyMesh(Mesh mesh) = 0;

        // Starts a picture, over what has been drawn but nearer than all of it.
        virtual void beginView(const View& view) = 0;
        virtual void drawMesh(const MeshDraw& draw) = 0;
        // Behind everything else. False if this GPU can't: draw a dome instead.
        virtual bool drawSky() = 0;
        // Shapes by their distance, mixing with meshes. False if this GPU can't: make a mesh instead.
        virtual bool drawField(const Field& field) = 0;

        // Draws what casts shadows, seen from the sun, before beginView. False if this GPU can't.
        virtual bool beginShadows(const Mat4& sunViewProjection) = 0;
        virtual void endShadows() = 0;
        // How many pixels across they are.
        virtual u32 shadowSize() const { return kShadowSize; }

        // --- Effects. A GPU without shaders has none: then things are drawn as they are. ---

        // From source that either shader language can read; see gpu/effect_source.h. No effect, and why, if
        // it can't be made.
        virtual Effect createEffect(EffectKind kind, const char* source, char* error, size_t errorSize)
        {
            (void)kind;
            (void)source;
            if (errorSize)
            {
                error[0] = 0;
            }
            return {};
        }
        virtual void destroyEffect(Effect effect) { (void)effect; }
        // Puts all that has been drawn so far through one. The area is none for all of it.
        virtual bool applyEffect(Effect effect, const float (&values)[16], float time, const Scissor& area)
        {
            (void)effect;
            (void)values;
            (void)time;
            (void)area;
            return false;
        }
        virtual bool bloom(const Bloom& bloom, const Scissor& area)
        {
            (void)bloom;
            (void)area;
            return false;
        }

        // Copies the last finished frame as RGBA8 rows, top row first. False if unsupported.
        virtual bool capture(u8* pixels, size_t size) = 0;

        virtual Stats stats() const = 0;

        // Whether frames wait for the display, as they do to begin with. For measuring; not every GPU can.
        virtual void setWait(bool wait) { (void)wait; }
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
