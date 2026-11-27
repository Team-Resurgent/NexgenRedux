#include "gpu/gpu.h"

#include <xtl.h>
#include <xgraphics.h>

#include <math.h>
#include <string.h>

// Direct3D 8 on the original Xbox.
namespace nx::gpu
{
    namespace
    {
        constexpr DWORD kVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        // One more than the GPU can have queued, so writing this frame's vertices rarely waits.
        constexpr u32 kStreamFrames = 3;
        // Half what the interface allows: each frame's buffer comes out of the 64 MB.
        constexpr u32 kStreamCount = 16384;

        struct BufferSlot
        {
            IDirect3DVertexBuffer8* buffer;
            u32 count;
        };

        struct PipelineSlot
        {
            PipelineDesc desc;
        };

        constexpr u32 kShadowSide = 1024;
        constexpr u32 kBloomWidth = 160, kBloomHeight = 120;

        constexpr DWORD kMeshFormat = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

        struct MeshSlot
        {
            // A dynamic one has as many as there can be frames on their way, so none is written while it is read.
            IDirect3DVertexBuffer8* vertices[kStreamFrames];
            IDirect3DIndexBuffer8* indices;
            u32 vertexCount, indexCount;
            u32 current;
            bool dynamic;
        };

        struct TextureSlot
        {
            IDirect3DTexture8* texture;
            IDirect3DVolumeTexture8* volume;
            TextureInfo info;
            // A picture to draw into. It is linear, as anything drawn into must be here, and so is read in pixels rather
            // than 0 to 1: its uMax and vMax are its size.
            IDirect3DTexture8* depth;
            // One that is changed often, by updateTexture: linear too, so it needn't be swizzled each time.
            bool linear;
            // And a second, for lockTexture: what is written goes into the one not being drawn, and the two are swapped.
            IDirect3DTexture8* spare;
            u32 shown;   // the frame they were last swapped in
        };

        // D3DCOLOR is b, g, r, a in memory.
        void copyVertices(Vertex* out, const Vertex* in, u32 count)
        {
            for (u32 i = 0; i < count; i++)
            {
                Vertex v = in[i];
                v.r = in[i].b;
                v.b = in[i].r;
                out[i] = v;
            }
        }

        void copyVertices(MeshVertex* out, const MeshVertex* in, u32 count)
        {
            for (u32 i = 0; i < count; i++)
            {
                MeshVertex v = in[i];
                v.r = in[i].b;
                v.b = in[i].r;
                out[i] = v;
            }
        }

        // What can be done here instead of a shader someone wrote, if it says which with "// xbox: name".
        enum class StandIn : u8 { Lava, Dissolve, Hologram, Flag, Tube, Pixels, Ripple, Glitch, Grade, Ink, Count };
        const char* const kStandIns[] = { "lava", "dissolve", "hologram", "flag", "tube", "pixels", "ripple", "glitch", "grade", "ink" };
        static_assert(sizeof(kStandIns) / sizeof(kStandIns[0]) == static_cast<size_t>(StandIn::Count));

        struct EffectSlot
        {
            StandIn kind;
        };

        struct PictureVertex
        {
            float x, y, z, rhw;
            DWORD color;
            float u, v;
        };

        D3DCOLORVALUE colorValue(const Color& c)
        {
            const D3DCOLORVALUE v = { c.r, c.g, c.b, c.a };
            return v;
        }

        D3DCOLOR packed(const Color& c)
        {
            const auto part = [](float v) { return static_cast<DWORD>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255 + 0.5f); };
            return D3DCOLOR_ARGB(part(c.a), part(c.r), part(c.g), part(c.b));
        }

        u32 powerOfTwo(u32 value)
        {
            u32 p = 1;
            while (p < value)
            {
                p <<= 1;
            }
            return p;
        }

        class D3D8Device final : public Device
        {
        public:
            ~D3D8Device() override
            {
                if (m_device)
                {
                    m_device->SetTexture(0, nullptr);
                    m_device->SetStreamSource(0, nullptr, 0);
                    m_device->SetIndices(nullptr, 0);
                    m_device->SetPixelShader(0);
                    for (const DWORD shader : { m_shadowShader, m_toneShader, m_edgeShader, m_gradeShader })
                    {
                        if (shader)
                        {
                            m_device->DeletePixelShader(shader);
                        }
                    }
                }
                m_buffers.forEach([](BufferSlot& b) { b.buffer->Release(); });
                m_textures.forEach([](TextureSlot& t)
                {
                    if (t.texture)
                    {
                        t.texture->Release();
                    }
                    if (t.spare)
                    {
                        t.spare->Release();
                    }
                    if (t.volume)
                    {
                        t.volume->Release();
                    }
                });
                m_meshes.forEach([](MeshSlot& m) { releaseMesh(m); });
                for (IDirect3DTexture8* t : { m_shadowColor, m_shadowDepth, m_bloom[0], m_bloom[1], m_copy })
                {
                    if (t)
                    {
                        t->Release();
                    }
                }
                for (IDirect3DVertexBuffer8* stream : m_streams)
                {
                    if (stream)
                    {
                        stream->Release();
                    }
                }
                if (m_device)
                {
                    m_device->Release();
                }
                if (m_d3d)
                {
                    m_d3d->Release();
                }
            }

            bool init()
            {
                if (!m_buffers.init(1024, MemTag::Buffer) || !m_pipelines.init(64, MemTag::Buffer) || !m_textures.init(1024, MemTag::Texture) || !m_meshes.init(2048, MemTag::Buffer) || !m_effects.init(128, MemTag::Buffer))
                {
                    return false;
                }
                m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
                if (m_d3d == nullptr)
                {
                    NX_ERROR("d3d8: Direct3DCreate8 failed");
                    return false;
                }

                // The dashboard's video settings decide what the console may output.
                const DWORD video = XGetVideoFlags();
                const bool pal50 = XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I && !(video & XC_VIDEO_FLAGS_PAL_60Hz);
                m_width = 640;
                m_height = pal50 ? 576 : 480;

                D3DPRESENT_PARAMETERS pp = {};
                pp.BackBufferWidth = m_width;
                pp.BackBufferHeight = m_height;
                pp.BackBufferFormat = D3DFMT_X8R8G8B8;
                pp.BackBufferCount = 1;
                pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
                pp.EnableAutoDepthStencil = TRUE;
                pp.AutoDepthStencilFormat = D3DFMT_D24S8;
                pp.FullScreen_RefreshRateInHz = pal50 ? 50 : 60;
                pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;
                if (video & XC_VIDEO_FLAGS_WIDESCREEN)
                {
                    pp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
                }
                if (!pal50 && (video & XC_VIDEO_FLAGS_HDTV_480p))
                {
                    pp.Flags |= D3DPRESENTFLAG_PROGRESSIVE;
                }
                const HRESULT result = m_d3d->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &m_device);
                if (FAILED(result))
                {
                    NX_ERROR("d3d8: CreateDevice failed (0x%08lx)", static_cast<unsigned long>(result));
                    m_device = nullptr;
                    return false;
                }
                for (IDirect3DVertexBuffer8*& stream : m_streams)
                {
                    if (FAILED(m_device->CreateVertexBuffer(kStreamCount * sizeof(Vertex), D3DUSAGE_WRITEONLY, kVertexFormat, D3DPOOL_DEFAULT, &stream)))
                    {
                        NX_ERROR("d3d8: no memory for streamed vertices");
                        stream = nullptr;
                        return false;
                    }
                }
                makeShadowShader();
                const u8 white[4] = { 255, 255, 255, 255 };
                m_white = createTexture(rgbaTexture(white, 1, 1));
                return static_cast<bool>(m_white);
            }

            const char* name() const override { return "d3d8"; }

            void size(u32& width, u32& height) const override
            {
                width = m_width;
                height = m_height;
            }

            Buffer createVertexBuffer(const Vertex* vertices, u32 count) override
            {
                if (vertices == nullptr || count == 0)
                {
                    return {};
                }
                const UINT bytes = count * sizeof(Vertex);
                IDirect3DVertexBuffer8* buffer = nullptr;
                if (FAILED(m_device->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, kVertexFormat, D3DPOOL_DEFAULT, &buffer)))
                {
                    NX_ERROR("d3d8: CreateVertexBuffer failed");
                    return {};
                }
                BYTE* memory = nullptr;
                if (FAILED(buffer->Lock(0, bytes, &memory, 0)))
                {
                    buffer->Release();
                    return {};
                }
                copyVertices(reinterpret_cast<Vertex*>(memory), vertices, count);
                buffer->Unlock();

                const Buffer handle = m_buffers.add({ buffer, count });
                if (!handle)
                {
                    buffer->Release();
                    return {};
                }
                m_bufferBytes += bytes;
                return handle;
            }

            void destroyBuffer(Buffer handle) override
            {
                if (BufferSlot* b = m_buffers.get(handle))
                {
                    m_bufferBytes -= b->count * sizeof(Vertex);
                    b->buffer->Release();  // the Xbox keeps it alive until the GPU has finished with it
                    m_buffers.remove(handle);
                }
            }

            Pipeline createPipeline(const PipelineDesc& desc) override { return m_pipelines.add({ desc }); }
            void destroyPipeline(Pipeline handle) override { m_pipelines.remove(handle); }

            Texture createTexture(const TextureData& data) override
            {
                if (!validTexture(data))
                {
                    return {};
                }
                if (data.dynamic && data.format == PixelFormat::Rgba8)
                {
                    TextureSlot t = {};
                    if (FAILED(m_device->CreateTexture(data.fullWidth, data.fullHeight, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &t.texture)) ||
                        FAILED(m_device->CreateTexture(data.fullWidth, data.fullHeight, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &t.spare)))
                    {
                        NX_ERROR("d3d8: no memory for a %ux%u texture", static_cast<unsigned>(data.fullWidth), static_cast<unsigned>(data.fullHeight));
                        if (t.texture)
                        {
                            t.texture->Release();
                        }
                        return {};
                    }
                    t.linear = true;
                    t.info.width = data.width;
                    t.info.height = data.height;
                    t.info.uMax = static_cast<float>(data.width);
                    t.info.vMax = static_cast<float>(data.height);
                    t.info.bytes = static_cast<size_t>(data.fullWidth) * data.fullHeight * 4;
                    const Texture handle = m_textures.add(t);
                    if (!handle)
                    {
                        t.texture->Release();
                        t.spare->Release();
                        return {};
                    }
                    m_textureBytes += t.info.bytes * 2;
                    updateTexture(handle, data.pixels);
                    return handle;
                }
                // Textures must be powers of two here, so an image goes in the corner of a larger one with
                // its last row and column repeated, which keeps filtering clean at the edge.
                const u32 fullWidth = powerOfTwo(data.fullWidth);
                const u32 fullHeight = powerOfTwo(data.fullHeight);
                const bool compressed = data.format != PixelFormat::Rgba8;
                if (compressed && (fullWidth != data.fullWidth || fullHeight != data.fullHeight))
                {
                    NX_ERROR("d3d8: a %ux%u compressed texture: cook it for this console", static_cast<unsigned>(data.fullWidth), static_cast<unsigned>(data.fullHeight));
                    return {};
                }
                const size_t bytes = pixelBytes(data.format, fullWidth, fullHeight);
                const D3DFORMAT format = data.format == PixelFormat::Bc1 ? D3DFMT_DXT1 : data.format == PixelFormat::Bc3 ? D3DFMT_DXT5 : D3DFMT_A8R8G8B8;

                u32 levels = 1;
                while (data.mips && !compressed && ((fullWidth >> levels) || (fullHeight >> levels)))
                {
                    levels++;
                }
                u8* linear = nullptr;
                if (!compressed)
                {
                    linear = static_cast<u8*>(memAlloc(bytes, MemTag::Texture));
                    if (linear == nullptr)
                    {
                        NX_ERROR("d3d8: no memory to load a %ux%u texture", static_cast<unsigned>(data.width), static_cast<unsigned>(data.height));
                        return {};
                    }
                    for (u32 y = 0; y < fullHeight; y++)
                    {
                        const u8* row = data.pixels + static_cast<size_t>(y < data.fullHeight ? y : data.fullHeight - 1) * data.fullWidth * 4;
                        u8* out = linear + static_cast<size_t>(y) * fullWidth * 4;
                        for (u32 x = 0; x < fullWidth; x++, out += 4)
                        {
                            const u8* in = row + static_cast<size_t>(x < data.fullWidth ? x : data.fullWidth - 1) * 4;
                            out[0] = in[2];
                            out[1] = in[1];
                            out[2] = in[0];
                            out[3] = in[3];
                        }
                    }
                }

                IDirect3DTexture8* texture = nullptr;
                D3DLOCKED_RECT rect;
                bool ok = SUCCEEDED(m_device->CreateTexture(fullWidth, fullHeight, levels, 0, format, D3DPOOL_DEFAULT, &texture));
                if (ok && SUCCEEDED(texture->LockRect(0, &rect, nullptr, 0)))
                {
                    if (compressed)
                    {
                        memcpy(rect.pBits, data.pixels, bytes);
                    }
                    else
                    {
                        XGSwizzleRect(linear, fullWidth * 4, nullptr, rect.pBits, fullWidth, fullHeight, nullptr, 4);
                    }
                    texture->UnlockRect(0);
                    // Each copy is the one before it, averaged.
                    u32 width = fullWidth, height = fullHeight;
                    for (u32 level = 1; level < levels; level++)
                    {
                        const u32 nextWidth = width > 1 ? width / 2 : 1, nextHeight = height > 1 ? height / 2 : 1;
                        for (u32 y = 0; y < nextHeight; y++)
                        {
                            for (u32 x = 0; x < nextWidth; x++)
                            {
                                const u8* a = linear + (static_cast<size_t>(y * height / nextHeight) * width + x * width / nextWidth) * 4;
                                const size_t right = width > 1 ? 4 : 0, below = height > 1 ? static_cast<size_t>(width) * 4 : 0;
                                u8* out = linear + (static_cast<size_t>(y) * nextWidth + x) * 4;
                                for (u32 k = 0; k < 4; k++)
                                {
                                    out[k] = static_cast<u8>((a[k] + a[k + right] + a[k + below] + a[k + right + below] + 2) / 4);
                                }
                            }
                        }
                        width = nextWidth;
                        height = nextHeight;
                        if (SUCCEEDED(texture->LockRect(level, &rect, nullptr, 0)))
                        {
                            XGSwizzleRect(linear, width * 4, nullptr, rect.pBits, width, height, nullptr, 4);
                            texture->UnlockRect(level);
                        }
                    }
                }
                else
                {
                    ok = false;
                    NX_ERROR("d3d8: no memory for a %ux%u texture", static_cast<unsigned>(fullWidth), static_cast<unsigned>(fullHeight));
                }
                memFree(linear);

                Texture handle;
                if (ok)
                {
                    TextureInfo info;
                    info.width = data.width;
                    info.height = data.height;
                    info.uMax = static_cast<float>(data.width) / static_cast<float>(fullWidth);
                    info.vMax = static_cast<float>(data.height) / static_cast<float>(fullHeight);
                    info.bytes = levels > 1 ? bytes + bytes / 3 : bytes;
                    handle = m_textures.add({ texture, nullptr, info, nullptr, false, nullptr, 0 });
                }
                if (!handle && texture)
                {
                    texture->Release();
                }
                m_textureBytes += handle ? m_textures.get(handle)->info.bytes : 0;
                return handle;
            }

            Texture createVolume(const u8* pixels, u32 size) override
            {
                if (pixels == nullptr || size == 0 || size > 128 || (size & (size - 1)) != 0)
                {
                    return {};
                }
                const u32 bytes = size * size * size * 4;
                u8* linear = static_cast<u8*>(memAlloc(bytes, MemTag::Texture));
                if (linear == nullptr)
                {
                    return {};
                }
                for (u32 i = 0; i < size * size * size; i++)
                {
                    linear[i * 4 + 0] = pixels[i * 4 + 2];
                    linear[i * 4 + 1] = pixels[i * 4 + 1];
                    linear[i * 4 + 2] = pixels[i * 4 + 0];
                    linear[i * 4 + 3] = pixels[i * 4 + 3];
                }
                IDirect3DVolumeTexture8* volume = nullptr;
                D3DLOCKED_BOX box;
                bool ok = SUCCEEDED(m_device->CreateVolumeTexture(size, size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &volume));
                if (ok && SUCCEEDED(volume->LockBox(0, &box, nullptr, 0)))
                {
                    // The whole of it, to the start: given, not left out.
                    D3DBOX all = {};
                    all.Right = all.Bottom = all.Back = size;
                    XGPOINT3D start = {};
                    XGSwizzleBox(linear, size * 4, size * size * 4, &all, box.pBits, size, size, size, &start, 4);
                    volume->UnlockBox(0);
                }
                else
                {
                    ok = false;
                }
                memFree(linear);
                TextureSlot t = {};
                t.volume = volume;
                t.info.width = t.info.height = size;
                t.info.bytes = bytes;
                const Texture handle = ok ? m_textures.add(t) : Texture();
                if (!handle)
                {
                    if (volume)
                    {
                        volume->Release();
                    }
                    return {};
                }
                m_textureBytes += bytes;
                return handle;
            }

            // The GPU looks a colour up where the dot products of its parts fall: from where black is the near edge of the
            // first cell, and white the far edge of the last, not their middles. So each cell is what is halfway across
            // it.
            Texture createGrade(const u8* pixels, u32 size) override
            {
                if (pixels == nullptr || size < 2 || size > 64 || (size & (size - 1)) != 0)
                {
                    return {};
                }
                u8* moved = static_cast<u8*>(memAlloc(size * size * size * 4, MemTag::Texture));
                if (moved == nullptr)
                {
                    return {};
                }
                const float last = static_cast<float>(size - 1);
                u32 i0[64], i1[64];
                float f[64];
                for (u32 i = 0; i < size; i++)
                {
                    const float p = (static_cast<float>(i) + 0.5f) / static_cast<float>(size) * last;
                    i0[i] = static_cast<u32>(p) < size - 2 ? static_cast<u32>(p) : size - 2;
                    i1[i] = i0[i] + 1;
                    f[i] = p - static_cast<float>(i0[i]);
                }
                for (u32 b = 0; b < size; b++)
                {
                    for (u32 g = 0; g < size; g++)
                    {
                        for (u32 r = 0; r < size; r++)
                        {
                            float sum[4] = { 0, 0, 0, 0 };
                            for (u32 corner = 0; corner < 8; corner++)
                            {
                                const u32 x = corner & 1 ? i1[r] : i0[r], y = corner & 2 ? i1[g] : i0[g], z = corner & 4 ? i1[b] : i0[b];
                                const float w = (corner & 1 ? f[r] : 1 - f[r]) * (corner & 2 ? f[g] : 1 - f[g]) * (corner & 4 ? f[b] : 1 - f[b]);
                                const u8* from = pixels + (x + y * size + z * size * size) * 4;
                                for (u32 k = 0; k < 4; k++)
                                {
                                    sum[k] += static_cast<float>(from[k]) * w;
                                }
                            }
                            u8* out = moved + (r + g * size + b * size * size) * 4;
                            for (u32 k = 0; k < 4; k++)
                            {
                                out[k] = static_cast<u8>(sum[k] + 0.5f);
                            }
                        }
                    }
                }
                const Texture made = createVolume(moved, size);
                memFree(moved);
                return made;
            }

            void destroyTexture(Texture handle) override
            {
                if (TextureSlot* t = m_textures.get(handle))
                {
                    m_device->SetTexture(0, nullptr);
                    m_textureBytes -= t->info.bytes * (t->spare ? 2 : 1);
                    if (t->texture)
                    {
                        t->texture->Release();
                    }
                    if (t->spare)
                    {
                        t->spare->Release();
                    }
                    if (t->volume)
                    {
                        t->volume->Release();
                    }
                    if (t->depth)
                    {
                        t->depth->Release();
                    }
                    m_textures.remove(handle);
                    m_boundTexture = {};
                }
            }

            bool updateTexture(Texture handle, const u8* rgba) override
            {
                const TextureSlot* t = m_textures.get(handle);
                D3DLOCKED_RECT locked;
                if (t == nullptr || !t->linear || rgba == nullptr || FAILED(t->texture->LockRect(0, &locked, nullptr, 0)))
                {
                    return false;
                }
                // b, g, r, a here.
                for (u32 y = 0; y < t->info.height; y++)
                {
                    const u32* in = reinterpret_cast<const u32*>(rgba + static_cast<size_t>(y) * t->info.width * 4);
                    u32* out = reinterpret_cast<u32*>(static_cast<u8*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch);
                    for (u32 x = 0; x < t->info.width; x++)
                    {
                        const u32 p = in[x];
                        out[x] = (p & 0xFF00FF00u) | ((p & 0xFFu) << 16) | ((p >> 16) & 0xFFu);
                    }
                }
                t->texture->UnlockRect(0);
                return true;
            }

            u8* lockTexture(Texture handle, u32& pitch) override
            {
                TextureSlot* t = m_textures.get(handle);
                if (t == nullptr || t->spare == nullptr)
                {
                    return nullptr;
                }
                // The spare was last drawn before they were swapped. Unless that was this frame or the one before,
                // which the GPU may not have finished, it can be written now without waiting.
                const DWORD flags = m_frameNumber - t->shown >= 2 ? D3DLOCK_NOOVERWRITE : 0;
                D3DLOCKED_RECT locked;
                if (FAILED(t->spare->LockRect(0, &locked, nullptr, flags)))
                {
                    return nullptr;
                }
                pitch = static_cast<u32>(locked.Pitch);
                return static_cast<u8*>(locked.pBits);
            }

            void unlockTexture(Texture handle) override
            {
                if (TextureSlot* t = m_textures.get(handle); t && t->spare)
                {
                    t->spare->UnlockRect(0);
                    IDirect3DTexture8* drawn = t->texture;
                    t->texture = t->spare;
                    t->spare = drawn;
                    t->shown = m_frameNumber;
                    m_boundTexture = {};  // bound again, to draw the new one
                }
            }

            Texture createTarget(u32 width, u32 height) override
            {
                if (width == 0 || height == 0 || width > 1024 || height > 1024)
                {
                    return {};
                }
                TextureSlot t = {};
                if (FAILED(m_device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &t.texture)) ||
                    FAILED(m_device->CreateTexture(width, height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_LIN_D24S8, D3DPOOL_DEFAULT, &t.depth)))
                {
                    if (t.texture)
                    {
                        t.texture->Release();
                    }
                    return {};
                }
                // See-through until something is drawn.
                D3DLOCKED_RECT locked;
                if (SUCCEEDED(t.texture->LockRect(0, &locked, nullptr, 0)))
                {
                    memset(locked.pBits, 0, static_cast<size_t>(locked.Pitch) * height);
                    t.texture->UnlockRect(0);
                }
                t.info.width = width;
                t.info.height = height;
                t.info.uMax = static_cast<float>(width);
                t.info.vMax = static_cast<float>(height);
                t.info.bytes = static_cast<size_t>(width) * height * 8;
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    t.texture->Release();
                    t.depth->Release();
                    return {};
                }
                m_textureBytes += t.info.bytes;
                return handle;
            }

            bool beginTarget(Texture handle, const Color& clear) override
            {
                const TextureSlot* t = m_textures.get(handle);
                if (!m_inFrame || m_drawing || m_inShadows || t == nullptr || t->depth == nullptr)
                {
                    return false;
                }
                IDirect3DSurface8* color = nullptr;
                IDirect3DSurface8* depth = nullptr;
                t->texture->GetSurfaceLevel(0, &color);
                t->depth->GetSurfaceLevel(0, &depth);
                m_device->GetRenderTarget(&m_screenColor);
                m_device->GetDepthStencilSurface(&m_screenDepth);
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                m_boundTexture = {};
                const bool fine = SUCCEEDED(m_device->SetRenderTarget(color, depth));
                color->Release();
                depth->Release();
                if (!fine)
                {
                    m_device->SetRenderTarget(m_screenColor, m_screenDepth);
                    releaseScreen();
                    return false;
                }
                m_drawing = handle;
                m_screenWidth = m_width;
                m_screenHeight = m_height;
                m_width = t->info.width;
                m_height = t->info.height;
                m_mode = Mode::None;
                const D3DVIEWPORT8 viewport = { 0, 0, m_width, m_height, 0.0f, 1.0f };
                m_device->SetViewport(&viewport);
                m_device->SetScissors(0, FALSE, nullptr);
                m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_COLORVALUE(clear.r, clear.g, clear.b, clear.a), 1.0f, 0);
                return true;
            }

            void endTarget() override
            {
                if (!m_drawing)
                {
                    return;
                }
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                m_boundTexture = {};
                m_device->SetRenderTarget(m_screenColor, m_screenDepth);
                releaseScreen();
                m_drawing = {};
                m_width = m_screenWidth;
                m_height = m_screenHeight;
                m_mode = Mode::None;
                const D3DVIEWPORT8 viewport = { 0, 0, m_width, m_height, 0.0f, 1.0f };
                m_device->SetViewport(&viewport);
            }

            void releaseScreen()
            {
                for (IDirect3DSurface8** surface : { &m_screenColor, &m_screenDepth })
                {
                    if (*surface)
                    {
                        (*surface)->Release();
                        *surface = nullptr;
                    }
                }
            }

            TextureInfo textureInfo(Texture handle) override
            {
                const TextureSlot* t = m_textures.get(handle);
                return t ? t->info : TextureInfo();
            }

            bool beginFrame(const Color& clear) override
            {
                NX_ASSERT(!m_inFrame);
                m_inFrame = true;
                m_current = {};
                m_bound = {};
                m_boundTexture = {};
                m_streamed = 0;
                m_streamIndex = (m_streamIndex + 1) % kStreamFrames;

                m_device->SetScissors(0, FALSE, nullptr);
                m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_COLORVALUE(clear.r, clear.g, clear.b, clear.a), 1.0f, 0);
                m_device->BeginScene();

                m_mode = Mode::None;
                return true;
            }

            // Everything not covered by a pipeline, so a frame never depends on what ran before.
            void flatState()
            {
                const Mat4 identity;
                m_device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(identity.m));
                m_device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(identity.m));
                m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
                m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
                m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                m_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                m_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                m_device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                m_device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
                m_device->SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2);
                m_device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
                m_device->SetPixelShader(0);
                m_device->SetVertexShader(kVertexFormat);
            }

            u32 stream(const Vertex* vertices, u32 count) override
            {
                NX_ASSERT(m_inFrame && vertices != nullptr);
                if (count > kStreamCount - m_streamed)
                {
                    return kStreamFull;
                }
                BYTE* memory = nullptr;
                // The first lock of a frame waits if the GPU is still reading this buffer; later ones
                // write past what it has been told to draw.
                const DWORD flags = m_streamed == 0 ? 0 : D3DLOCK_NOOVERWRITE;
                IDirect3DVertexBuffer8* buffer = m_streams[m_streamIndex];
                if (FAILED(buffer->Lock(m_streamed * sizeof(Vertex), count * sizeof(Vertex), &memory, flags)))
                {
                    return kStreamFull;
                }
                copyVertices(reinterpret_cast<Vertex*>(memory), vertices, count);
                buffer->Unlock();
                const u32 first = m_streamed;
                m_streamed += count;
                return first;
            }

            void draw(const Draw& draw) override
            {
                NX_ASSERT(m_inFrame);
                BufferSlot* buffer = m_buffers.get(draw.vertices);
                PipelineSlot* pipeline = m_pipelines.get(draw.pipeline);
                const Texture textureHandle = draw.texture ? draw.texture : m_white;
                TextureSlot* texture = m_textures.get(textureHandle);
                const u32 available = draw.vertices ? (buffer ? buffer->count : 0) : m_streamed;
                if ((draw.vertices && buffer == nullptr) || pipeline == nullptr || texture == nullptr || draw.first > available || draw.count > available - draw.first)
                {
                    m_current.rejected++;
                    return;
                }

                if (m_mode != Mode::Flat)
                {
                    m_mode = Mode::Flat;
                    m_bound = {};
                    m_boundTexture = {};
                    flatState();
                    wholeViewport();
                }
                if (m_bound != draw.pipeline)
                {
                    bind(pipeline->desc);
                    m_bound = draw.pipeline;
                }
                if (m_boundTexture != textureHandle)
                {
                    m_device->SetTexture(0, texture->texture);
                    m_boundTexture = textureHandle;
                }
                const DWORD filter = draw.filter == Filter::Nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR;
                m_device->SetTextureStageState(0, D3DTSS_MINFILTER, filter);
                m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
                if (!setScissor(draw.scissor))
                {
                    return;  // clipped away entirely
                }
                m_device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(draw.transform.m));
                m_device->SetStreamSource(0, buffer ? buffer->buffer : m_streams[m_streamIndex], sizeof(Vertex));
                const bool lines = pipeline->desc.topology == Topology::Lines;
                m_device->DrawPrimitive(lines ? D3DPT_LINELIST : D3DPT_TRIANGLELIST, draw.first, draw.count / (lines ? 2 : 3));

                m_current.draws++;
                m_current.vertices += draw.count;
            }

            // --- Meshes: fixed-function lighting ---

            Mesh createMesh(const MeshVertex* vertices, u32 vertexCount, const u16* indices, u32 indexCount, bool dynamic) override
            {
                if (vertices == nullptr || indices == nullptr || vertexCount == 0 || vertexCount > 65536 || indexCount < 3)
                {
                    return {};
                }
                MeshSlot m = {};
                m.vertexCount = vertexCount;
                m.indexCount = indexCount;
                m.dynamic = dynamic;
                bool ok = true;
                for (u32 i = 0; i < (dynamic ? kStreamFrames : 1u) && ok; i++)
                {
                    BYTE* memory = nullptr;
                    ok = SUCCEEDED(m_device->CreateVertexBuffer(vertexCount * sizeof(MeshVertex), D3DUSAGE_WRITEONLY, kMeshFormat, D3DPOOL_DEFAULT, &m.vertices[i])) &&
                         SUCCEEDED(m.vertices[i]->Lock(0, 0, &memory, 0));
                    if (ok)
                    {
                        copyVertices(reinterpret_cast<MeshVertex*>(memory), vertices, vertexCount);
                        m.vertices[i]->Unlock();
                    }
                }
                BYTE* memory = nullptr;
                ok = ok && SUCCEEDED(m_device->CreateIndexBuffer(indexCount * sizeof(u16), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m.indices)) &&
                     SUCCEEDED(m.indices->Lock(0, 0, &memory, 0));
                Mesh handle;
                if (ok)
                {
                    memcpy(memory, indices, indexCount * sizeof(u16));
                    m.indices->Unlock();
                    handle = m_meshes.add(m);
                }
                if (!handle)
                {
                    NX_ERROR("d3d8: no memory for a mesh of %u vertices", static_cast<unsigned>(vertexCount));
                    releaseMesh(m);
                    return {};
                }
                m_bufferBytes += meshBytes(m);
                return handle;
            }

            void updateMesh(Mesh handle, const MeshVertex* vertices, u32 count) override
            {
                MeshSlot* m = m_meshes.get(handle);
                BYTE* memory = nullptr;
                if (m && m->dynamic && count <= m->vertexCount)
                {
                    m->current = (m->current + 1) % kStreamFrames;
                    if (SUCCEEDED(m->vertices[m->current]->Lock(0, 0, &memory, 0)))
                    {
                        copyVertices(reinterpret_cast<MeshVertex*>(memory), vertices, count);
                        m->vertices[m->current]->Unlock();
                    }
                }
            }

            Effect createEffect(EffectKind kind, const char* source, char* error, size_t errorSize) override
            {
                if (errorSize)
                {
                    error[0] = 0;
                }
                const char* at = strstr(source, "// xbox: ");
                for (u32 i = 0; at && i < static_cast<u32>(StandIn::Count); i++)
                {
                    const size_t length = strlen(kStandIns[i]);
                    const bool picture = i >= static_cast<u32>(StandIn::Tube);
                    if (strncmp(at + 9, kStandIns[i], length) == 0 && static_cast<u8>(at[9 + length]) <= ' ' && picture == (kind == EffectKind::Picture))
                    {
                        return m_effects.add({ static_cast<StandIn>(i) });
                    }
                }
                return {};
            }

            void destroyEffect(Effect effect) override { m_effects.remove(effect); }

            bool applyEffect(Effect effect, const float (&values)[16], float time, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                const EffectSlot* e = m_effects.get(effect);
                if (!m_inFrame || e == nullptr || !makeCopy())
                {
                    return false;
                }
                IDirect3DSurface8* target = nullptr;
                IDirect3DSurface8* depth = nullptr;
                IDirect3DSurface8* copy = nullptr;
                m_device->GetRenderTarget(&target);
                m_device->GetDepthStencilSurface(&depth);
                m_copy->GetSurfaceLevel(0, &copy);
                const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
                readScreen(target);
                m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
                m_device->SetTexture(0, &m_screen);
                m_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                const auto plain = [](float&, float&, float, float) -> DWORD { return 0xFFFFFFFF; };
                const bool pixels = e->kind == StandIn::Pixels && makeBloom();
                if (pixels)
                {
                    into(0);
                    const PictureVertex v[6] = { { -0.5f, -0.5f, 0, 1, 0xFFFFFFFF, 0, 0 }, { kBloomWidth - 0.5f, -0.5f, 0, 1, 0xFFFFFFFF, w, 0 }, { kBloomWidth - 0.5f, kBloomHeight - 0.5f, 0, 1, 0xFFFFFFFF, w, h },
                        { -0.5f, -0.5f, 0, 1, 0xFFFFFFFF, 0, 0 }, { kBloomWidth - 0.5f, kBloomHeight - 0.5f, 0, 1, 0xFFFFFFFF, w, h }, { -0.5f, kBloomHeight - 0.5f, 0, 1, 0xFFFFFFFF, 0, h } };
                    picture(v, 6);
                }
                else
                {
                    m_device->SetRenderTarget(copy, nullptr);
                    grid(1, 1, plain);
                }
                m_device->SetRenderTarget(target, depth);
                wholeViewport();
                if (area.enabled)
                {
                    setScissor(area);
                }
                m_device->SetTexture(0, pixels ? m_bloom[0] : m_copy);
                const float much = values[0];
                switch (e->kind)
                {
                case StandIn::Pixels:
                    m_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
                    m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
                    grid(1, 1, [&](float& u, float& v, float, float) -> DWORD
                    {
                        u *= kBloomWidth / w;
                        v *= kBloomHeight / h;
                        return 0xFFFFFFFF;
                    });
                    break;
                case StandIn::Tube:
                {
                    // Beyond the glass is black.
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_BORDER);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_BORDER);
                    m_device->SetTextureStageState(0, D3DTSS_BORDERCOLOR, 0xFF000000);
                    grid(24, 18, [&](float& u, float& v, float x, float y) -> DWORD
                    {
                        const float px = x * 2 - 1, py = y * 2 - 1, r = px * px + py * py;
                        u = px * (1 + 0.09f * much * r) * 0.5f + 0.5f;
                        v = py * (1 + 0.09f * much * r) * 0.5f + 0.5f;
                        const float edge = 1.12f - r * 0.34f;
                        return packed({ edge, edge, edge, 1 });
                    });
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                    // Every other line, darker.
                    static PictureVertex lines[2 * 288];
                    u32 n = 0;
                    for (u32 y = 0; y + 1 < m_height && n + 2 <= 576; y += 2)
                    {
                        lines[n++] = { 0, static_cast<float>(y), 0, 1, 0x58000000, 0, 0 };
                        lines[n++] = { w, static_cast<float>(y), 0, 1, 0x58000000, 0, 0 };
                    }
                    m_device->SetTexture(0, m_textures.get(m_white)->texture);
                    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                    m_device->DrawPrimitiveUP(D3DPT_LINELIST, n / 2, lines, sizeof(PictureVertex));
                    break;
                }
                case StandIn::Ripple:
                    grid(24, 18, [&](float& u, float& v, float x, float y) -> DWORD
                    {
                        u += sinf(y * 18 + time * 2.2f) * 0.006f * much + sinf((x + y) * 11 - time * 1.6f) * 0.004f * much;
                        v += sinf(x * 15 + time * 1.8f) * 0.007f * much;
                        const float bright = 0.5f + 0.5f * sinf(x * 9 + sinf(y * 8 + time) * 2 + time * 1.3f);
                        return packed({ 0.45f + bright * 0.12f, 0.72f + bright * 0.2f, 0.85f + bright * 0.15f, 1 });
                    });
                    break;
                case StandIn::Glitch:
                {
                    const u32 beat = static_cast<u32>(time * 9);
                    grid(1, 18, [&](float& u, float&, float, float y) -> DWORD
                    {
                        const u32 strip = static_cast<u32>(y * 17.99f) * 7919u + beat * 104729u;
                        const u32 r = (strip * 1103515245u + 12345u) >> 16;
                        u += r % 5 == 0 ? (static_cast<float>(r % 101) / 100 - 0.5f) * 0.09f * much : 0;
                        return 0xFFFFFFFF;
                    });
                    // Some colour that has slipped.
                    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                    const float slip = 0.006f * much * (beat % 3 == 0 ? 2.5f : 1);
                    grid(1, 1, [&](float& u, float&, float, float) -> DWORD { u += slip; return 0xFF500000; });
                    grid(1, 1, [&](float& u, float&, float, float) -> DWORD { u -= slip; return 0xFF000050; });
                    break;
                }
                case StandIn::Ink:
                    if (!makeInk())
                    {
                        grid(1, 1, plain);
                        break;
                    }
                    // Four flat tones of each colour, a step at a time.
                    m_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0);
                    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                    m_device->SetPixelShader(m_toneShader);
                    for (u32 i = 0; i < 4; i++)
                    {
                        const float at = 0.1f + 0.22f * static_cast<float>(i);
                        const float constants[2][4] = { { at, at, at, 0 }, { 0.25f, 0.25f, 0.25f, 0 } };
                        m_device->SetPixelShaderConstant(0, constants, 2);
                        grid(1, 1, plain);
                    }
                    // And dark where a picture changes from one side to the other, or from above to below.
                    m_device->SetPixelShader(m_edgeShader);
                    m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX4);
                    for (DWORD i = 0; i < 4; i++)
                    {
                        m_device->SetTexture(i, m_copy);
                        m_device->SetTextureStageState(i, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                        m_device->SetTextureStageState(i, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                        m_device->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
                        m_device->SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                        m_device->SetTextureStageState(i, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                        m_device->SetTextureStageState(i, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                    }
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
                    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
                    quad(w, h, w, h, 0.9f, 0.9f, false);
                    m_device->SetPixelShader(0);
                    for (DWORD i = 1; i < 4; i++)
                    {
                        m_device->SetTexture(i, nullptr);
                    }
                    break;
                case StandIn::Grade:
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                    grid(16, 12, [&](float&, float&, float x, float y) -> DWORD
                    {
                        const float px = x * 2 - 1, py = y * 2 - 1, edge = 1 - (px * px + py * py) * 0.22f * values[1];
                        const float warm = values[0] * (1 - y);
                        return packed({ 0.54f * edge, 0.5f * edge, (0.47f + warm * 0.07f) * edge, 1 });
                    });
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    break;
                default:
                    grid(1, 1, plain);
                    break;
                }
                m_device->SetTexture(0, nullptr);
                copy->Release();
                target->Release();
                if (depth)
                {
                    depth->Release();
                }
                m_bound = {};
                m_boundTexture = {};
                m_mode = Mode::None;
                return true;
            }

            void destroyMesh(Mesh handle) override
            {
                if (MeshSlot* m = m_meshes.get(handle))
                {
                    // The GPU may still be using what it was told to draw.
                    m_device->SetStreamSource(0, nullptr, 0);
                    m_device->SetIndices(nullptr, 0);
                    m_bufferBytes -= meshBytes(*m);
                    releaseMesh(*m);
                    m_meshes.remove(handle);
                }
            }

            void beginView(const View& view) override
            {
                NX_ASSERT(m_inFrame);
                m_view = view;
                m_mode = Mode::None;
                useMeshes();
                m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
            }

            void drawMesh(const MeshDraw& draw) override
            {
                if (draw.lines && !m_inShadows)
                {
                    m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
                    m_device->SetRenderState(D3DRS_BACKFILLMODE, D3DFILL_WIREFRAME);
                    drawTriangles(draw);
                    m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                    m_device->SetRenderState(D3DRS_BACKFILLMODE, D3DFILL_SOLID);
                    return;
                }
                drawTriangles(draw);
            }

            void drawTriangles(const MeshDraw& draw)
            {
                NX_ASSERT(m_inFrame);
                const MeshSlot* m = m_meshes.get(draw.mesh);
                const Texture handle = draw.texture ? draw.texture : m_white;
                const TextureSlot* texture = m_textures.get(handle);
                const u32 count = m && draw.count == 0 && draw.first <= m->indexCount ? m->indexCount - draw.first : draw.count;
                if (m == nullptr || texture == nullptr || draw.first > m->indexCount || count > m->indexCount - draw.first || draw.lightCount > kMeshLights)
                {
                    m_current.rejected++;
                    return;
                }
                if (m_inShadows)
                {
                    m_device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(draw.world.m));
                    m_device->SetStreamSource(0, m->vertices[m->current], sizeof(MeshVertex));
                    m_device->SetIndices(m->indices, 0);
                    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, m->vertexCount, draw.first, count / 3);
                    m_current.draws++;
                    m_current.triangles += count / 3;
                    return;
                }
                useMeshes();
                if (m_boundTexture != handle)
                {
                    m_device->SetTexture(0, texture->texture);
                    m_boundTexture = handle;
                }
                const DWORD address = draw.repeat ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
                m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, address);
                m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, address);
                m_device->SetTextureStageState(0, D3DTSS_MIPFILTER, draw.repeat ? D3DTEXF_LINEAR : D3DTEXF_NONE);
                // The tint multiplies everything, after the lights.
                const bool lit = draw.lit && draw.glow < 1;
                // What is solid has no use for its alpha, so that says how dark a shadow is.
                const bool shaded = m_shadowed && lit && !draw.indoors && draw.blend == Blend::None && !draw.effect && draw.cutout <= 0;
                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed(draw.color));
                m_device->SetRenderState(D3DRS_LIGHTING, lit);
                // The same as the stages do, and then some of the sun's light away.
                m_device->SetPixelShader(shaded ? m_shadowShader : 0);
                if (shaded)
                {
                    const float shade = m_shade * (1 - draw.glow);
                    const float constants[2][4] = { { draw.color.r, draw.color.g, draw.color.b, draw.color.a }, { shade, shade, shade, 1 } };
                    m_device->SetPixelShaderConstant(0, constants, 2);
                }
                m_device->LightEnable(0, !draw.indoors);
                if (lit)
                {
                    D3DMATERIAL8 material = {};
                    material.Diffuse.a = 1;
                    material.Emissive = colorValue({ draw.glow, draw.glow, draw.glow, 1 });
                    m_device->SetMaterial(&material);
                    for (u32 i = 0; i < kMeshLights; i++)
                    {
                        const bool on = i < draw.lightCount && draw.lights[i] < m_view.lightCount;
                        if (on)
                        {
                            const PointLight& from = m_view.lights[draw.lights[i]];
                            D3DLIGHT8 light = {};
                            light.Type = D3DLIGHT_POINT;
                            light.Diffuse = colorValue(from.color);
                            light.Position.x = from.place.x;
                            light.Position.y = from.place.y;
                            light.Position.z = from.place.z;
                            light.Range = from.range;
                            light.Attenuation0 = 1;
                            light.Attenuation2 = 8 / (from.range * from.range);
                            m_device->SetLight(2 + i, &light);
                        }
                        m_device->LightEnable(2 + i, on);
                    }
                }
                m_device->SetRenderState(D3DRS_FOGENABLE, draw.fogged);
                m_device->SetRenderState(D3DRS_CULLMODE, draw.cull == Cull::Back ? D3DCULL_CW : D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, draw.depthWrite);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, draw.blend != Blend::None);
                m_device->SetRenderState(D3DRS_SRCBLEND, draw.blend == Blend::Multiply ? D3DBLEND_DESTCOLOR : D3DBLEND_SRCALPHA);
                m_device->SetRenderState(D3DRS_DESTBLEND, draw.blend == Blend::Additive ? D3DBLEND_ONE : draw.blend == Blend::Multiply ? D3DBLEND_ZERO : D3DBLEND_INVSRCALPHA);
                // Cut out where the picture is clear. The wind's bending needs a vertex shader, so here it stays still.
                m_device->SetRenderState(D3DRS_ALPHATESTENABLE, draw.cutout > 0);
                if (draw.cutout > 0)
                {
                    m_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
                    m_device->SetRenderState(D3DRS_ALPHAREF, static_cast<DWORD>(draw.cutout >= 1 ? 255 : draw.cutout * 255));
                }
                m_device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(draw.world.m));
                m_device->SetStreamSource(0, m->vertices[m->current], sizeof(MeshVertex));
                m_device->SetIndices(m->indices, 0);
                if (const EffectSlot* effect = m_effects.get(draw.effect))
                {
                    drawStandIn(draw, *m, count, effect->kind);
                    return;
                }
                const TextureSlot* volume = draw.volume ? m_textures.get(draw.volume) : nullptr;
                if (volume && volume->volume)
                {
                    // Where each part is in the camera's space, back into the world, scaled and moved: the volume's
                    // coordinates. The view here has its z turned round, so that is undone first.
                    Mat4 flip;
                    flip.m[10] = -1;
                    Mat4 into;
                    into.m[0] = into.m[5] = into.m[10] = draw.volumeScale;
                    into.m[12] = draw.volumeOffset.x;
                    into.m[13] = draw.volumeOffset.y;
                    into.m[14] = draw.volumeOffset.z;
                    const Mat4 uvw = into * m_view.view.inverse() * flip;
                    m_device->SetTexture(0, volume->volume);
                    m_boundTexture = {};
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
                    m_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
                    m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
                    m_device->SetTransform(D3DTS_TEXTURE0, reinterpret_cast<const D3DMATRIX*>(uvw.m));
                }
                const bool inPixels = !(volume && volume->volume) && texture && (texture->depth || texture->linear);
                if (inPixels)
                {
                    Mat4 scale;
                    scale.m[0] = texture->info.uMax;
                    scale.m[5] = texture->info.vMax;
                    m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
                    m_device->SetTransform(D3DTS_TEXTURE0, reinterpret_cast<const D3DMATRIX*>(scale.m));
                }
                m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, m->vertexCount, draw.first, count / 3);
                if (inPixels)
                {
                    m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                }
                if (volume && volume->volume)
                {
                    m_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
                    m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                    m_device->SetTexture(0, nullptr);
                }
                m_current.draws++;
                m_current.triangles += count / 3;
            }

            // What is bright, smaller; spread, with four looks at a time; and added.
            bool bloom(const Bloom& b, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                if (!m_inFrame || !makeBloom())
                {
                    return false;
                }
                IDirect3DSurface8* target = nullptr;
                IDirect3DSurface8* depth = nullptr;
                m_device->GetRenderTarget(&target);
                m_device->GetDepthStencilSurface(&depth);
                if (target == nullptr)
                {
                    return false;
                }
                readScreen(target);
                m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX4);
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTextureStageState(i, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                    m_device->SetTextureStageState(i, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                    m_device->SetTextureStageState(i, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                    m_device->SetTextureStageState(i, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                    m_device->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
                    m_device->SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                }
                const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
                constexpr float bw = kBloomWidth, bh = kBloomHeight;

                averageOf(&m_screen);
                into(0);
                quad(bw, bh, w, h, w / bw * 0.25f, h / bh * 0.25f, false);
                // Only as much of it as is over the threshold, twice over.
                const float t = b.threshold * 0.9f;
                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed({ t, t, t, 1 }));
                m_device->SetTexture(0, m_bloom[0]);
                for (DWORD i = 1; i < 4; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SUBTRACT);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
                m_device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
                m_device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                m_device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                into(1);
                quad(bw, bh, bw, bh, 0, 0, false);
                // Along and then down, twice, wider the second time.
                u32 from = 1;
                for (u32 pass = 0; pass < 4; pass++)
                {
                    averageOf(m_bloom[from]);
                    from = 1 - from;
                    into(from);
                    const float step = (pass < 2 ? 1.2f : 3.0f) * b.spread;
                    quad(bw, bh, bw, bh, pass % 2 ? 0 : step, pass % 2 ? step : 0, true);
                }

                m_device->SetRenderTarget(target, depth);
                wholeViewport();
                if (area.enabled)
                {
                    setScissor(area);
                }
                m_device->SetTexture(0, m_bloom[from]);
                for (DWORD i = 1; i < 4; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                const float much = b.strength > 1 ? 1 : b.strength;
                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed({ much, much, much, 1 }));
                m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                quad(w, h, bw, bh, 0, 0, false);
                m_device->SetTexture(0, nullptr);
                target->Release();
                if (depth)
                {
                    depth->Release();
                }
                m_bound = {};
                m_boundTexture = {};
                m_mode = Mode::None;
                return true;
            }

            // A copy of the picture, and then from it each colour's parts made into a place in the table, in the GPU's
            // texture stages, and looked up there.
            bool grade(Texture table, float strength, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                const TextureSlot* t = m_textures.get(table);
                if (!m_inFrame || t == nullptr || t->volume == nullptr || !makeCopy() || !makeGradeShader())
                {
                    return false;
                }
                IDirect3DSurface8* target = nullptr;
                IDirect3DSurface8* depth = nullptr;
                IDirect3DSurface8* copy = nullptr;
                m_device->GetRenderTarget(&target);
                m_device->GetDepthStencilSurface(&depth);
                if (target == nullptr)
                {
                    if (depth)
                    {
                        depth->Release();
                    }
                    return false;
                }
                m_copy->GetSurfaceLevel(0, &copy);
                readScreen(target);
                m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
                m_device->SetTexture(0, &m_screen);
                m_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
                m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
                m_device->SetRenderTarget(copy, nullptr);
                grid(1, 1, [](float&, float&, float, float) -> DWORD { return 0xFFFFFFFF; });
                m_device->SetRenderTarget(target, depth);
                wholeViewport();
                if (area.enabled)
                {
                    setScissor(area);
                }

                m_device->SetTexture(0, m_copy);
                m_device->SetTexture(1, nullptr);
                m_device->SetTexture(2, nullptr);
                m_device->SetTexture(3, t->volume);
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
                    m_device->SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                }
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(3, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(3, D3DTSS_MIPFILTER, D3DTEXF_NONE);
                const float much[4] = { strength, strength, strength, strength };
                m_device->SetPixelShader(m_gradeShader);
                m_device->SetPixelShaderConstant(0, much, 1);
                m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE3(1) | D3DFVF_TEXCOORDSIZE3(2) | D3DFVF_TEXCOORDSIZE3(3));
                struct Corner
                {
                    float x, y, z, rhw;
                    float u, v;
                    float red[3], green[3], blue[3];
                };
                const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
                Corner c[4];
                for (u32 i = 0; i < 4; i++)
                {
                    const bool right = (i & 1) != 0, low = (i & 2) != 0;
                    c[i] = { (right ? w : 0) - 0.5f, (low ? h : 0) - 0.5f, 0, 1, right ? w : 0, low ? h : 0, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
                }
                m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, c, sizeof(Corner));
                m_current.draws++;

                m_device->SetPixelShader(0);
                m_device->SetTexture(0, nullptr);
                m_device->SetTexture(3, nullptr);
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
                m_device->SetTextureStageState(3, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
                copy->Release();
                target->Release();
                if (depth)
                {
                    depth->Release();
                }
                m_bound = {};
                m_boundTexture = {};
                m_mode = Mode::None;
                return true;
            }

            bool drawSky() override { return false; }
            bool drawField(const Field&) override { return false; }
            // From the sun, into a depth texture, which the GPU compares as it reads it.
            bool beginShadows(const Mat4& sun) override
            {
                NX_ASSERT(m_inFrame);
                if (!m_shadowsTried)
                {
                    // Only once something has shadows: they take a megabyte.
                    m_shadowsTried = true;
                    if (FAILED(m_device->CreateTexture(kShadowSide, kShadowSide, 1, D3DUSAGE_RENDERTARGET, D3DFMT_LIN_R5G6B5, D3DPOOL_DEFAULT, &m_shadowColor)) ||
                        FAILED(m_device->CreateTexture(kShadowSide, kShadowSide, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_LIN_D16, D3DPOOL_DEFAULT, &m_shadowDepth)))
                    {
                        NX_WARN("d3d8: no memory for shadows");
                    }
                }
                if (m_shadowDepth == nullptr || m_shadowColor == nullptr)
                {
                    return false;
                }
                IDirect3DSurface8* color = nullptr;
                IDirect3DSurface8* depth = nullptr;
                m_device->GetRenderTarget(&m_savedColor);
                m_device->GetDepthStencilSurface(&m_savedDepth);
                m_shadowColor->GetSurfaceLevel(0, &color);
                m_shadowDepth->GetSurfaceLevel(0, &depth);
                // It can't be read while it is written.
                m_device->SetTexture(2, nullptr);
                const bool ok = SUCCEEDED(m_device->SetRenderTarget(color, depth));
                color->Release();
                depth->Release();
                if (!ok)
                {
                    endShadows();
                    return false;
                }
                m_mode = Mode::None;
                flatState();
                // A clear edge, which is what there is beyond it.
                const D3DVIEWPORT8 all = { 0, 0, kShadowSide, kShadowSide, 0.0f, 1.0f };
                m_device->SetViewport(&all);
                m_device->SetScissors(0, FALSE, nullptr);
                m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
                const D3DVIEWPORT8 within = { 2, 2, kShadowSide - 4, kShadowSide - 4, 0.0f, 1.0f };
                m_device->SetViewport(&within);
                m_device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(sun.m));
                m_device->SetVertexShader(kMeshFormat);
                m_device->SetTexture(0, nullptr);
                m_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                // Back a little, and more where it is steep to the sun, so that nothing shades itself.
                const float back = 8, steep = 3;
                DWORD bits;
                memcpy(&bits, &back, sizeof(bits));
                m_device->SetRenderState(D3DRS_POLYGONOFFSETZOFFSET, bits);
                memcpy(&bits, &steep, sizeof(bits));
                m_device->SetRenderState(D3DRS_POLYGONOFFSETZSLOPESCALE, bits);
                m_device->SetRenderState(D3DRS_SOLIDOFFSETENABLE, TRUE);
                m_inShadows = true;
                return true;
            }

            u32 shadowSize() const override { return kShadowSide - 4; }

            void endShadows() override
            {
                m_device->SetRenderTarget(m_savedColor, m_savedDepth);
                for (IDirect3DSurface8** s : { &m_savedColor, &m_savedDepth })
                {
                    if (*s)
                    {
                        (*s)->Release();
                        *s = nullptr;
                    }
                }
                m_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
                m_device->SetRenderState(D3DRS_SOLIDOFFSETENABLE, FALSE);
                m_inShadows = false;
                m_mode = Mode::None;
                m_bound = {};
                m_boundTexture = {};
            }

            void endFrame() override
            {
                endTarget();
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                m_device->EndScene();
                m_device->Present(nullptr, nullptr, nullptr, nullptr);
                m_frameNumber++;
                m_last = m_current;
            }

            bool capture(u8* pixels, size_t bytes) override
            {
                if (pixels == nullptr || bytes < static_cast<size_t>(m_width) * m_height * 4 || m_inFrame)
                {
                    return false;
                }
                IDirect3DSurface8* front = nullptr;
                // Index -1 is the front buffer on the Xbox.
                if (FAILED(m_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &front)) || front == nullptr)
                {
                    return false;
                }
                D3DLOCKED_RECT rect;
                const bool ok = SUCCEEDED(front->LockRect(&rect, nullptr, D3DLOCK_READONLY));
                if (ok)
                {
                    for (u32 y = 0; y < m_height; y++)
                    {
                        const u8* in = static_cast<const u8*>(rect.pBits) + static_cast<size_t>(y) * rect.Pitch;
                        u8* out = pixels + static_cast<size_t>(y) * m_width * 4;
                        for (u32 x = 0; x < m_width; x++, in += 4, out += 4)
                        {
                            out[0] = in[2];
                            out[1] = in[1];
                            out[2] = in[0];
                            out[3] = 255;
                        }
                    }
                    front->UnlockRect();
                }
                front->Release();
                return ok;
            }

            Stats stats() const override
            {
                Stats s = m_last;
                s.bufferBytes = m_bufferBytes;
                s.textureBytes = m_textureBytes;
                return s;
            }

        private:
            // False when nothing would be visible: the hardware has no empty scissor rectangle.
            bool setScissor(const Scissor& s)
            {
                if (!s.enabled)
                {
                    m_device->SetScissors(0, FALSE, nullptr);
                    return true;
                }
                D3DRECT rect = { s.x, s.y, s.x + s.width, s.y + s.height };
                rect.x1 = rect.x1 < 0 ? 0 : rect.x1;
                rect.y1 = rect.y1 < 0 ? 0 : rect.y1;
                rect.x2 = rect.x2 > static_cast<LONG>(m_width) ? static_cast<LONG>(m_width) : rect.x2;
                rect.y2 = rect.y2 > static_cast<LONG>(m_height) ? static_cast<LONG>(m_height) : rect.y2;
                if (rect.x2 <= rect.x1 || rect.y2 <= rect.y1)
                {
                    return false;
                }
                m_device->SetScissors(1, FALSE, &rect);
                return true;
            }

            enum class Mode : u8 { None, Flat, Mesh };

            void wholeViewport()
            {
                const D3DVIEWPORT8 viewport = { 0, 0, m_width, m_height, 0.0f, 1.0f };
                m_device->SetViewport(&viewport);
            }

            void useMeshes()
            {
                if (m_mode == Mode::Mesh)
                {
                    return;
                }
                m_mode = Mode::Mesh;
                m_bound = {};
                m_boundTexture = {};
                flatState();
                wholeViewport();
                m_device->SetScissors(0, FALSE, nullptr);
                if (m_view.area.enabled)
                {
                    const D3DVIEWPORT8 viewport = { static_cast<DWORD>(m_view.area.x), static_cast<DWORD>(m_view.area.y), static_cast<DWORD>(m_view.area.width), static_cast<DWORD>(m_view.area.height), 0.0f, 1.0f };
                    m_device->SetViewport(&viewport);
                }
                // With z turned round in between, which comes to the same: the fog goes by distance only for
                // a projection that looks along +z.
                Mat4 view = m_view.view, projection = m_view.projection;
                for (u32 i = 0; i < 4; i++)
                {
                    view.m[i * 4 + 2] = -view.m[i * 4 + 2];
                    projection.m[8 + i] = -projection.m[8 + i];
                }
                m_device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(view.m));
                m_device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(projection.m));
                m_device->SetVertexShader(kMeshFormat);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                m_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
                m_device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
                m_device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
                m_device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
                m_device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
                m_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
                m_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                m_device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
                m_device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                m_device->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
                m_device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
                m_device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                m_shadowed = m_view.shadowStrength > 0 && m_shadowDepth && m_shadowShader;
                if (m_shadowed)
                {
                    useShadows(view);
                }

                // Light from above and below: what is in common is ambient, and the rest comes straight down.
                const Color& sky = m_view.ambientSky;
                const Color& ground = m_view.ambientGround;
                m_device->SetRenderState(D3DRS_AMBIENT, packed({ (sky.r + ground.r) * 0.5f, (sky.g + ground.g) * 0.5f, (sky.b + ground.b) * 0.5f, 1 }));
                D3DLIGHT8 light = {};
                light.Type = D3DLIGHT_DIRECTIONAL;
                light.Diffuse = colorValue(m_view.sunColor);
                light.Direction.x = -m_view.sunDirection.x;
                light.Direction.y = -m_view.sunDirection.y;
                light.Direction.z = -m_view.sunDirection.z;
                m_device->SetLight(0, &light);
                m_device->LightEnable(0, TRUE);
                D3DLIGHT8 above = {};
                above.Type = D3DLIGHT_DIRECTIONAL;
                above.Diffuse = colorValue({ (sky.r - ground.r) * 0.5f, (sky.g - ground.g) * 0.5f, (sky.b - ground.b) * 0.5f, 1 });
                above.Direction.y = -1;
                m_device->SetLight(1, &above);
                m_device->LightEnable(1, TRUE);

                m_device->SetRenderState(D3DRS_FOGCOLOR, packed(m_view.fogColor));
                m_device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
                const float start = m_view.fogStart, end = m_view.fogEnd;
                DWORD bits;
                memcpy(&bits, &start, sizeof(bits));
                m_device->SetRenderState(D3DRS_FOGSTART, bits);
                memcpy(&bits, &end, sizeof(bits));
                m_device->SetRenderState(D3DRS_FOGEND, bits);
            }

            // The third texture is asked of each pixel whether the sun sees it. Only with a pixel shader does the GPU
            // compare depths: with the fixed stages it reads them as a picture.
            void useShadows(const Mat4& view)
            {
                // From the camera to the map, in its pixels, with depth as it is kept there.
                Mat4 map;
                const float half = (kShadowSide - 4) * 0.5f, middle = kShadowSide * 0.5f;
                map.m[0] = half;
                map.m[5] = -half;
                map.m[10] = 65535.0f;
                map.m[12] = middle + 0.5f;
                map.m[13] = middle + 0.5f;
                const Mat4 whole = map * m_view.shadows * view.inverse();
                m_device->SetTransform(D3DTS_TEXTURE2, reinterpret_cast<const D3DMATRIX*>(whole.m));
                m_device->SetTexture(2, m_shadowDepth);
                m_device->SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION | 2);
                m_device->SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT4 | D3DTTFF_PROJECTED);
                m_device->SetTextureStageState(2, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(2, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                m_device->SetTextureStageState(2, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(2, D3DTSS_MIPFILTER, D3DTEXF_NONE);
                m_device->SetRenderState(D3DRS_SHADOWFUNC, D3DCMP_GREATEREQUAL);
                // How much each corner faces the sun, as a colour.
                const Vec3& d = m_view.sunDirection;
                const float sunX = view.m[0] * d.x + view.m[4] * d.y + view.m[8] * d.z, sunY = view.m[1] * d.x + view.m[5] * d.y + view.m[9] * d.z, sunZ = view.m[2] * d.x + view.m[6] * d.y + view.m[10] * d.z;
                Mat4 facing;
                for (u32 i = 0; i < 3; i++)
                {
                    facing.m[i] = sunX;
                    facing.m[4 + i] = sunY;
                    facing.m[8 + i] = sunZ;
                }
                m_device->SetTransform(D3DTS_TEXTURE1, reinterpret_cast<const D3DMATRIX*>(facing.m));
                // Its picture isn't looked at, but a stage without one makes no coordinates.
                m_device->SetTexture(1, m_textures.get(m_white)->texture);
                m_device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL | 1);
                m_device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
                // Only the sun's part of the light goes.
                const auto bright = [](const Color& c) { return c.r * 0.3f + c.g * 0.6f + c.b * 0.1f; };
                const float sun = bright(m_view.sunColor), rest = (bright(m_view.ambientSky) + bright(m_view.ambientGround)) * 0.5f;
                m_shade = m_view.shadowStrength * sun / (sun + rest + 0.001f);
            }

            void readScreen(IDirect3DSurface8* target)
            {
                // The same memory, to be read. It is in pixels, as anything that isn't a power of two.
                m_screen.Common = 1 | D3DCOMMON_TYPE_TEXTURE;
                m_screen.Data = target->Data;
                m_screen.Lock = 0;
                m_screen.Format = target->Format;
                m_screen.Size = target->Size;
                m_mode = Mode::None;
                flatState();
                m_device->SetScissors(0, FALSE, nullptr);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            }

            // Small pictures that the stand-ins are made of.
            void makePatterns()
            {
                if (m_lava)
                {
                    return;
                }
                constexpr u32 n = 64;
                static u8 pixels[n * n * 4];
                const auto wave = [](float x, float y, float a, float b, float c) { return sinf((x * a + y * b) * 6.28319f / n + c); };
                const auto make = [&](auto&& fill)
                {
                    for (u32 y = 0; y < n; y++)
                    {
                        for (u32 x = 0; x < n; x++)
                        {
                            fill(static_cast<float>(x), static_cast<float>(y), &pixels[(y * n + x) * 4]);
                        }
                    }
                    TextureData data = rgbaTexture(pixels, n, n);
                    data.mips = true;
                    return createTexture(data);
                };
                // Whole numbers of waves, so that it meets itself at the edges.
                m_lava = make([&](float x, float y, u8* out)
                {
                    const float bend = wave(x, y, 1, 2, 0) * 5, bend2 = wave(x, y, 2, -1, 1) * 5;
                    const float v = fabsf(wave(x + bend, y + bend2, 2, 1, 0.5f) + wave(x + bend2, y + bend, -1, 3, 2) * 0.8f) / 1.8f;
                    const float hot = v < 0.22f ? 1 - v / 0.22f : 0;
                    out[0] = static_cast<u8>(22 + hot * 233);
                    out[1] = static_cast<u8>(14 + hot * hot * 170);
                    out[2] = static_cast<u8>(12 + hot * hot * hot * 60);
                    out[3] = 255;
                });
                m_noise = make([&](float x, float y, u8* out)
                {
                    const float v = wave(x, y, 2, 3, 0) + wave(x, y, -3, 2, 1) + wave(x, y, 5, 1, 2) * 0.7f + wave(x, y, -4, 7, 3) * 0.5f + wave(x, y, 9, -6, 1) * 0.3f;
                    out[0] = out[1] = out[2] = 255;
                    const float alpha = v / 7 + 0.5f;
                    out[3] = static_cast<u8>(alpha < 0 ? 0 : alpha > 1 ? 255 : alpha * 255);
                });
                m_lines = make([&](float, float y, u8* out)
                {
                    const u32 at = static_cast<u32>(y) % 8;
                    out[0] = out[1] = out[2] = static_cast<u8>(at < 4 ? 255 : 90);
                    out[3] = 255;
                });
                m_stripes = make([&](float, float y, u8* out)
                {
                    out[0] = 242;
                    out[1] = 237;
                    out[2] = 224;
                    out[3] = y < n / 2 ? 0 : 255;
                });
            }

            // Where a texture goes on a thing, from where each part of it is: across and down.
            void fromPlace(DWORD stage, const float (&across)[4], const float (&down)[4])
            {
                Mat4 m;
                for (u32 i = 0; i < 4; i++)
                {
                    m.m[i * 4] = across[i];
                    m.m[i * 4 + 1] = down[i];
                }
                m_device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), reinterpret_cast<const D3DMATRIX*>(m.m));
                m_device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_OBJECT | stage);
                m_device->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
                m_device->SetTextureStageState(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
                m_device->SetTextureStageState(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
                m_device->SetTextureStageState(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                m_device->SetTextureStageState(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
            }

            // Everything else has been set as for any mesh.
            void drawStandIn(const MeshDraw& draw, const MeshSlot& m, u32 count, StandIn kind)
            {
                makePatterns();
                const float none[16] = {};
                const float* v = draw.values ? draw.values : none;
                const float time = m_view.time;
                m_device->SetPixelShader(0);
                const auto go = [&]
                {
                    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, m.vertexCount, draw.first, count / 3);
                    m_current.draws++;
                    m_current.triangles += count / 3;
                };
                const auto use = [&](DWORD stage, Texture t) { m_device->SetTexture(stage, m_textures.get(t)->texture); };
                switch (kind)
                {
                case StandIn::Lava:
                    use(0, m_lava);
                    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
                    m_device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    fromPlace(0, { 0.9f, 0.4f, 0.25f, time * 0.02f }, { 0.1f, 0.8f, 0.85f, time * 0.05f });
                    go();
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    break;
                case StandIn::Hologram:
                    use(0, m_lines);
                    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
                    m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed({ v[0], v[1], v[2], 1 }));
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                    fromPlace(0, { 0.1f, 0, 0, 0 }, { 0, 2.2f, 0, -time * 0.25f });
                    go();
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    break;
                case StandIn::Dissolve:
                {
                    use(2, m_noise);
                    m_device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    m_device->SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_CURRENT);
                    m_device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_device->SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    fromPlace(2, { 1.1f, 0.5f, 0.3f, 0 }, { 0.2f, 0.9f, 0.9f, 0 });
                    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
                    const float cut = v[0] * 0.8f + 0.06f;
                    const auto level = [](float x) { return static_cast<DWORD>(x <= 0 ? 0 : x >= 1 ? 255 : x * 255); };
                    // What is left, and round it, what burns.
                    m_device->SetRenderState(D3DRS_ALPHAREF, level(v[0] > 0.001f ? cut + 0.06f : 0));
                    go();
                    if (v[0] > 0.001f)
                    {
                        m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
                        m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed({ v[4], v[5], v[6], 1 }));
                        m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
                        m_device->SetRenderState(D3DRS_ALPHAREF, level(cut));
                        m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
                        go();
                        m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                        m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    }
                    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                    m_device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                    m_device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    break;
                }
                case StandIn::Flag:
                {
                    // Its corners are moved here.
                    constexpr u32 kMost = 512;
                    static MeshVertex moved[kMost];
                    BYTE* memory = nullptr;
                    BYTE* indices = nullptr;
                    if (m.vertexCount > kMost || FAILED(m.vertices[m.current]->Lock(0, 0, &memory, D3DLOCK_READONLY)))
                    {
                        go();
                        break;
                    }
                    memcpy(moved, memory, m.vertexCount * sizeof(MeshVertex));
                    m.vertices[m.current]->Unlock();
                    for (u32 i = 0; i < m.vertexCount; i++)
                    {
                        MeshVertex& c = moved[i];
                        const float out = c.u * v[0];
                        c.y += sinf(c.u * 9 - time * 7) * 0.03f * out;
                        c.z += (sinf(c.u * 11 - time * 8 + c.v * 2) * 0.16f + sinf(c.u * 5 - time * 5) * 0.12f) * out;
                        const float lit = 0.8f + 0.2f * cosf(c.u * 11 - time * 8 + c.v * 2);
                        c.r = c.g = c.b = static_cast<u8>(lit * 255);
                    }
                    use(0, m_stripes);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
                    m_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
                    Mat4 across;
                    across.m[1] = 0.6f;
                    across.m[5] = 3;
                    m_device->SetTransform(D3DTS_TEXTURE0, reinterpret_cast<const D3DMATRIX*>(across.m));
                    m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
                    m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed(draw.color));
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_LERP);
                    m_device->SetTextureStageState(0, D3DTSS_COLORARG0, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
                    m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    m_device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                    m.indices->Lock(0, 0, &indices, D3DLOCK_READONLY);
                    m_device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, m.vertexCount, count / 3, reinterpret_cast<const u16*>(indices) + draw.first, D3DFMT_INDEX16, moved, sizeof(MeshVertex));
                    m.indices->Unlock();
                    m_current.draws++;
                    m_current.triangles += count / 3;
                    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    m_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                    m_device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    break;
                }
                default:
                    go();
                    break;
                }
                // The stages as they were, shadows and all.
                m_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
                m_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                m_boundTexture = {};
                m_mode = Mode::None;
            }

            bool makeCopy()
            {
                if (!m_copyTried)
                {
                    m_copyTried = true;
                    if (FAILED(m_device->CreateTexture(m_width, m_height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_LIN_X8R8G8B8, D3DPOOL_DEFAULT, &m_copy)))
                    {
                        NX_WARN("d3d8: no memory for effects");
                        m_copy = nullptr;
                    }
                }
                return m_copy != nullptr;
            }

            void picture(const PictureVertex* v, u32 count)
            {
                m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, v, sizeof(PictureVertex));
                m_current.draws++;
            }

            // The whole picture as a grid, whose corners each look somewhere.
            template <typename Look>
            void grid(u32 columns, u32 rows, Look&& look)
            {
                static PictureVertex v[24 * 18 * 6];
                u32 n = 0;
                const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
                for (u32 y = 0; y < rows; y++)
                {
                    for (u32 x = 0; x < columns; x++)
                    {
                        static const u32 corners[6][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } };
                        for (const auto& c : corners)
                        {
                            const float px = static_cast<float>(x + c[0]) / static_cast<float>(columns), py = static_cast<float>(y + c[1]) / static_cast<float>(rows);
                            PictureVertex& out = v[n++];
                            out.x = px * w - 0.5f;
                            out.y = py * h - 0.5f;
                            out.z = 0;
                            out.rhw = 1;
                            float u = px, t = py;
                            out.color = look(u, t, px, py);
                            out.u = u * w;
                            out.v = t * h;
                        }
                    }
                }
                picture(v, n);
            }

            bool makeBloom()
            {
                if (!m_bloomTried)
                {
                    m_bloomTried = true;
                    for (IDirect3DTexture8*& t : m_bloom)
                    {
                        if (FAILED(m_device->CreateTexture(kBloomWidth, kBloomHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &t)))
                        {
                            NX_WARN("d3d8: no memory for bloom");
                            t = nullptr;
                        }
                    }
                }
                return m_bloom[0] && m_bloom[1];
            }

            void into(u32 which)
            {
                IDirect3DSurface8* surface = nullptr;
                m_bloom[which]->GetSurfaceLevel(0, &surface);
                m_device->SetRenderTarget(surface, nullptr);
                surface->Release();
            }

            // Four looks at the same picture, added up in quarters.
            void averageOf(IDirect3DBaseTexture8* picture)
            {
                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, 0x40404040);
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTexture(i, picture);
                    m_device->SetTextureStageState(i, D3DTSS_COLOROP, i ? D3DTOP_MULTIPLYADD : D3DTOP_MODULATE);
                    m_device->SetTextureStageState(i, D3DTSS_COLORARG0, D3DTA_CURRENT);
                    m_device->SetTextureStageState(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_device->SetTextureStageState(i, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    m_device->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_device->SetTextureStageState(i, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
                }
            }

            // Over the whole of something, with looks to each side: along a line, or at four corners.
            void quad(float w, float h, float u1, float v1, float du, float dv, bool line)
            {
                struct Corner
                {
                    float x, y, z, rhw;
                    float uv[4][2];
                };
                static const float along[4][2] = { { -1.5f, -1.5f }, { -0.5f, -0.5f }, { 0.5f, 0.5f }, { 1.5f, 1.5f } };
                static const float corners[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
                Corner c[4];
                for (u32 i = 0; i < 4; i++)
                {
                    const bool right = (i & 1) != 0, low = (i & 2) != 0;
                    c[i].x = (right ? w : 0) - 0.5f;
                    c[i].y = (low ? h : 0) - 0.5f;
                    c[i].z = 0;
                    c[i].rhw = 1;
                    for (u32 k = 0; k < 4; k++)
                    {
                        const float* at = line ? along[k] : corners[k];
                        c[i].uv[k][0] = (right ? u1 : 0) + at[0] * du;
                        c[i].uv[k][1] = (low ? v1 : 0) + at[1] * dv;
                    }
                }
                m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, c, sizeof(Corner));
                m_current.draws++;
            }

            bool makeInk()
            {
                if (m_toneShader && m_edgeShader)
                {
                    return true;
                }
                constexpr DWORD kOne = PS_REGISTER_ZERO | PS_INPUTMAPPING_UNSIGNED_INVERT;
                constexpr DWORD kMinus = PS_REGISTER_ZERO | PS_INPUTMAPPING_EXPAND_NORMAL;
                D3DPIXELSHADERDEF tone = {};
                tone.PSTextureModes = PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_NONE, PS_TEXTUREMODES_NONE, PS_TEXTUREMODES_NONE);
                tone.PSCombinerCount = PS_COMBINERCOUNT(4, PS_COMBINERCOUNT_UNIQUE_C0);
                // How far over a level, sixteen times, so that it is all or nothing.
                tone.PSRGBInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_RGB, kOne, PS_REGISTER_C0 | PS_CHANNEL_RGB, kMinus);
                tone.PSRGBOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R0, PS_COMBINEROUTPUT_SHIFTLEFT_2);
                tone.PSRGBInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, kOne, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                tone.PSRGBOutputs[1] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_COMBINEROUTPUT_SHIFTLEFT_2);
                tone.PSRGBInputs[2] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                tone.PSRGBOutputs[2] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                tone.PSRGBInputs[3] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, kOne, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                tone.PSRGBOutputs[3] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                tone.PSC0Mapping = PS_CONSTANTMAPPING(0, 15, 1, 15, 15, 15, 15, 15);
                tone.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 15, 15, 15, 15, 15, 15);

                D3DPIXELSHADERDEF edge = {};
                edge.PSTextureModes = PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_PROJECT2D);
                edge.PSCombinerCount = PS_COMBINERCOUNT(5, 0);
                // The corners, across from one another; squared, so that either way counts.
                edge.PSRGBInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_RGB, kOne, PS_REGISTER_T3 | PS_CHANNEL_RGB, kMinus);
                edge.PSRGBOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R0, PS_COMBINEROUTPUT_SHIFTLEFT_1);
                edge.PSRGBInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_T1 | PS_CHANNEL_RGB, kOne, PS_REGISTER_T2 | PS_CHANNEL_RGB, kMinus);
                edge.PSRGBOutputs[1] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R1, PS_COMBINEROUTPUT_SHIFTLEFT_1);
                constexpr DWORD r0 = PS_REGISTER_R0 | PS_CHANNEL_RGB | PS_INPUTMAPPING_SIGNED_IDENTITY, r1 = PS_REGISTER_R1 | PS_CHANNEL_RGB | PS_INPUTMAPPING_SIGNED_IDENTITY;
                edge.PSRGBInputs[2] = PS_COMBINERINPUTS(r0, r0, r1, r1);
                edge.PSRGBOutputs[2] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R0, PS_COMBINEROUTPUT_SHIFTLEFT_2);
                edge.PSRGBInputs[3] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, kOne, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                edge.PSRGBOutputs[3] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_COMBINEROUTPUT_AB_DOT_PRODUCT | PS_COMBINEROUTPUT_SHIFTLEFT_1);
                edge.PSRGBInputs[4] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT, kOne, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                edge.PSRGBOutputs[4] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                edge.PSC0Mapping = edge.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 15, 15, 15, 15, 15, 15);
                if (FAILED(m_device->CreatePixelShader(&tone, &m_toneShader)))
                {
                    m_toneShader = 0;
                }
                if (FAILED(m_device->CreatePixelShader(&edge, &m_edgeShader)))
                {
                    m_edgeShader = 0;
                }
                return m_toneShader && m_edgeShader;
            }

            bool makeGradeShader()
            {
                if (m_gradeShader || m_gradeTried)
                {
                    return m_gradeShader != 0;
                }
                m_gradeTried = true;
                constexpr DWORD kOne = PS_REGISTER_ZERO | PS_INPUTMAPPING_UNSIGNED_INVERT;
                D3DPIXELSHADERDEF def = {};
                // The picture; its red, green and blue, each a dot product with one of them; and the table, there.
                def.PSTextureModes = PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_DOTPRODUCT, PS_TEXTUREMODES_DOTPRODUCT, PS_TEXTUREMODES_DOT_STR_3D);
                def.PSDotMapping = PS_DOTMAPPING(0, PS_DOTMAPPING_ZERO_TO_ONE, PS_DOTMAPPING_ZERO_TO_ONE, PS_DOTMAPPING_ZERO_TO_ONE);
                def.PSInputTexture = PS_INPUTTEXTURE(0, 0, 0, 0);
                def.PSCombinerCount = PS_COMBINERCOUNT(1, PS_COMBINERCOUNT_UNIQUE_C0);
                // As much of what the table says as is asked for, and the rest of what it was.
                def.PSRGBInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T3 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_T0 | PS_CHANNEL_RGB,
                                                       PS_REGISTER_C0 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT);
                def.PSRGBOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R0, 0);
                def.PSAlphaInputs[0] = PS_COMBINERINPUTS(kOne | PS_CHANNEL_ALPHA, kOne | PS_CHANNEL_ALPHA, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSAlphaOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                def.PSC0Mapping = PS_CONSTANTMAPPING(0, 15, 15, 15, 15, 15, 15, 15);
                def.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 15, 15, 15, 15, 15, 15);
                if (FAILED(m_device->CreatePixelShader(&def, &m_gradeShader)))
                {
                    NX_WARN("d3d8: no grading");
                    m_gradeShader = 0;
                }
                return m_gradeShader != 0;
            }

            void makeShadowShader()
            {
                D3DPIXELSHADERDEF def = {};
                def.PSTextureModes = PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_PASSTHRU, PS_TEXTUREMODES_PROJECT3D, PS_TEXTUREMODES_NONE);
                def.PSCombinerCount = PS_COMBINERCOUNT(5, PS_COMBINERCOUNT_UNIQUE_C0 | PS_COMBINERCOUNT_UNIQUE_C1);
                // Texture by light.
                def.PSRGBInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_RGB, PS_REGISTER_V0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSAlphaInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_ALPHA, PS_REGISTER_V0 | PS_CHANNEL_ALPHA, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[0] = def.PSAlphaOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // By tint.
                def.PSRGBInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSAlphaInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_ALPHA, PS_REGISTER_C0 | PS_CHANNEL_ALPHA, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[1] = def.PSAlphaOutputs[1] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // Where the sun doesn't reach, by how much this faces it: what turns away has already lost it, and would
                // only show the steps of the map.
                def.PSRGBInputs[2] = PS_COMBINERINPUTS(PS_REGISTER_T1 | PS_CHANNEL_RGB, PS_REGISTER_T2 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[2] = PS_COMBINEROUTPUTS(PS_REGISTER_R1, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_COMBINEROUTPUT_SHIFTLEFT_1);
                def.PSRGBInputs[3] = PS_COMBINERINPUTS(PS_REGISTER_R1 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[3] = PS_COMBINEROUTPUTS(PS_REGISTER_R1, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                def.PSRGBInputs[4] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_R1 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[4] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // Which of the constants each stage has: the tint, and how dark.
                def.PSC0Mapping = PS_CONSTANTMAPPING(15, 0, 15, 1, 15, 15, 15, 15);
                def.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 15, 15, 15, 15, 15, 15);
                if (FAILED(m_device->CreatePixelShader(&def, &m_shadowShader)))
                {
                    NX_WARN("d3d8: no shadows");
                    m_shadowShader = 0;
                }
            }

            static size_t meshBytes(const MeshSlot& m)
            {
                return m.vertexCount * sizeof(MeshVertex) * (m.dynamic ? kStreamFrames : 1) + m.indexCount * sizeof(u16);
            }

            static void releaseMesh(MeshSlot& m)
            {
                for (IDirect3DVertexBuffer8*& buffer : m.vertices)
                {
                    if (buffer)
                    {
                        buffer->Release();
                        buffer = nullptr;
                    }
                }
                if (m.indices)
                {
                    m.indices->Release();
                    m.indices = nullptr;
                }
            }

            void bind(const PipelineDesc& desc)
            {
                m_device->SetRenderState(D3DRS_CULLMODE, desc.cull == Cull::Back ? D3DCULL_CW : D3DCULL_NONE);
                // As in D3D11, depth is written only while the test is on, so writes alone always pass.
                m_device->SetRenderState(D3DRS_ZENABLE, desc.depthTest || desc.depthWrite ? D3DZB_TRUE : D3DZB_FALSE);
                m_device->SetRenderState(D3DRS_ZFUNC, desc.depthTest ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, desc.depthWrite);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, desc.blend != Blend::None);
                m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                m_device->SetRenderState(D3DRS_SRCBLEND, desc.blend == Blend::Multiply ? D3DBLEND_DESTCOLOR : D3DBLEND_SRCALPHA);
                m_device->SetRenderState(D3DRS_DESTBLEND, desc.blend == Blend::Additive ? D3DBLEND_ONE : desc.blend == Blend::Multiply ? D3DBLEND_ZERO : D3DBLEND_INVSRCALPHA);
            }

            IDirect3D8* m_d3d = nullptr;
            IDirect3DDevice8* m_device = nullptr;
            IDirect3DVertexBuffer8* m_streams[kStreamFrames] = {};
            HandlePool<BufferSlot, BufferTag> m_buffers;
            HandlePool<PipelineSlot, PipelineTag> m_pipelines;
            HandlePool<TextureSlot, TextureTag> m_textures;
            HandlePool<MeshSlot, MeshTag> m_meshes;
            HandlePool<EffectSlot, EffectTag> m_effects;
            Texture m_lava, m_noise, m_lines, m_stripes;
            IDirect3DTexture8* m_copy = nullptr;
            bool m_copyTried = false;
            IDirect3DTexture8* m_shadowColor = nullptr;
            IDirect3DTexture8* m_shadowDepth = nullptr;
            IDirect3DSurface8* m_savedColor = nullptr;
            IDirect3DSurface8* m_savedDepth = nullptr;
            bool m_shadowsTried = false, m_inShadows = false, m_shadowed = false;
            float m_shade = 0;
            DWORD m_shadowShader = 0, m_toneShader = 0, m_edgeShader = 0, m_gradeShader = 0;
            bool m_gradeTried = false;
            IDirect3DTexture8* m_bloom[2] = {};
            D3DTexture m_screen;
            bool m_bloomTried = false;
            View m_view;
            Mode m_mode = Mode::None;
            Texture m_white;
            Pipeline m_bound;
            Texture m_boundTexture;
            Stats m_current;
            Stats m_last;
            size_t m_bufferBytes = 0;
            size_t m_textureBytes = 0;
            u32 m_streamed = 0;
            u32 m_frameNumber = 0;
            u32 m_streamIndex = 0;
            u32 m_width = 0;
            u32 m_height = 0;
            // While a picture is drawn into: which, and the screen's to go back to.
            Texture m_drawing;
            u32 m_screenWidth = 0, m_screenHeight = 0;
            IDirect3DSurface8* m_screenColor = nullptr;
            IDirect3DSurface8* m_screenDepth = nullptr;
            bool m_inFrame = false;
        };
    }

    Device* createDevice(void*)
    {
        D3D8Device* device = create<D3D8Device>(MemTag::General);
        if (device && !device->init())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
