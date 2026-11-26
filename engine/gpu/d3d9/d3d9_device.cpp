#include "gpu/gpu.h"
#include "gpu/d3d9/d3d9_shaders.h"
#include "gpu/mesh_uniforms.h"
#include "base/handle.h"
#include "base/log.h"

#include <xtl.h>
#include <xgraphics.h>

#include <string.h>

// Direct3D 9 on the Xbox 360. The shaders are HLSL, compiled when the device starts. The CPU is big endian and the
// GPU reads what it is given as such, except where a format says otherwise: pictures and vertex colours are bytes in
// r, g, b, a order, so they use formats that leave them as they are.
namespace nx::gpu
{
    namespace
    {
        // Bytes r, g, b, a, read as they lie.
        const D3DFORMAT kRgba = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        // Compressed blocks as a PC writes them.
        const D3DFORMAT kBc1 = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_DXT1, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        const D3DFORMAT kBc3 = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_DXT4_5, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        // And in vertices, where the GPU swaps the bytes of each word as it reads them.
        const D3DDECLTYPE kColorBytes = D3DDECLTYPE_UBYTE4N;

        constexpr u32 kWidth = 1280, kHeight = 720;
        // One more than there can be frames on their way, so a dynamic mesh is never written while it is read.
        constexpr u32 kCopies = 3;

        struct BufferSlot
        {
            IDirect3DVertexBuffer9* buffer;
            u32 count;
        };

        struct PipelineSlot
        {
            PipelineDesc desc;
        };

        struct TextureSlot
        {
            IDirect3DTexture9* texture;
            IDirect3DVolumeTexture9* volume;
            TextureInfo info;
        };

        struct MeshSlot
        {
            IDirect3DVertexBuffer9* vertices[kCopies];
            IDirect3DIndexBuffer9* indices;
            u32 vertexCount, indexCount, current;
            bool dynamic;
        };

        struct EffectSlot
        {
            EffectKind kind;
        };

        template <typename T>
        void release(T*& p)
        {
            if (p)
            {
                p->Release();
                p = nullptr;
            }
        }

        // The constants every mesh shader reads: the view's, then the object's.
        struct ObjectConstants
        {
            Mat4 world;
            float tint[4], surface[4], unused[4], wind[4], volume[4];
            float lightPlace[kMeshLights][4], lightColor[kMeshLights][4];
            float shadowInfo[4];
        };
        static_assert(sizeof(ViewUniforms) == 52 * 16, "the view is c0 to c51");

        class D3D9Device final : public Device
        {
        public:
            ~D3D9Device() override
            {
                m_buffers.forEach([](BufferSlot& b) { release(b.buffer); });
                m_textures.forEach([](TextureSlot& t) { release(t.texture); release(t.volume); });
                m_meshes.forEach([](MeshSlot& m) { releaseMesh(m); });
                release(m_flatDecl);
                release(m_meshDecl);
                release(m_screenDecl);
                release(m_flatVs);
                release(m_flatPs);
                release(m_meshVs);
                release(m_meshPs);
                release(m_shadowVs);
                release(m_shadowPs);
                release(m_screenVs);
                release(m_skyPs);
                release(m_device);
                release(m_d3d);
            }

            bool init()
            {
                if (!m_buffers.init(4096, MemTag::Buffer) || !m_pipelines.init(256, MemTag::Buffer) || !m_textures.init(4096, MemTag::Texture) || !m_meshes.init(4096, MemTag::Buffer) ||
                    !m_effects.init(16, MemTag::Buffer))
                {
                    return false;
                }
                m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
                if (m_d3d == nullptr)
                {
                    NX_ERROR("d3d9: no Direct3D");
                    return false;
                }
                D3DPRESENT_PARAMETERS pp = {};
                pp.BackBufferWidth = kWidth;
                pp.BackBufferHeight = kHeight;
                pp.BackBufferFormat = D3DFMT_A8R8G8B8;
                pp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
                pp.MultiSampleType = D3DMULTISAMPLE_NONE;
                pp.BackBufferCount = 1;
                pp.EnableAutoDepthStencil = TRUE;
                pp.AutoDepthStencilFormat = D3DFMT_D24S8;
                pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
                pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
                if (FAILED(m_d3d->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &m_device)))
                {
                    NX_ERROR("d3d9: no device");
                    return false;
                }
                if (!createShaders() || !createDeclarations())
                {
                    return false;
                }
                const u8 white[4] = { 255, 255, 255, 255 };
                m_white = createTexture(rgbaTexture(white, 1, 1));
                return static_cast<bool>(m_white);
            }

            const char* name() const override { return "d3d9"; }

            void size(u32& width, u32& height) const override
            {
                width = kWidth;
                height = kHeight;
            }

            // --- 2D ---

            Buffer createVertexBuffer(const Vertex* vertices, u32 count) override
            {
                if (vertices == nullptr || count == 0)
                {
                    return {};
                }
                const UINT bytes = count * static_cast<UINT>(sizeof(Vertex));
                IDirect3DVertexBuffer9* buffer = nullptr;
                void* to = nullptr;
                if (FAILED(m_device->CreateVertexBuffer(bytes, 0, 0, D3DPOOL_DEFAULT, &buffer, nullptr)) || FAILED(buffer->Lock(0, 0, &to, 0)))
                {
                    release(buffer);
                    return {};
                }
                memcpy(to, vertices, bytes);
                buffer->Unlock();
                const Buffer handle = m_buffers.add({ buffer, count });
                if (!handle)
                {
                    release(buffer);
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
                    m_device->SetStreamSource(0, nullptr, 0, 0);
                    release(b->buffer);
                    m_buffers.remove(handle);
                }
            }

            Pipeline createPipeline(const PipelineDesc& desc) override { return m_pipelines.add({ desc }); }
            void destroyPipeline(Pipeline handle) override { m_pipelines.remove(handle); }

            Texture createTexture(const TextureData& source) override
            {
                if (!validTexture(source))
                {
                    return {};
                }
                const bool block = source.format != PixelFormat::Rgba8;
                const bool mips = source.mips && !block;
                u32 levels = 1;
                for (u32 w = source.fullWidth, h = source.fullHeight; mips && (w > 1 || h > 1); w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
                {
                    levels++;
                }
                const D3DFORMAT format = source.format == PixelFormat::Bc1 ? kBc1 : source.format == PixelFormat::Bc3 ? kBc3 : kRgba;
                IDirect3DTexture9* texture = nullptr;
                if (FAILED(m_device->CreateTexture(source.fullWidth, source.fullHeight, levels, 0, format, D3DPOOL_DEFAULT, &texture, nullptr)))
                {
                    NX_ERROR("d3d9: no texture of %ux%u", source.fullWidth, source.fullHeight);
                    return {};
                }
                // Each level, row by row: a block row is four rows of pixels.
                const u32 rowBytes = block ? source.fullWidth / 4 * (source.format == PixelFormat::Bc1 ? 8 : 16) : source.fullWidth * 4;
                const u32 rows = block ? source.fullHeight / 4 : source.fullHeight;
                if (!fill(texture, 0, source.pixels, rowBytes, rows))
                {
                    release(texture);
                    return {};
                }
                size_t bytes = pixelBytes(source.format, source.fullWidth, source.fullHeight);
                if (levels > 1)
                {
                    bytes += bytes / 3;
                    makeMips(texture, source.pixels, source.fullWidth, source.fullHeight, levels);
                }
                TextureSlot t = {};
                t.texture = texture;
                t.info.width = source.width;
                t.info.height = source.height;
                t.info.uMax = static_cast<float>(source.width) / static_cast<float>(source.fullWidth);
                t.info.vMax = static_cast<float>(source.height) / static_cast<float>(source.fullHeight);
                t.info.bytes = bytes;
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    release(texture);
                    return {};
                }
                m_textureBytes += bytes;
                return handle;
            }

            void destroyTexture(Texture handle) override
            {
                if (TextureSlot* t = m_textures.get(handle))
                {
                    for (DWORD i = 0; i < 4; i++)
                    {
                        m_device->SetTexture(i, nullptr);
                    }
                    m_textureBytes -= t->info.bytes;
                    release(t->texture);
                    release(t->volume);
                    m_textures.remove(handle);
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
                m_streamed = 0;
                m_mode = Mode::None;
                wholeViewport();
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_COLORVALUE(clear.r, clear.g, clear.b, clear.a), 1.0f, 0);
                return true;
            }

            u32 stream(const Vertex* vertices, u32 count) override
            {
                NX_ASSERT(m_inFrame && vertices != nullptr);
                if (count > kStreamVertices - m_streamed)
                {
                    return kStreamFull;
                }
                const u32 first = m_streamed;
                memcpy(m_stream + first, vertices, static_cast<size_t>(count) * sizeof(Vertex));
                m_streamed += count;
                return first;
            }

            void draw(const Draw& draw) override
            {
                NX_ASSERT(m_inFrame);
                BufferSlot* buffer = m_buffers.get(draw.vertices);
                PipelineSlot* pipeline = m_pipelines.get(draw.pipeline);
                TextureSlot* texture = m_textures.get(draw.texture ? draw.texture : m_white);
                const u32 available = draw.vertices ? (buffer ? buffer->count : 0) : m_streamed;
                if ((draw.vertices && buffer == nullptr) || pipeline == nullptr || texture == nullptr || draw.first > available || draw.count > available - draw.first)
                {
                    m_current.rejected++;
                    return;
                }
                if (draw.count == 0)
                {
                    return;
                }
                use(Mode::Flat);
                const PipelineDesc& p = pipeline->desc;
                setBlend(p.blend);
                m_device->SetRenderState(D3DRS_CULLMODE, p.cull == Cull::Back ? D3DCULL_CW : D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_ZENABLE, p.depthTest || p.depthWrite ? D3DZB_TRUE : D3DZB_FALSE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, p.depthWrite ? TRUE : FALSE);
                m_device->SetRenderState(D3DRS_ZFUNC, p.depthTest ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS);
                m_device->SetTexture(0, texture->texture);
                setSampler(0, draw.filter == Filter::Nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                setScissor(draw.scissor);
                m_device->SetVertexShaderConstantF(0, draw.transform.m, 4);
                const bool lines = p.topology == Topology::Lines;
                const D3DPRIMITIVETYPE type = lines ? D3DPT_LINELIST : D3DPT_TRIANGLELIST;
                const u32 primitives = lines ? draw.count / 2 : draw.count / 3;
                if (primitives)
                {
                    if (buffer)
                    {
                        m_device->SetStreamSource(0, buffer->buffer, 0, sizeof(Vertex));
                        m_device->DrawPrimitive(type, draw.first, primitives);
                    }
                    else
                    {
                        m_device->DrawPrimitiveUP(type, primitives, m_stream + draw.first, sizeof(Vertex));
                    }
                }
                m_current.draws++;
                m_current.vertices += draw.count;
            }

            void endFrame() override
            {
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_device->Present(nullptr, nullptr, nullptr, nullptr);
                m_last = m_current;
                m_frame++;
            }

            // --- Meshes ---

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
                const UINT vertexBytes = vertexCount * static_cast<UINT>(sizeof(MeshVertex));
                const UINT indexBytes = indexCount * static_cast<UINT>(sizeof(u16));
                bool fine = true;
                for (u32 i = 0; fine && i < (dynamic ? kCopies : 1); i++)
                {
                    void* to = nullptr;
                    fine = SUCCEEDED(m_device->CreateVertexBuffer(vertexBytes, 0, 0, D3DPOOL_DEFAULT, &m.vertices[i], nullptr)) && SUCCEEDED(m.vertices[i]->Lock(0, 0, &to, 0));
                    if (fine)
                    {
                        memcpy(to, vertices, vertexBytes);
                        m.vertices[i]->Unlock();
                    }
                }
                void* to = nullptr;
                fine = fine && SUCCEEDED(m_device->CreateIndexBuffer(indexBytes, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m.indices, nullptr)) && SUCCEEDED(m.indices->Lock(0, 0, &to, 0));
                if (fine)
                {
                    memcpy(to, indices, indexBytes);
                    m.indices->Unlock();
                }
                const Mesh handle = fine ? m_meshes.add(m) : Mesh();
                if (!handle)
                {
                    releaseMesh(m);
                    return {};
                }
                m_bufferBytes += meshBytes(m);
                return handle;
            }

            void updateMesh(Mesh handle, const MeshVertex* vertices, u32 count) override
            {
                MeshSlot* m = m_meshes.get(handle);
                if (m == nullptr || !m->dynamic || count > m->vertexCount)
                {
                    return;
                }
                m->current = (m->current + 1) % kCopies;
                void* to = nullptr;
                if (SUCCEEDED(m->vertices[m->current]->Lock(0, 0, &to, 0)))
                {
                    memcpy(to, vertices, count * sizeof(MeshVertex));
                    m->vertices[m->current]->Unlock();
                }
            }

            void destroyMesh(Mesh handle) override
            {
                if (MeshSlot* m = m_meshes.get(handle))
                {
                    m_device->SetStreamSource(0, nullptr, 0, 0);
                    m_device->SetIndices(nullptr);
                    m_bufferBytes -= meshBytes(*m);
                    releaseMesh(*m);
                    m_meshes.remove(handle);
                }
            }

            void beginView(const View& view) override
            {
                NX_ASSERT(m_inFrame);
                m_view = view;
                fillView(m_viewUniforms, view, false);
                m_area = view.area;
                m_mode = Mode::None;
                m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
            }

            void drawMesh(const MeshDraw& draw) override
            {
                NX_ASSERT(m_inFrame);
                const MeshSlot* m = m_meshes.get(draw.mesh);
                const TextureSlot* texture = m_textures.get(draw.texture ? draw.texture : m_white);
                const u32 count = m && draw.count == 0 && draw.first <= m->indexCount ? m->indexCount - draw.first : draw.count;
                if (m == nullptr || texture == nullptr || draw.first > m->indexCount || count > m->indexCount - draw.first || draw.lightCount > kMeshLights)
                {
                    m_current.rejected++;
                    return;
                }
                use(Mode::Mesh);
                ObjectConstants o = {};
                o.world = draw.world;
                put(o.tint, draw.color, draw.color.a);
                o.surface[0] = draw.glow;
                o.surface[1] = draw.shine;
                o.surface[2] = draw.lit ? draw.indoors ? 2.0f : 1.0f : 0.0f;
                o.surface[3] = draw.fogged ? 1.0f : 0.0f;
                o.wind[0] = draw.bendX;
                o.wind[1] = draw.bendZ;
                o.wind[2] = draw.cutout;
                const TextureSlot* volume = draw.volume ? m_textures.get(draw.volume) : nullptr;
                put(o.volume, draw.volumeOffset, volume && volume->volume ? draw.volumeScale : 0.0f);
                for (u32 i = 0; i < draw.lightCount; i++)
                {
                    const u32 index = draw.lights[i];
                    if (index < m_view.lightCount)
                    {
                        const PointLight& l = m_view.lights[index];
                        put(o.lightPlace[i], l.place, l.range > 0 ? 1 / l.range : 1);
                        put(o.lightColor[i], l.color, 1);
                    }
                }
                m_device->SetVertexShaderConstantF(52, o.world.m, sizeof(o) / 16);
                m_device->SetPixelShaderConstantF(52, o.world.m, sizeof(o) / 16);
                setBlend(draw.blend);
                m_device->SetRenderState(D3DRS_CULLMODE, draw.cull == Cull::Back && !draw.lines ? D3DCULL_CW : D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_FILLMODE, draw.lines ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, draw.depthWrite ? TRUE : FALSE);
                m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                m_device->SetTexture(0, texture->texture);
                setSampler(0, D3DTEXF_LINEAR, draw.repeat ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP, draw.repeat);
                if (volume && volume->volume)
                {
                    m_device->SetTexture(2, volume->volume);
                    setSampler(2, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, false);
                }
                m_device->SetStreamSource(0, m->vertices[m->current], 0, sizeof(MeshVertex));
                m_device->SetIndices(m->indices);
                m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m->vertexCount, draw.first, count / 3);
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                m_current.draws++;
                m_current.triangles += count / 3;
            }

            bool drawSky() override
            {
                use(Mode::Screen);
                m_device->SetPixelShader(m_skyPs);
                setBlend(Blend::None);
                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                static const float corners[6] = { -1, -1, 3, -1, -1, 3 };
                m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, corners, sizeof(float) * 2);
                m_current.draws++;
                return true;
            }

            bool drawField(const Field&) override { return false; }
            bool beginShadows(const Mat4&) override { return false; }
            void endShadows() override {}

            bool capture(u8*, size_t) override { return false; }

            Stats stats() const override
            {
                Stats s = m_last;
                s.bufferBytes = m_bufferBytes;
                s.textureBytes = m_textureBytes;
                return s;
            }

        private:
            enum class Mode : u8 { None, Flat, Mesh, Screen };

            void use(Mode mode)
            {
                if (m_mode == mode)
                {
                    return;
                }
                m_mode = mode;
                if (mode == Mode::Flat)
                {
                    wholeViewport();
                    m_device->SetVertexDeclaration(m_flatDecl);
                    m_device->SetVertexShader(m_flatVs);
                    m_device->SetPixelShader(m_flatPs);
                    return;
                }
                // The picture's own part of the screen, and what the view says about it.
                D3DVIEWPORT9 viewport = { 0, 0, kWidth, kHeight, 0, 1 };
                if (m_area.enabled)
                {
                    viewport = { static_cast<DWORD>(m_area.x), static_cast<DWORD>(m_area.y), static_cast<DWORD>(m_area.width), static_cast<DWORD>(m_area.height), 0, 1 };
                }
                m_device->SetViewport(&viewport);
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_device->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(&m_viewUniforms), 52);
                m_device->SetPixelShaderConstantF(0, reinterpret_cast<const float*>(&m_viewUniforms), 52);
                if (mode == Mode::Mesh)
                {
                    m_device->SetVertexDeclaration(m_meshDecl);
                    m_device->SetVertexShader(m_meshVs);
                    m_device->SetPixelShader(m_meshPs);
                }
                else
                {
                    m_device->SetVertexDeclaration(m_screenDecl);
                    m_device->SetVertexShader(m_screenVs);
                }
            }

            void wholeViewport()
            {
                const D3DVIEWPORT9 viewport = { 0, 0, kWidth, kHeight, 0, 1 };
                m_device->SetViewport(&viewport);
            }

            void setBlend(Blend blend)
            {
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, blend != Blend::None ? TRUE : FALSE);
                if (blend != Blend::None)
                {
                    m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                    m_device->SetRenderState(D3DRS_DESTBLEND, blend == Blend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
                    m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
                }
            }

            void setSampler(DWORD stage, D3DTEXTUREFILTERTYPE filter, D3DTEXTUREADDRESS address, bool mips)
            {
                m_device->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
                m_device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
                m_device->SetSamplerState(stage, D3DSAMP_MIPFILTER, mips ? D3DTEXF_LINEAR : D3DTEXF_NONE);
                m_device->SetSamplerState(stage, D3DSAMP_ADDRESSU, address);
                m_device->SetSamplerState(stage, D3DSAMP_ADDRESSV, address);
                m_device->SetSamplerState(stage, D3DSAMP_ADDRESSW, address);
            }

            void setScissor(const Scissor& s)
            {
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, s.enabled ? TRUE : FALSE);
                if (s.enabled)
                {
                    RECT r = { s.x, s.y, s.x + s.width, s.y + s.height };
                    r.left = r.left < 0 ? 0 : r.left;
                    r.top = r.top < 0 ? 0 : r.top;
                    r.right = r.right < r.left ? r.left : r.right;
                    r.bottom = r.bottom < r.top ? r.top : r.bottom;
                    m_device->SetScissorRect(&r);
                }
            }

            static bool fill(IDirect3DTexture9* texture, UINT level, const u8* pixels, u32 rowBytes, u32 rows)
            {
                D3DLOCKED_RECT locked;
                if (FAILED(texture->LockRect(level, &locked, nullptr, 0)))
                {
                    return false;
                }
                for (u32 y = 0; y < rows; y++)
                {
                    memcpy(static_cast<u8*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch, pixels + static_cast<size_t>(y) * rowBytes, rowBytes);
                }
                texture->UnlockRect(level);
                return true;
            }

            // Each smaller copy, from the one before: the average of four.
            static void makeMips(IDirect3DTexture9* texture, const u8* pixels, u32 width, u32 height, u32 levels)
            {
                u8* from = static_cast<u8*>(memAlloc(static_cast<size_t>(width) * height * 4, MemTag::Texture));
                if (from == nullptr)
                {
                    return;
                }
                memcpy(from, pixels, static_cast<size_t>(width) * height * 4);
                for (u32 level = 1; level < levels; level++)
                {
                    const u32 w = width > 1 ? width / 2 : 1, h = height > 1 ? height / 2 : 1;
                    for (u32 y = 0; y < h; y++)
                    {
                        for (u32 x = 0; x < w; x++)
                        {
                            const u32 x0 = x * 2 < width ? x * 2 : width - 1, x1 = x * 2 + 1 < width ? x * 2 + 1 : width - 1;
                            const u32 y0 = y * 2 < height ? y * 2 : height - 1, y1 = y * 2 + 1 < height ? y * 2 + 1 : height - 1;
                            for (u32 k = 0; k < 4; k++)
                            {
                                const u32 sum = from[(y0 * width + x0) * 4 + k] + from[(y0 * width + x1) * 4 + k] + from[(y1 * width + x0) * 4 + k] + from[(y1 * width + x1) * 4 + k];
                                from[(y * w + x) * 4 + k] = static_cast<u8>((sum + 2) / 4);
                            }
                        }
                    }
                    width = w;
                    height = h;
                    fill(texture, level, from, width * 4, height);
                }
                memFree(from);
            }

            static size_t meshBytes(const MeshSlot& m) { return m.vertexCount * sizeof(MeshVertex) * (m.dynamic ? kCopies : 1) + m.indexCount * sizeof(u16); }

            static void releaseMesh(MeshSlot& m)
            {
                for (IDirect3DVertexBuffer9*& v : m.vertices)
                {
                    release(v);
                }
                release(m.indices);
            }

            // --- Shaders ---

            bool compile(const char* source, size_t length, const char* entry, const char* profile, ID3DXBuffer*& code)
            {
                ID3DXBuffer* errors = nullptr;
                const HRESULT hr = D3DXCompileShader(source, static_cast<UINT>(length), nullptr, nullptr, entry, profile, 0, &code, &errors, nullptr);
                if (FAILED(hr))
                {
                    NX_ERROR("d3d9: %s: %s", entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "can't be compiled");
                    release(errors);
                    return false;
                }
                release(errors);
                return true;
            }

            bool vertexShader(const char* source, size_t length, const char* entry, IDirect3DVertexShader9*& out)
            {
                ID3DXBuffer* code = nullptr;
                const bool fine = compile(source, length, entry, "vs_3_0", code) && SUCCEEDED(m_device->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &out));
                release(code);
                return fine;
            }

            bool pixelShader(const char* source, size_t length, const char* entry, IDirect3DPixelShader9*& out)
            {
                ID3DXBuffer* code = nullptr;
                const bool fine = compile(source, length, entry, "ps_3_0", code) && SUCCEEDED(m_device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &out));
                release(code);
                return fine;
            }

            bool createShaders()
            {
                using namespace shaders360;
                const size_t common = sizeof(kMeshCommon) - 1, main = sizeof(kMeshMain) - 1;
                char* mesh = static_cast<char*>(memAlloc(common + main + 1, MemTag::General));
                if (mesh == nullptr)
                {
                    return false;
                }
                memcpy(mesh, kMeshCommon, common);
                memcpy(mesh + common, kMeshMain, main + 1);
                const size_t length = common + main;
                const bool fine = vertexShader(kFlat, sizeof(kFlat) - 1, "flatVs", m_flatVs) && pixelShader(kFlat, sizeof(kFlat) - 1, "flatPs", m_flatPs) &&
                                  vertexShader(mesh, length, "meshVs", m_meshVs) && pixelShader(mesh, length, "meshPs", m_meshPs) &&
                                  vertexShader(mesh, length, "shadowVs", m_shadowVs) && pixelShader(mesh, length, "shadowPs", m_shadowPs) &&
                                  vertexShader(mesh, length, "screenVs", m_screenVs) && pixelShader(mesh, length, "skyPs", m_skyPs);
                memFree(mesh);
                return fine;
            }

            bool createDeclarations()
            {
                const D3DVERTEXELEMENT9 flat[] = {
                    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                    { 0, 12, kColorBytes, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
                    { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                    D3DDECL_END(),
                };
                const D3DVERTEXELEMENT9 mesh[] = {
                    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
                    { 0, 24, kColorBytes, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
                    { 0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                    D3DDECL_END(),
                };
                const D3DVERTEXELEMENT9 screen[] = {
                    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                    D3DDECL_END(),
                };
                return SUCCEEDED(m_device->CreateVertexDeclaration(flat, &m_flatDecl)) && SUCCEEDED(m_device->CreateVertexDeclaration(mesh, &m_meshDecl)) &&
                       SUCCEEDED(m_device->CreateVertexDeclaration(screen, &m_screenDecl));
            }

            IDirect3D9* m_d3d = nullptr;
            IDirect3DDevice9* m_device = nullptr;
            IDirect3DVertexDeclaration9* m_flatDecl = nullptr;
            IDirect3DVertexDeclaration9* m_meshDecl = nullptr;
            IDirect3DVertexDeclaration9* m_screenDecl = nullptr;
            IDirect3DVertexShader9* m_flatVs = nullptr;
            IDirect3DPixelShader9* m_flatPs = nullptr;
            IDirect3DVertexShader9* m_meshVs = nullptr;
            IDirect3DPixelShader9* m_meshPs = nullptr;
            IDirect3DVertexShader9* m_shadowVs = nullptr;
            IDirect3DPixelShader9* m_shadowPs = nullptr;
            IDirect3DVertexShader9* m_screenVs = nullptr;
            IDirect3DPixelShader9* m_skyPs = nullptr;

            HandlePool<BufferSlot, BufferTag> m_buffers;
            HandlePool<PipelineSlot, PipelineTag> m_pipelines;
            HandlePool<TextureSlot, TextureTag> m_textures;
            HandlePool<MeshSlot, MeshTag> m_meshes;
            HandlePool<EffectSlot, EffectTag> m_effects;
            Texture m_white;

            Vertex m_stream[kStreamVertices];
            u32 m_streamed = 0;
            bool m_inFrame = false;
            Mode m_mode = Mode::None;
            View m_view;
            ViewUniforms m_viewUniforms = {};
            Scissor m_area;
            Stats m_current, m_last;
            size_t m_bufferBytes = 0, m_textureBytes = 0;
            u64 m_frame = 0;
        };
    }

    Device* createDevice(void*)
    {
        D3D9Device* device = create<D3D9Device>(MemTag::General);
        if (device && !device->init())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
