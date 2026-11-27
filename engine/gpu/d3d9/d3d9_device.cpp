#include "gpu/gpu.h"
#include "gpu/d3d9/d3d9_shaders.h"
#include "gpu/d3d9/space_shaders360.h"
#include "gpu/mesh_uniforms.h"
#include "base/handle.h"
#include "base/log.h"

#include <xtl.h>
#include <xgraphics.h>

#include <string.h>

#include <string>

// Direct3D 9 on the Xbox 360. The shaders are HLSL, compiled when the device starts. The CPU is big endian and the
// GPU reads what it is given as such, except where a format says otherwise: pictures and vertex colours are bytes in
// r, g, b, a order, so they use formats that leave them as they are.
namespace nx::gpu
{
    namespace
    {
        // Bytes r, g, b, a, read as they lie.
        const D3DFORMAT kRgba = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        // The same, laid out in the GPU's own order, as volumes must be.
        const D3DFORMAT kRgbaTiled = static_cast<D3DFORMAT>(MAKED3DFMT(GPUTEXTUREFORMAT_8_8_8_8, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR));
        // Compressed blocks as a PC writes them.
        const D3DFORMAT kBc1 = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_DXT1, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        const D3DFORMAT kBc3 = static_cast<D3DFORMAT>(MAKELINFMT(MAKED3DFMT(GPUTEXTUREFORMAT_DXT4_5, GPUENDIAN_NONE, TRUE, GPUSIGN_ALL_UNSIGNED, GPUNUMFORMAT_FRACTION, GPUSWIZZLE_ABGR)));
        // And in vertices, where the GPU swaps the bytes of each word as it reads them.
        const D3DDECLTYPE kColorBytes = D3DDECLTYPE_UBYTE4N;

        constexpr u32 kWidth = 1280, kHeight = 720;
        // The GPU draws into its own memory (EDRAM, 2048 tiles of 80x16 pixels) and copies the result out. The screen
        // has its colour and depth in the first 1440 tiles; what is drawn elsewhere in a frame goes after them, so the
        // screen is never written over.
        constexpr DWORD kScreenTiles = (kWidth / 80) * (kHeight / 16) * 2;
        constexpr u32 kShadowSide = 512;
        // Bloom is spread at a quarter of the size across.
        constexpr u32 kBloomWidth = 320, kBloomHeight = 180;
        constexpr DWORD tilesOf(u32 width, u32 height) { return ((width + 79) / 80) * ((height + 15) / 16); }
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
            // A picture to draw into: where in the GPU's memory it is drawn, which is the screen's while it is.
            IDirect3DSurface9* color;
            IDirect3DSurface9* depth;
            bool dynamic;    // changed by updateTexture
            u32 rows;
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
            IDirect3DVertexShader9* vertex;
            IDirect3DPixelShader9* pixel;
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
                release(m_shadowColor);
                release(m_shadowDepth);
                release(m_shadowTexture);
                m_effects.forEach([](EffectSlot& e) { release(e.vertex); release(e.pixel); });
                for (IDirect3DPixelShader9*& shader : m_postPs)
                {
                    release(shader);
                }
                release(m_postVs);
                release(m_screenCopy);
                release(m_small);
                for (IDirect3DPixelShader9*& s : m_spacePs)
                {
                    release(s);
                }
                release(m_depthCopy);
                release(m_work);
                release(m_meter[0]);
                release(m_meter[1]);
                release(m_bloom[0]);
                release(m_bloom[1]);
                release(m_device);
                release(m_d3d);
            }

            bool init()
            {
                if (!m_buffers.init(4096, MemTag::Buffer) || !m_pipelines.init(256, MemTag::Buffer) || !m_textures.init(4096, MemTag::Texture) || !m_meshes.init(4096, MemTag::Buffer) ||
                    !m_effects.init(128, MemTag::Buffer))
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
                if (!createShaders() || !createDeclarations() || !createShadowMap())
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
                width = m_width;
                height = m_height;
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
                const bool mips = source.mips && !block && !source.dynamic;
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
                t.dynamic = source.dynamic && !block;
                t.rows = source.fullHeight;
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    release(texture);
                    return {};
                }
                m_textureBytes += bytes;
                return handle;
            }

            bool updateTexture(Texture handle, const u8* rgba) override
            {
                const TextureSlot* t = m_textures.get(handle);
                if (t == nullptr || !t->dynamic || rgba == nullptr)
                {
                    return false;
                }
                D3DSURFACE_DESC desc;
                t->texture->GetLevelDesc(0, &desc);
                return fill(t->texture, 0, rgba, desc.Width * 4, t->rows);
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
                    release(t->color);
                    release(t->depth);
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
                m_viewDrawn = false;
                m_inFrame = true;
                m_current = {};
                m_streamed = 0;
                m_mode = Mode::None;
                m_shadowReady = false;
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
                if (draw.clearStencil)
                {
                    m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                    m_device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0);
                }
                setScissor(draw.scissor);
                stencil(draw.stencil, draw.stencilRef);
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
                stencil(Stencil::None, 0);
                m_current.draws++;
                m_current.vertices += draw.count;
            }

            void endFrame() override
            {
                endTarget();
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
                sceneOf(view, m_scene);
                m_viewDrawn = m_viewDrawn || !m_drawing;
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
                if (m_shadowing)
                {
                    m_device->SetVertexShaderConstantF(52, draw.world.m, 4);
                    m_device->SetStreamSource(0, m->vertices[m->current], 0, sizeof(MeshVertex));
                    m_device->SetIndices(m->indices);
                    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m->vertexCount, draw.first, count / 3);
                    m_current.draws++;
                    return;
                }
                use(Mode::Mesh);
                ObjectConstants o = {};
                o.shadowInfo[0] = m_shadowReady ? 1.0f : 0.0f;
                o.shadowInfo[1] = 1.0f / static_cast<float>(kShadowSide);
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
                m_device->SetTexture(1, m_shadowReady ? m_shadowTexture : nullptr);
                setSampler(1, D3DTEXF_POINT, D3DTADDRESS_CLAMP, false);
                if (volume && volume->volume)
                {
                    m_device->SetTexture(2, volume->volume);
                    setSampler(2, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, false);
                }
                const EffectSlot* effect = draw.effect ? m_effects.get(draw.effect) : nullptr;
                effect = effect && effect->kind == EffectKind::Surface ? effect : nullptr;
                if (effect)
                {
                    static const float none[16] = {};
                    setEffectValues(draw.values ? reinterpret_cast<const float(&)[16]>(*draw.values) : none, m_view.time, {}, true);
                    m_device->SetVertexShader(effect->vertex);
                    m_device->SetPixelShader(effect->pixel);
                }
                m_device->SetStreamSource(0, m->vertices[m->current], 0, sizeof(MeshVertex));
                m_device->SetIndices(m->indices);
                m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m->vertexCount, draw.first, count / 3);
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                if (effect)
                {
                    m_device->SetVertexShader(m_meshVs);
                    m_device->SetPixelShader(m_meshPs);
                }
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

            // A cube of pixels in the GPU's own order. They repeat.
            Texture createVolume(const u8* pixels, u32 size) override
            {
                if (pixels == nullptr || size < 2 || size > 128 || (size & (size - 1)) != 0)
                {
                    return {};
                }
                IDirect3DVolumeTexture9* volume = nullptr;
                D3DLOCKED_BOX box;
                if (FAILED(m_device->CreateVolumeTexture(size, size, size, 1, 0, kRgbaTiled, D3DPOOL_DEFAULT, &volume, nullptr)))
                {
                    return {};
                }
                if (FAILED(volume->LockBox(0, &box, nullptr, 0)))
                {
                    release(volume);
                    return {};
                }
                XGTileVolume(box.pBits, size, size, size, nullptr, pixels, size * 4, size * size * 4, nullptr, 4);
                volume->UnlockBox(0);
                TextureSlot t = {};
                t.volume = volume;
                t.info.width = t.info.height = size;
                t.info.bytes = static_cast<size_t>(size) * size * size * 4;
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    release(volume);
                    return {};
                }
                m_textureBytes += t.info.bytes;
                return handle;
            }

            // --- Pictures to draw into ---

            // A texture to copy into, and a place in the GPU's memory to draw it: the screen's own, which is copied out first
            // and drawn back after, so it may be as large as the screen.
            Texture createTarget(u32 width, u32 height) override
            {
                if (width == 0 || height == 0 || width > kWidth * 2 || height > kHeight * 2 || tilesOf(width, height) * 2 > kScreenTiles)
                {
                    return {};
                }
                TextureSlot t = {};
                D3DSURFACE_PARAMETERS color = {};
                color.Base = 0;
                D3DSURFACE_PARAMETERS depth = {};
                depth.Base = tilesOf(width, height);
                if (FAILED(m_device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t.texture, nullptr)) ||
                    FAILED(m_device->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &t.color, &color)) ||
                    FAILED(m_device->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &t.depth, &depth)))
                {
                    release(t.texture);
                    release(t.color);
                    release(t.depth);
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
                t.info.bytes = static_cast<size_t>(width) * height * 4;
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    release(t.texture);
                    release(t.color);
                    release(t.depth);
                    return {};
                }
                m_textureBytes += t.info.bytes;
                return handle;
            }

            bool beginTarget(Texture handle, const Color& clear) override
            {
                const TextureSlot* t = m_textures.get(handle);
                if (!m_inFrame || m_drawing || m_shadowing || t == nullptr || t->color == nullptr || !makePost())
                {
                    return false;
                }
                // The screen, out of the way.
                copyScreen();
                m_device->GetRenderTarget(0, &m_screenColor);
                m_device->GetDepthStencilSurface(&m_screenDepth);
                for (DWORD i = 0; i < 4; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                m_device->SetRenderTarget(0, t->color);
                m_device->SetDepthStencilSurface(t->depth);
                m_drawing = handle;
                m_width = t->info.width;
                m_height = t->info.height;
                m_mode = Mode::None;
                wholeViewport();
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_COLORVALUE(clear.r, clear.g, clear.b, clear.a), 1.0f, 0);
                return true;
            }

            void endTarget() override
            {
                if (!m_drawing)
                {
                    return;
                }
                if (const TextureSlot* t = m_textures.get(m_drawing))
                {
                    m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, t->texture, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
                }
                m_drawing = {};
                m_width = kWidth;
                m_height = kHeight;
                m_device->SetRenderTarget(0, m_screenColor);
                m_device->SetDepthStencilSurface(m_screenDepth);
                release(m_screenColor);
                release(m_screenDepth);
                // And the screen back where it was.
                postState({});
                m_device->SetTexture(0, m_screenCopy);
                setSampler(0, D3DTEXF_POINT, D3DTADDRESS_CLAMP, false);
                m_device->SetPixelShader(m_postPs[Post::Copy]);
                cover();
                endPost();
                m_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
            }

            // --- Over the picture: what has been drawn is copied out, and drawn back through a shader ---

            bool grade(Texture table, float strength, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                const TextureSlot* t = m_textures.get(table);
                if (!m_inFrame || t == nullptr || t->volume == nullptr || !makePost())
                {
                    return false;
                }
                copyScreen();
                postState(area);
                m_device->SetTexture(0, m_screenCopy);
                setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                m_device->SetTexture(2, t->volume);
                setSampler(2, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                const float values[4] = { strength, static_cast<float>(t->info.width), 0, 0 };
                m_device->SetPixelShaderConstantF(0, values, 1);
                m_device->SetPixelShader(m_postPs[Post::Grade]);
                cover();
                endPost();
                return true;
            }

            bool bloom(const Bloom& b, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                if (!m_inFrame || b.strength <= 0 || !makePost())
                {
                    return false;
                }
                copyScreen();
                IDirect3DSurface9* screen = nullptr;
                IDirect3DSurface9* depth = nullptr;
                m_device->GetRenderTarget(0, &screen);
                m_device->GetDepthStencilSurface(&depth);
                m_device->SetRenderTarget(0, m_small);
                m_device->SetDepthStencilSurface(nullptr);
                postState({});
                const D3DVIEWPORT9 small = { 0, 0, kBloomWidth, kBloomHeight, 0, 1 };
                m_device->SetViewport(&small);
                // What is bright, smaller.
                const float bright[4] = { 1.0f / kWidth, 1.0f / kHeight, b.threshold, 0 };
                pass(Post::Bright, m_screenCopy, bright, m_bloom[0]);
                // Spread along, then down; twice, wider the second time.
                for (u32 round = 0; round < 2; round++)
                {
                    const float wide = b.spread * (round == 0 ? 1.0f : 2.5f);
                    const float along[4] = { wide / kBloomWidth, 0, 0, 0 };
                    const float down[4] = { 0, wide / kBloomHeight, 0, 0 };
                    pass(Post::Blur, m_bloom[0], along, m_bloom[1]);
                    pass(Post::Blur, m_bloom[1], down, m_bloom[0]);
                }
                m_device->SetRenderTarget(0, screen);
                m_device->SetDepthStencilSurface(depth);
                release(screen);
                release(depth);
                postState(area);
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
                m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                m_device->SetTexture(0, m_bloom[0]);
                setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                const float much[4] = { b.strength, 0, 0, 0 };
                m_device->SetPixelShaderConstantF(0, much, 1);
                m_device->SetPixelShader(m_postPs[Post::Add]);
                cover();
                endPost();
                return true;
            }

            Effect createEffect(EffectKind kind, const char* source, char* error, size_t errorSize) override
            {
                using namespace shaders360;
                if (errorSize)
                {
                    error[0] = 0;
                }
                EffectSlot slot = {};
                slot.kind = kind;
                bool fine;
                if (kind == EffectKind::Picture)
                {
                    const std::string text = std::string(kEffectWords) + kPictureStart + "#line 1\n" + source + kPictureMain;
                    fine = vertexFrom(text, "effectVs", slot.vertex, error, errorSize) && pixelFrom(text, "effectPs", slot.pixel, error, errorSize);
                }
                else
                {
                    const std::string start = std::string(kMeshCommon) + kEffectWords;
                    const std::string pixel = start + kSurfacePixelStart + "#line 1\n" + source + kSurfacePixelMain;
                    const std::string vertex = start + kSurfaceVertexStart + "#line 1\n" + source + kSurfaceVertexMain;
                    fine = vertexFrom(vertex, "effectVs", slot.vertex, error, errorSize) && pixelFrom(pixel, "effectPs", slot.pixel, error, errorSize);
                }
                const Effect effect = fine ? m_effects.add(slot) : Effect();
                if (!effect)
                {
                    release(slot.vertex);
                    release(slot.pixel);
                }
                return effect;
            }

            void destroyEffect(Effect handle) override
            {
                if (EffectSlot* e = m_effects.get(handle))
                {
                    release(e->vertex);
                    release(e->pixel);
                    m_effects.remove(handle);
                }
            }

            bool applyEffect(Effect handle, const float (&values)[16], float time, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                const EffectSlot* e = m_effects.get(handle);
                if (!m_inFrame || e == nullptr || e->kind != EffectKind::Picture || !makePost())
                {
                    return false;
                }
                copyScreen();
                postState({});
                if (area.enabled)
                {
                    const D3DVIEWPORT9 viewport = { static_cast<DWORD>(area.x), static_cast<DWORD>(area.y), static_cast<DWORD>(area.width), static_cast<DWORD>(area.height), 0, 1 };
                    m_device->SetViewport(&viewport);
                }
                setEffectValues(values, time, area, false);
                m_device->SetTexture(0, m_screenCopy);
                setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                seeDepth(nullptr);
                m_device->SetVertexShader(e->vertex);
                m_device->SetPixelShader(e->pixel);
                cover();
                unseeDepth();
                endPost();
                return true;
            }

            // Smaller, spread along and down as far as it is to be blurred, and back over it.
            bool blur(float pixels, const Scissor& area) override
            {
                if (m_drawing)
                {
                    return false;   // for the screen only
                }
                if (!m_inFrame || pixels <= 0 || !makePost())
                {
                    return false;
                }
                copyScreen();
                IDirect3DSurface9* screen = nullptr;
                IDirect3DSurface9* depth = nullptr;
                m_device->GetRenderTarget(0, &screen);
                m_device->GetDepthStencilSurface(&depth);
                m_device->SetRenderTarget(0, m_small);
                m_device->SetDepthStencilSurface(nullptr);
                postState({});
                const D3DVIEWPORT9 small = { 0, 0, kBloomWidth, kBloomHeight, 0, 1 };
                m_device->SetViewport(&small);
                // All of it, not only what is bright.
                const float all[4] = { 1.0f / kWidth, 1.0f / kHeight, -1, 0 };
                pass(Post::Bright, m_screenCopy, all, m_bloom[0]);
                // A pixel there is four here; nine looks reach four pixels each way.
                const float reach = pixels * static_cast<float>(kBloomHeight) / 720.0f;
                const u32 rounds = reach > 8 ? 2 : 1;
                const float wide = fmaxf(0.5f, reach / (8.0f * static_cast<float>(rounds)));
                for (u32 round = 0; round < rounds; round++)
                {
                    const float along[4] = { wide / kBloomWidth, 0, 0, 0 };
                    const float down[4] = { 0, wide / kBloomHeight, 0, 0 };
                    pass(Post::Blur, m_bloom[0], along, m_bloom[1]);
                    pass(Post::Blur, m_bloom[1], down, m_bloom[0]);
                }
                m_device->SetRenderTarget(0, screen);
                m_device->SetDepthStencilSurface(depth);
                release(screen);
                release(depth);
                postState(area);
                m_device->SetTexture(0, m_bloom[0]);
                setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                m_device->SetPixelShader(m_postPs[Post::Copy]);
                cover();
                endPost();
                return true;
            }

            bool seesDepth() const override { return true; }

            bool spaceLook(const SpaceLook& look, const Scissor& area) override
            {
                if (m_drawing || !m_inFrame || !m_viewDrawn || !makePost() || !makeSpace())
                {
                    return false;
                }
                const D3DVIEWPORT9 view = area.enabled ? D3DVIEWPORT9{ static_cast<DWORD>(area.x), static_cast<DWORD>(area.y), static_cast<DWORD>(area.width), static_cast<DWORD>(area.height), 0, 1 }
                                                       : D3DVIEWPORT9{ 0, 0, kWidth, kHeight, 0, 1 };
                // Over the view, from the copy of it, how far away each pixel is, and perhaps a picture more.
                const auto run = [&](IDirect3DPixelShader9* shader, const float (&values)[16], IDirect3DTexture9* extra, bool copy)
                {
                    if (copy)
                    {
                        copyScreen();
                    }
                    postState({});
                    m_device->SetViewport(&view);
                    setEffectValues(values, 0, area, false);
                    m_device->SetTexture(0, m_screenCopy);
                    setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                    seeDepth(extra);
                    m_device->SetPixelShader(shader);
                    cover();
                    unseeDepth();
                    endPost();
                };
                const float scale = static_cast<float>(kHeight) / 720.0f;
                if (look.occlusion > 0 && spaceShader(0) && spaceShader(1))
                {
                    // Into the screen, kept, and then the screen as it was times that.
                    const float values[16] = { look.occlusion, look.occlusionRadius };
                    copyScreen();
                    run(m_spacePs[0], values, nullptr, false);
                    m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, m_work, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
                    run(m_spacePs[1], values, m_work, false);
                }
                if (look.reflections > 0 && spaceShader(2))
                {
                    const float values[16] = { look.reflections };
                    run(m_spacePs[2], values, nullptr, true);
                }
                if (look.focus > 0 && look.focusBlur > 0 && spaceShader(3))
                {
                    const float values[16] = { look.focus, look.focusDepth, look.focusFalloff, look.focusBlur * scale, look.bokeh };
                    run(m_spacePs[3], values, nullptr, true);
                }
                if (look.exposure > 0 && spaceShader(4) && spaceShader(5))
                {
                    // How bright it is, a little of the way from last frame's, in the small place.
                    copyScreen();
                    IDirect3DSurface9* screen = nullptr;
                    IDirect3DSurface9* depth = nullptr;
                    m_device->GetRenderTarget(0, &screen);
                    m_device->GetDepthStencilSurface(&depth);
                    m_device->SetRenderTarget(0, m_small);
                    m_device->SetDepthStencilSurface(nullptr);
                    postState({});
                    const D3DVIEWPORT9 small = { 0, 0, kBloomWidth, kBloomHeight, 0, 1 };
                    m_device->SetViewport(&small);
                    const float measure[16] = { m_metered ? look.exposureAdapt : 1.0f };
                    setEffectValues(measure, 0, area, false);
                    m_device->SetTexture(0, m_screenCopy);
                    setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                    // Last frame's; no depth here, where there is no depth to copy.
                    m_device->SetTexture(4, m_meter[m_meterAt]);
                    setSampler(4, D3DTEXF_POINT, D3DTADDRESS_CLAMP, false);
                    m_device->SetPixelShader(m_spacePs[4]);
                    cover();
                    unseeDepth();
                    m_meterAt = 1 - m_meterAt;
                    m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, m_meter[m_meterAt], nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
                    m_device->SetRenderTarget(0, screen);
                    m_device->SetDepthStencilSurface(depth);
                    release(screen);
                    release(depth);
                    endPost();
                    m_metered = true;
                    const float values[16] = { 0, look.exposure, look.exposureRange > 1 ? look.exposureRange : 1 };
                    run(m_spacePs[5], values, m_meter[m_meterAt], false);
                }
                else
                {
                    m_metered = false;
                }
                return true;
            }

            // The shaders, how far away each pixel is (out of the GPU's own memory), a picture for occlusion, and two of
            // how bright it is. Made when first wanted.
            bool makeSpace()
            {
                if (m_spaceTried)
                {
                    return m_spaceFine;
                }
                m_spaceTried = true;
                m_spaceFine = SUCCEEDED(m_device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_D24S8, D3DPOOL_DEFAULT, &m_depthCopy, nullptr)) &&
                              SUCCEEDED(m_device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_work, nullptr)) &&
                              SUCCEEDED(m_device->CreateTexture(kBloomWidth, kBloomHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_meter[0], nullptr)) &&
                              SUCCEEDED(m_device->CreateTexture(kBloomWidth, kBloomHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_meter[1], nullptr));
                if (!m_spaceFine)
                {
                    NX_WARN("d3d9: no effects that see how far away things are");
                }
                return m_spaceFine;
            }

            // One of the shaders, made the first time it is wanted: compiling them all would hold up the first frame.
            IDirect3DPixelShader9* spaceShader(u32 which)
            {
                if (m_spacePs[which] == nullptr && !m_spaceFailed[which])
                {
                    using namespace shaders360;
                    static const char* const kEntries[] = { "occlusionPs", "occludePs", "reflectPs", "focusPs", "meterKeepPs", "exposePs" };
                    const std::string text = std::string(kEffectWords) + kSpace;
                    char error[512];
                    m_spaceFailed[which] = !pixelFrom(text, kEntries[which], m_spacePs[which], error, sizeof(error));
                    if (m_spaceFailed[which])
                    {
                        NX_ERROR("d3d9: %s: %s", kEntries[which], error);
                    }
                }
                return m_spacePs[which];
            }

            // How far away each pixel of the screen is, copied out where effects can read it, and a picture more.
            void seeDepth(IDirect3DTexture9* extra)
            {
                IDirect3DTexture9* depth = nullptr;
                if (m_viewDrawn && (m_depthCopy || makeSpace()) && m_depthCopy)
                {
                    m_device->Resolve(D3DRESOLVE_DEPTHSTENCIL, nullptr, m_depthCopy, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
                    depth = m_depthCopy;
                }
                m_device->SetTexture(3, depth);
                setSampler(3, D3DTEXF_POINT, D3DTADDRESS_CLAMP, false);
                m_device->SetTexture(4, extra);
                setSampler(4, D3DTEXF_POINT, D3DTADDRESS_CLAMP, false);
            }

            void unseeDepth()
            {
                m_device->SetTexture(3, nullptr);
                m_device->SetTexture(4, nullptr);
            }

            // What effects are told of a view: see EffectUniforms.
            static void sceneOf(const View& view, float (&out)[12])
            {
                const Mat4& p = view.projection;
                out[0] = p.m[0];
                out[1] = p.m[5];
                out[2] = p.m[10];
                out[3] = p.m[14];
                const Vec3 up = view.view.transformDirection({ 0, 1, 0 });
                out[4] = up.x;
                out[5] = up.y;
                out[6] = -up.z;
                out[7] = p.m[10] + 1 != 0 ? p.m[14] / (1 + p.m[10]) : 1e6f;
                const Vec3 sun = view.view.transformDirection(view.sunDirection);
                const float ahead = -sun.z;
                out[8] = ahead > 1e-4f ? 0.5f + 0.5f * sun.x * p.m[0] / ahead : 0.5f;
                out[9] = ahead > 1e-4f ? 0.5f - 0.5f * sun.y * p.m[5] / ahead : 0.5f;
                out[10] = ahead > 1e-4f ? 1.0f : 0.0f;
                out[11] = 0;
            }

            // What the sun sees, as how far it is: into the GPU's memory after the screen, then copied out.
            bool beginShadows(const Mat4& sunViewProjection) override
            {
                NX_ASSERT(m_inFrame && !m_shadowing);
                if (m_shadowColor == nullptr)
                {
                    return false;
                }
                m_shadowing = true;
                m_mode = Mode::None;
                m_device->SetTexture(1, nullptr);
                m_device->GetRenderTarget(0, &m_savedColor);
                m_device->GetDepthStencilSurface(&m_savedDepth);
                m_device->SetRenderTarget(0, m_shadowColor);
                m_device->SetDepthStencilSurface(m_shadowDepth);
                const D3DVIEWPORT9 viewport = { 0, 0, kShadowSide, kShadowSide, 0, 1 };
                m_device->SetViewport(&viewport);
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF, 1.0f, 0);
                ViewUniforms u;
                memset(static_cast<void*>(&u), 0, sizeof(u));
                u.viewProjection = sunViewProjection;
                m_device->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(&u), 4);
                m_device->SetVertexDeclaration(m_meshDecl);
                m_device->SetVertexShader(m_shadowVs);
                m_device->SetPixelShader(m_shadowPs);
                setBlend(Blend::None);
                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
                m_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                return true;
            }

            void endShadows() override
            {
                NX_ASSERT(m_shadowing);
                m_shadowing = false;
                m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, m_shadowTexture, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
                m_device->SetRenderTarget(0, m_savedColor);
                m_device->SetDepthStencilSurface(m_savedDepth);
                release(m_savedColor);
                release(m_savedDepth);
                m_shadowReady = true;
                m_mode = Mode::None;
            }

            u32 shadowSize() const override { return kShadowSide; }

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
                D3DVIEWPORT9 viewport = { 0, 0, m_width, m_height, 0, 1 };
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
                const D3DVIEWPORT9 viewport = { 0, 0, m_width, m_height, 0, 1 };
                m_device->SetViewport(&viewport);
            }

            void stencil(Stencil mode, u8 ref)
            {
                if (mode == Stencil::None && !m_stencilOn)
                {
                    return;
                }
                m_stencilOn = mode != Stencil::None;
                m_device->SetRenderState(D3DRS_STENCILENABLE, m_stencilOn ? TRUE : FALSE);
                m_device->SetRenderState(D3DRS_COLORWRITEENABLE, mode == Stencil::Write ? 0 : 0xF);
                if (m_stencilOn)
                {
                    const bool write = mode == Stencil::Write;
                    m_device->SetRenderState(D3DRS_STENCILFUNC, write ? D3DCMP_ALWAYS : mode == Stencil::Inside ? D3DCMP_EQUAL : D3DCMP_NOTEQUAL);
                    m_device->SetRenderState(D3DRS_STENCILREF, ref);
                    m_device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
                    m_device->SetRenderState(D3DRS_STENCILWRITEMASK, write ? 0xFF : 0);
                    m_device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
                    m_device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
                    m_device->SetRenderState(D3DRS_STENCILPASS, write ? D3DSTENCILOP_REPLACE : D3DSTENCILOP_KEEP);
                }
                // Marking a mask: only where it is more opaque than not, so text and pictures mark their shapes.
                m_device->SetRenderState(D3DRS_ALPHATESTENABLE, mode == Stencil::Write ? TRUE : FALSE);
                if (mode == Stencil::Write)
                {
                    m_device->SetRenderState(D3DRS_ALPHAREF, 128);
                    m_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
                }
            }
            bool m_stencilOn = false;

            void setBlend(Blend blend)
            {
                m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, blend != Blend::None ? TRUE : FALSE);
                if (blend != Blend::None)
                {
                    m_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
                    const bool multiply = blend == Blend::Multiply;
                    m_device->SetRenderState(D3DRS_SRCBLEND, multiply ? D3DBLEND_DESTCOLOR : D3DBLEND_SRCALPHA);
                    m_device->SetRenderState(D3DRS_DESTBLEND, blend == Blend::Additive ? D3DBLEND_ONE : multiply ? D3DBLEND_ZERO : D3DBLEND_INVSRCALPHA);
                    m_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
                    m_device->SetRenderState(D3DRS_SRCBLENDALPHA, multiply ? D3DBLEND_ZERO : D3DBLEND_ONE);
                    m_device->SetRenderState(D3DRS_DESTBLENDALPHA, multiply ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
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

            // --- Over the picture ---

            enum Post { Bright, Blur, Add, Grade, Copy, PostCount };

            // A copy of what is on the screen to read, a smaller place to draw in the GPU's memory after the screen and
            // the shadows, and two pictures that size. Made when they are first wanted.
            bool makePost()
            {
                if (m_postTried)
                {
                    return m_screenCopy != nullptr;
                }
                m_postTried = true;
                D3DSURFACE_PARAMETERS at = {};
                at.Base = kScreenTiles;
                const bool fine = SUCCEEDED(m_device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_screenCopy, nullptr)) &&
                                  SUCCEEDED(m_device->CreateRenderTarget(kBloomWidth, kBloomHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_small, &at)) &&
                                  SUCCEEDED(m_device->CreateTexture(kBloomWidth, kBloomHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_bloom[0], nullptr)) &&
                                  SUCCEEDED(m_device->CreateTexture(kBloomWidth, kBloomHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_bloom[1], nullptr));
                if (!fine)
                {
                    NX_WARN("d3d9: nothing can be drawn over the picture");
                    release(m_screenCopy);
                }
                return m_screenCopy != nullptr;
            }

            void copyScreen() { m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, m_screenCopy, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr); }

            void postState(const Scissor& area)
            {
                m_mode = Mode::None;
                wholeViewport();
                setScissor(area);
                m_device->SetVertexDeclaration(m_screenDecl);
                m_device->SetVertexShader(m_postVs);
                setBlend(Blend::None);
                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
                m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            }

            // A triangle over all of the target.
            void cover()
            {
                static const float corners[6] = { -1, -1, 3, -1, -1, 3 };
                m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, corners, sizeof(float) * 2);
                m_current.draws++;
            }

            // One step of bloom, in the small place, kept in a picture.
            void pass(Post which, IDirect3DTexture9* from, const float (&values)[4], IDirect3DTexture9* to)
            {
                m_device->SetTexture(0, from);
                setSampler(0, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, false);
                m_device->SetPixelShaderConstantF(0, values, 1);
                m_device->SetPixelShader(m_postPs[which]);
                cover();
                m_device->SetTexture(0, nullptr);
                m_device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, to, nullptr, 0, 0, nullptr, 1.0f, 0, nullptr);
            }

            void endPost()
            {
                for (DWORD i = 0; i < 3; i++)
                {
                    m_device->SetTexture(i, nullptr);
                }
                m_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                m_mode = Mode::None;
            }

            // values[0] to [3], the area of the picture it covers, and its size and the time.
            void setEffectValues(const float (&values)[16], float time, const Scissor& area, bool vertex)
            {
                float u[36] = {};
                // How a view in space is seen: see EffectUniforms.
                memcpy(u + 24, m_scene, sizeof(m_scene));
                if (!m_viewDrawn)
                {
                    u[24] = 0;
                    u[31] = 1e6f;
                }
                memcpy(u, values, sizeof(values));
                u[18] = u[19] = 1;
                u[20] = static_cast<float>(kWidth);
                u[21] = static_cast<float>(kHeight);
                u[22] = time;
                if (area.enabled)
                {
                    u[16] = static_cast<float>(area.x) / kWidth;
                    u[17] = static_cast<float>(area.y) / kHeight;
                    u[18] = static_cast<float>(area.width) / kWidth;
                    u[19] = static_cast<float>(area.height) / kHeight;
                    u[20] = static_cast<float>(area.width);
                    u[21] = static_cast<float>(area.height);
                }
                m_device->SetPixelShaderConstantF(80, u, 9);
                if (vertex)
                {
                    m_device->SetVertexShaderConstantF(80, u, 9);
                }
            }

            // --- Shaders ---

            // Either kind, with what went wrong given back, for effects that others write.
            bool vertexFrom(const std::string& text, const char* entry, IDirect3DVertexShader9*& vertex, char* error, size_t errorSize)
            {
                ID3DXBuffer* code = nullptr;
                const bool fine = compile(text.c_str(), text.size(), entry, "vs_3_0", code, error, errorSize) && SUCCEEDED(m_device->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &vertex));
                release(code);
                return fine;
            }

            bool pixelFrom(const std::string& text, const char* entry, IDirect3DPixelShader9*& pixel, char* error, size_t errorSize)
            {
                ID3DXBuffer* code = nullptr;
                const bool fine = compile(text.c_str(), text.size(), entry, "ps_3_0", code, error, errorSize) && SUCCEEDED(m_device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &pixel));
                release(code);
                return fine;
            }

            bool compile(const char* source, size_t length, const char* entry, const char* profile, ID3DXBuffer*& code, char* error = nullptr, size_t errorSize = 0)
            {
                ID3DXBuffer* errors = nullptr;
                const HRESULT hr = D3DXCompileShader(source, static_cast<UINT>(length), nullptr, nullptr, entry, profile, 0, &code, &errors, nullptr);
                if (FAILED(hr))
                {
                    const char* why = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "can't be compiled";
                    if (errorSize)
                    {
                        snprintf(error, errorSize, "%s", why);
                    }
                    else
                    {
                        NX_ERROR("d3d9: %s: %s", entry, why);
                    }
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
                return fine && vertexShader(kPost, sizeof(kPost) - 1, "postVs", m_postVs) && pixelShader(kPost, sizeof(kPost) - 1, "brightPs", m_postPs[Post::Bright]) &&
                       pixelShader(kPost, sizeof(kPost) - 1, "blurPs", m_postPs[Post::Blur]) && pixelShader(kPost, sizeof(kPost) - 1, "addPs", m_postPs[Post::Add]) &&
                       pixelShader(kPost, sizeof(kPost) - 1, "gradePs", m_postPs[Post::Grade]) && pixelShader(kPost, sizeof(kPost) - 1, "copyPs", m_postPs[Post::Copy]);
            }

            bool createShadowMap()
            {
                D3DSURFACE_PARAMETERS color = {};
                color.Base = kScreenTiles;
                D3DSURFACE_PARAMETERS depth = {};
                depth.Base = kScreenTiles + tilesOf(kShadowSide, kShadowSide);
                depth.HierarchicalZBase = 0xFFFFFFFF;
                if (FAILED(m_device->CreateRenderTarget(kShadowSide, kShadowSide, D3DFMT_R32F, D3DMULTISAMPLE_NONE, 0, FALSE, &m_shadowColor, &color)) ||
                    FAILED(m_device->CreateDepthStencilSurface(kShadowSide, kShadowSide, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_shadowDepth, &depth)) ||
                    FAILED(m_device->CreateTexture(kShadowSide, kShadowSide, 1, 0, D3DFMT_R32F, D3DPOOL_DEFAULT, &m_shadowTexture, nullptr)))
                {
                    NX_WARN("d3d9: no shadows");
                    release(m_shadowColor);
                    release(m_shadowDepth);
                    release(m_shadowTexture);
                }
                return true;
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
            IDirect3DSurface9* m_shadowColor = nullptr;
            IDirect3DSurface9* m_shadowDepth = nullptr;
            IDirect3DTexture9* m_shadowTexture = nullptr;
            IDirect3DSurface9* m_savedColor = nullptr;
            IDirect3DSurface9* m_savedDepth = nullptr;
            bool m_shadowing = false, m_shadowReady = false;
            IDirect3DVertexShader9* m_postVs = nullptr;
            IDirect3DPixelShader9* m_postPs[PostCount] = {};
            IDirect3DTexture9* m_screenCopy = nullptr;
            // While a picture is drawn into: which, its size, and the screen's surfaces to go back to.
            Texture m_drawing;
            u32 m_width = kWidth, m_height = kHeight;
            IDirect3DSurface9* m_screenColor = nullptr;
            IDirect3DSurface9* m_screenDepth = nullptr;
            IDirect3DSurface9* m_small = nullptr;
            IDirect3DTexture9* m_bloom[2] = {};
            IDirect3DPixelShader9* m_spacePs[6] = {};
            IDirect3DTexture9* m_depthCopy = nullptr;
            IDirect3DTexture9* m_work = nullptr;
            IDirect3DTexture9* m_meter[2] = {};
            u32 m_meterAt = 0;
            bool m_spaceTried = false, m_spaceFine = false, m_metered = false, m_viewDrawn = false;
            bool m_spaceFailed[6] = {};
            float m_scene[12] = {};
            bool m_postTried = false;

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
