#include "gpu/gpu.h"

#include <xtl.h>
#include <xgraphics.h>

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
            TextureInfo info;
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
                    if (m_shadowShader)
                    {
                        m_device->DeletePixelShader(m_shadowShader);
                    }
                }
                m_buffers.forEach([](BufferSlot& b) { b.buffer->Release(); });
                m_textures.forEach([](TextureSlot& t) { t.texture->Release(); });
                m_meshes.forEach([](MeshSlot& m) { releaseMesh(m); });
                for (IDirect3DTexture8* t : { m_shadowColor, m_shadowDepth })
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
                if (!m_buffers.init(1024, MemTag::Buffer) || !m_pipelines.init(64, MemTag::Buffer) || !m_textures.init(1024, MemTag::Texture) || !m_meshes.init(2048, MemTag::Buffer))
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
                    handle = m_textures.add({ texture, info });
                }
                if (!handle && texture)
                {
                    texture->Release();
                }
                m_textureBytes += handle ? m_textures.get(handle)->info.bytes : 0;
                return handle;
            }

            void destroyTexture(Texture handle) override
            {
                if (TextureSlot* t = m_textures.get(handle))
                {
                    m_device->SetTexture(0, nullptr);
                    m_textureBytes -= t->info.bytes;
                    t->texture->Release();
                    m_textures.remove(handle);
                    m_boundTexture = {};
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
                const bool shaded = m_shadowed && lit && draw.blend == Blend::None;
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
                m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                m_device->SetRenderState(D3DRS_DESTBLEND, draw.blend == Blend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
                m_device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(draw.world.m));
                m_device->SetStreamSource(0, m->vertices[m->current], sizeof(MeshVertex));
                m_device->SetIndices(m->indices, 0);
                m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, m->vertexCount, draw.first, count / 3);
                m_current.draws++;
                m_current.triangles += count / 3;
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
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                m_device->EndScene();
                m_device->Present(nullptr, nullptr, nullptr, nullptr);
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
                m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                m_device->SetRenderState(D3DRS_DESTBLEND, desc.blend == Blend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
            }

            IDirect3D8* m_d3d = nullptr;
            IDirect3DDevice8* m_device = nullptr;
            IDirect3DVertexBuffer8* m_streams[kStreamFrames] = {};
            HandlePool<BufferSlot, BufferTag> m_buffers;
            HandlePool<PipelineSlot, PipelineTag> m_pipelines;
            HandlePool<TextureSlot, TextureTag> m_textures;
            HandlePool<MeshSlot, MeshTag> m_meshes;
            IDirect3DTexture8* m_shadowColor = nullptr;
            IDirect3DTexture8* m_shadowDepth = nullptr;
            IDirect3DSurface8* m_savedColor = nullptr;
            IDirect3DSurface8* m_savedDepth = nullptr;
            bool m_shadowsTried = false, m_inShadows = false, m_shadowed = false;
            float m_shade = 0;
            DWORD m_shadowShader = 0;
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
            u32 m_streamIndex = 0;
            u32 m_width = 0;
            u32 m_height = 0;
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
