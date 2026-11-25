#include "gpu/gpu.h"
#include "gpu/d3d11/shaders/basic_shaders.h"
#include "gpu/d3d11/shaders/mesh_shaders.h"
#include "gpu/d3d11/shaders/mesh_source.h"
#include "gpu/d3d11/shaders/post_shaders.h"
#include "gpu/effect_source.h"
#include "gpu/mesh_uniforms.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>

#if defined(NX_UWP)
#include "gpu/d3d11/uwp_window.h"
#endif

#include <stdio.h>
#include <string.h>
#include <string>

// Direct3D 11, for Windows and later Xbox One / Series. Frames are drawn to our own target and then
// copied to the window, so they can be captured and later scaled.
namespace nx::gpu
{
    namespace
    {
#if defined(NX_UWP)
        using NativeWindow = const UwpWindow*;
#else
        using NativeWindow = HWND;
#endif

        template <typename T>
        void release(T*& object)
        {
            if (object)
            {
                object->Release();
                object = nullptr;
            }
        }

        struct BufferSlot
        {
            ID3D11Buffer* buffer;
            u32 count;
        };

        struct PipelineSlot
        {
            ID3D11BlendState* blend;
            ID3D11DepthStencilState* depth;
            ID3D11RasterizerState* raster;
            Topology topology;
        };

        struct EffectSlot
        {
            EffectKind kind;
            ID3D11VertexShader* vertex;
            ID3D11PixelShader* pixel;
        };

        // Half the size each time.
        constexpr u32 kBlurs = 5;
        struct Blur
        {
            ID3D11Texture2D* texture;
            ID3D11RenderTargetView* target;
            ID3D11ShaderResourceView* view;
            u32 width, height;
        };

        struct MeshSlot
        {
            ID3D11Buffer* vertices;
            ID3D11Buffer* indices;
            u32 vertexCount, indexCount;
            bool dynamic;
        };

        struct TextureSlot
        {
            ID3D11Texture2D* texture;
            ID3D11ShaderResourceView* view;
            TextureInfo info;
        };

        void releasePipeline(PipelineSlot& p)
        {
            release(p.blend);
            release(p.depth);
            release(p.raster);
        }

        void releaseMesh(MeshSlot& m)
        {
            release(m.vertices);
            release(m.indices);
        }

        void releaseTexture(TextureSlot& t)
        {
            release(t.view);
            release(t.texture);
        }

        class D3D11Device final : public Device
        {
        public:
            explicit D3D11Device(NativeWindow window) : m_window(window) {}

            ~D3D11Device() override
            {
                m_buffers.forEach([](BufferSlot& b) { release(b.buffer); });
                m_pipelines.forEach([](PipelineSlot& p) { releasePipeline(p); });
                m_textures.forEach([](TextureSlot& t) { releaseTexture(t); });
                m_meshes.forEach([](MeshSlot& m) { releaseMesh(m); });
                m_effects.forEach([](EffectSlot& e)
                {
                    release(e.vertex);
                    release(e.pixel);
                });
                for (ID3D11BlendState*& state : m_meshBlend)
                {
                    release(state);
                }
                for (ID3D11DepthStencilState*& state : m_meshDepth)
                {
                    release(state);
                }
                for (ID3D11RasterizerState*& state : m_meshRaster)
                {
                    release(state);
                }
                release(m_skyDepth);
                release(m_shadowRaster);
                release(m_shadowView);
                release(m_shadowTarget);
                release(m_shadowMap);
                release(m_shadowSampler);
                release(m_wrap);
                release(m_viewUniforms);
                release(m_objectUniforms);
                release(m_fieldUniforms);
                release(m_meshLayout);
                release(m_effectUniforms);
                release(m_clipRaster);
                release(m_noDepth);
                release(m_blurPs);
                release(m_meshVs);
                release(m_meshPs);
                release(m_shadowVs);
                release(m_screenVs);
                release(m_skyPs);
                release(m_fieldPs);
                releaseTargets();
                release(m_linear);
                release(m_nearest);
                release(m_stream);
                release(m_uniforms);
                release(m_layout);
                release(m_vertexShader);
                release(m_pixelShader);
                release(m_swapchain);
                if (m_context)
                {
                    m_context->ClearState();
                    m_context->Flush();
                }
                release(m_context);
                release(m_device);
            }

            bool init()
            {
                if (!m_buffers.init(4096, MemTag::Buffer) || !m_pipelines.init(256, MemTag::Buffer) || !m_textures.init(4096, MemTag::Texture) || !m_meshes.init(4096, MemTag::Buffer) || !m_effects.init(128, MemTag::Buffer))
                {
                    return false;
                }
                if (!createDevice() || !createSwapchain() || !createShaders() || !createMeshState() || !resize())
                {
                    return false;
                }
                const u8 white[4] = { 255, 255, 255, 255 };
                m_white = createTexture(rgbaTexture(white, 1, 1));
                return static_cast<bool>(m_white);
            }

            const char* name() const override { return m_warp ? "d3d11/warp" : "d3d11"; }

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
                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = count * static_cast<UINT>(sizeof(Vertex));
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                const D3D11_SUBRESOURCE_DATA data = { vertices, 0, 0 };
                ID3D11Buffer* buffer = nullptr;
                if (!ok(m_device->CreateBuffer(&desc, &data, &buffer), "vertex buffer"))
                {
                    return {};
                }
                const Buffer handle = m_buffers.add({ buffer, count });
                if (!handle)
                {
                    release(buffer);
                    return {};
                }
                m_bufferBytes += desc.ByteWidth;
                return handle;
            }

            void destroyBuffer(Buffer handle) override
            {
                if (BufferSlot* b = m_buffers.get(handle))
                {
                    m_bufferBytes -= b->count * sizeof(Vertex);
                    release(b->buffer);
                    m_buffers.remove(handle);
                }
            }

            Pipeline createPipeline(const PipelineDesc& desc) override
            {
                PipelineSlot p = {};
                p.topology = desc.topology;

                D3D11_BLEND_DESC blend = {};
                D3D11_RENDER_TARGET_BLEND_DESC& rt = blend.RenderTarget[0];
                rt.BlendEnable = desc.blend != Blend::None;
                rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
                rt.DestBlend = desc.blend == Blend::Additive ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
                rt.BlendOp = D3D11_BLEND_OP_ADD;
                rt.SrcBlendAlpha = D3D11_BLEND_ONE;
                rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
                rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

                D3D11_DEPTH_STENCIL_DESC depth = {};
                // With the test off, D3D11 also stops writing depth, so writes keep it on and always pass.
                depth.DepthEnable = desc.depthTest || desc.depthWrite;
                depth.DepthWriteMask = desc.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                depth.DepthFunc = desc.depthTest ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_ALWAYS;

                D3D11_RASTERIZER_DESC raster = {};
                raster.FillMode = D3D11_FILL_SOLID;
                raster.CullMode = desc.cull == Cull::Back ? D3D11_CULL_BACK : D3D11_CULL_NONE;
                raster.FrontCounterClockwise = TRUE;
                raster.DepthClipEnable = TRUE;
                raster.ScissorEnable = TRUE;

                if (!ok(m_device->CreateBlendState(&blend, &p.blend), "blend state") ||
                    !ok(m_device->CreateDepthStencilState(&depth, &p.depth), "depth state") ||
                    !ok(m_device->CreateRasterizerState(&raster, &p.raster), "raster state"))
                {
                    releasePipeline(p);
                    return {};
                }
                const Pipeline handle = m_pipelines.add(p);
                if (!handle)
                {
                    releasePipeline(p);
                }
                return handle;
            }

            void destroyPipeline(Pipeline handle) override
            {
                if (PipelineSlot* p = m_pipelines.get(handle))
                {
                    releasePipeline(*p);
                    m_pipelines.remove(handle);
                }
            }

            Texture createTexture(const TextureData& source) override
            {
                if (!validTexture(source))
                {
                    return {};
                }
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = source.fullWidth;
                desc.Height = source.fullHeight;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = source.format == PixelFormat::Bc1 ? DXGI_FORMAT_BC1_UNORM : source.format == PixelFormat::Bc3 ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                const UINT pitch = source.format == PixelFormat::Rgba8 ? source.fullWidth * 4 : source.fullWidth / 4 * (source.format == PixelFormat::Bc1 ? 8 : 16);
                const D3D11_SUBRESOURCE_DATA data = { source.pixels, pitch, 0 };
                const bool mips = source.mips && source.format == PixelFormat::Rgba8;
                if (mips)
                {
                    desc.MipLevels = 0;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
                    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
                }
                TextureSlot t = {};
                t.info.width = source.width;
                t.info.height = source.height;
                t.info.uMax = static_cast<float>(source.width) / static_cast<float>(source.fullWidth);
                t.info.vMax = static_cast<float>(source.height) / static_cast<float>(source.fullHeight);
                t.info.bytes = pixelBytes(source.format, source.fullWidth, source.fullHeight);
                if (!ok(m_device->CreateTexture2D(&desc, mips ? nullptr : &data, &t.texture), "texture") ||
                    !ok(m_device->CreateShaderResourceView(t.texture, nullptr, &t.view), "texture view"))
                {
                    releaseTexture(t);
                    return {};
                }
                if (mips)
                {
                    m_context->UpdateSubresource(t.texture, 0, nullptr, source.pixels, pitch, 0);
                    m_context->GenerateMips(t.view);
                    t.info.bytes += t.info.bytes / 3;
                }
                const Texture handle = m_textures.add(t);
                if (!handle)
                {
                    releaseTexture(t);
                    return {};
                }
                m_textureBytes += t.info.bytes;
                return handle;
            }

            void destroyTexture(Texture handle) override
            {
                if (TextureSlot* t = m_textures.get(handle))
                {
                    m_textureBytes -= t->info.bytes;
                    releaseTexture(*t);  // Direct3D keeps it until the GPU has finished with it
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
                if (!resize())
                {
                    return false;
                }
                m_inFrame = true;
                m_current = {};
                m_bound = {};
                m_boundTexture = {};
                m_streamed = 0;
                m_mode = Mode::None;

                const float color[4] = { clear.r, clear.g, clear.b, clear.a };
                m_context->ClearRenderTargetView(m_colorView, color);
                m_context->ClearDepthStencilView(m_depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
                m_context->OMSetRenderTargets(1, &m_colorView, m_depthView);
                return true;
            }

            u32 stream(const Vertex* vertices, u32 count) override
            {
                NX_ASSERT(m_inFrame && vertices != nullptr);
                if (count > kStreamVertices - m_streamed)
                {
                    return kStreamFull;
                }
                D3D11_MAPPED_SUBRESOURCE mapped;
                const D3D11_MAP mode = m_streamed == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
                if (FAILED(m_context->Map(m_stream, 0, mode, 0, &mapped)))
                {
                    return kStreamFull;
                }
                const u32 first = m_streamed;
                memcpy(static_cast<Vertex*>(mapped.pData) + first, vertices, static_cast<size_t>(count) * sizeof(Vertex));
                m_context->Unmap(m_stream, 0);
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

                use(Mode::Flat);
                if (m_bound != draw.pipeline)
                {
                    m_context->OMSetBlendState(pipeline->blend, nullptr, 0xFFFFFFFF);
                    m_context->OMSetDepthStencilState(pipeline->depth, 0);
                    m_context->RSSetState(pipeline->raster);
                    m_context->IASetPrimitiveTopology(pipeline->topology == Topology::Lines ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    m_bound = draw.pipeline;
                }
                const Texture handle = draw.texture ? draw.texture : m_white;
                if (m_boundTexture != handle)
                {
                    m_context->PSSetShaderResources(0, 1, &texture->view);
                    m_boundTexture = handle;
                }
                m_context->PSSetSamplers(0, 1, draw.filter == Filter::Nearest ? &m_nearest : &m_linear);
                setScissor(draw.scissor);

                D3D11_MAPPED_SUBRESOURCE mapped;
                if (SUCCEEDED(m_context->Map(m_uniforms, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    memcpy(mapped.pData, draw.transform.m, sizeof(draw.transform.m));
                    m_context->Unmap(m_uniforms, 0);
                }
                ID3D11Buffer* source = buffer ? buffer->buffer : m_stream;
                const UINT stride = sizeof(Vertex);
                const UINT offset = 0;
                m_context->IASetVertexBuffers(0, 1, &source, &stride, &offset);
                m_context->Draw(draw.count, draw.first);

                m_current.draws++;
                m_current.vertices += draw.count;
            }

            // --- Meshes ---

            Mesh createMesh(const MeshVertex* vertices, u32 vertexCount, const u16* indices, u32 indexCount, bool dynamic) override
            {
                if (vertices == nullptr || indices == nullptr || vertexCount == 0 || vertexCount > 65536 || indexCount < 3)
                {
                    return {};
                }
                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = vertexCount * static_cast<UINT>(sizeof(MeshVertex));
                desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
                desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                D3D11_BUFFER_DESC indexDesc = {};
                indexDesc.ByteWidth = indexCount * static_cast<UINT>(sizeof(u16));
                indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
                indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                const D3D11_SUBRESOURCE_DATA vertexData = { vertices, 0, 0 };
                const D3D11_SUBRESOURCE_DATA indexData = { indices, 0, 0 };
                MeshSlot m = {};
                m.vertexCount = vertexCount;
                m.indexCount = indexCount;
                m.dynamic = dynamic;
                Mesh handle;
                if (ok(m_device->CreateBuffer(&desc, &vertexData, &m.vertices), "mesh vertices") && ok(m_device->CreateBuffer(&indexDesc, &indexData, &m.indices), "mesh indices"))
                {
                    handle = m_meshes.add(m);
                }
                if (!handle)
                {
                    releaseMesh(m);
                    return {};
                }
                m_bufferBytes += desc.ByteWidth + indexDesc.ByteWidth;
                return handle;
            }

            void updateMesh(Mesh handle, const MeshVertex* vertices, u32 count) override
            {
                MeshSlot* m = m_meshes.get(handle);
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (m && m->dynamic && count <= m->vertexCount && SUCCEEDED(m_context->Map(m->vertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    memcpy(mapped.pData, vertices, count * sizeof(MeshVertex));
                    m_context->Unmap(m->vertices, 0);
                }
            }

            void destroyMesh(Mesh handle) override
            {
                if (MeshSlot* m = m_meshes.get(handle))
                {
                    m_bufferBytes -= m->vertexCount * sizeof(MeshVertex) + m->indexCount * sizeof(u16);
                    releaseMesh(*m);
                    m_meshes.remove(handle);
                }
            }

            void beginView(const View& view) override
            {
                NX_ASSERT(m_inFrame && !m_shadowing);
                ViewUniforms u;
                fillView(u, view, false);
                upload(m_viewUniforms, &u, sizeof(u));
                m_viewTime = view.time;
                m_context->ClearDepthStencilView(m_depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
                m_area = view.area;
                m_mode = Mode::None;
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
                use(m_shadowing ? Mode::Shadow : Mode::Mesh);
                ObjectUniforms u;
                fillObject(u, draw);
                upload(m_objectUniforms, &u, sizeof(u));
                const EffectSlot* effect = draw.effect && !m_shadowing ? m_effects.get(draw.effect) : nullptr;
                effect = effect && effect->kind == EffectKind::Surface ? effect : nullptr;
                if (effect)
                {
                    const Scissor all;
                    setValues(draw.values, m_viewTime, all);
                    m_context->VSSetShader(effect->vertex, nullptr, 0);
                    m_context->PSSetShader(effect->pixel, nullptr, 0);
                    m_context->VSSetConstantBuffers(3, 1, &m_effectUniforms);
                }
                if (!m_shadowing)
                {
                    m_context->OMSetBlendState(m_meshBlend[static_cast<u32>(draw.blend)], nullptr, 0xFFFFFFFF);
                    m_context->OMSetDepthStencilState(m_meshDepth[draw.depthWrite], 0);
                    m_context->RSSetState(m_meshRaster[draw.lines ? 2 : static_cast<u32>(draw.cull)]);
                    m_context->PSSetSamplers(0, 1, draw.repeat ? &m_wrap : &m_linear);
                    if (m_boundTexture != handle)
                    {
                        m_context->PSSetShaderResources(0, 1, &texture->view);
                        m_boundTexture = handle;
                    }
                }
                const UINT stride = sizeof(MeshVertex);
                const UINT offset = 0;
                m_context->IASetVertexBuffers(0, 1, &m->vertices, &stride, &offset);
                m_context->IASetIndexBuffer(m->indices, DXGI_FORMAT_R16_UINT, 0);
                m_context->DrawIndexed(count, draw.first, 0);
                if (effect)
                {
                    m_context->VSSetShader(m_meshVs, nullptr, 0);
                    m_context->PSSetShader(m_meshPs, nullptr, 0);
                }
                m_current.draws++;
                m_current.triangles += count / 3;
            }

            bool drawSky() override
            {
                use(Mode::Screen);
                m_context->PSSetShader(m_skyPs, nullptr, 0);
                m_context->OMSetBlendState(m_meshBlend[0], nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(m_skyDepth, 0);
                m_context->Draw(3, 0);
                m_current.draws++;
                return true;
            }

            bool drawField(const Field& field) override
            {
                use(Mode::Screen);
                FieldUniforms u;
                fillField(u, field);
                upload(m_fieldUniforms, &u, sizeof(u));
                m_context->PSSetConstantBuffers(2, 1, &m_fieldUniforms);
                m_context->PSSetShader(m_fieldPs, nullptr, 0);
                m_context->OMSetBlendState(m_meshBlend[0], nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(m_meshDepth[1], 0);
                m_context->Draw(3, 0);
                m_current.draws++;
                return true;
            }

            bool beginShadows(const Mat4& sunViewProjection) override
            {
                NX_ASSERT(m_inFrame && !m_shadowing);
                m_shadowing = true;
                m_mode = Mode::None;
                ViewUniforms u;
                memset(static_cast<void*>(&u), 0, sizeof(u));
                u.viewProjection = sunViewProjection;
                upload(m_viewUniforms, &u, sizeof(u));
                // It can't be read while it is being drawn.
                ID3D11ShaderResourceView* none = nullptr;
                m_context->PSSetShaderResources(1, 1, &none);
                m_context->OMSetRenderTargets(0, nullptr, m_shadowTarget);
                m_context->ClearDepthStencilView(m_shadowTarget, D3D11_CLEAR_DEPTH, 1.0f, 0);
                return true;
            }

            void endShadows() override
            {
                NX_ASSERT(m_shadowing);
                m_shadowing = false;
                m_mode = Mode::None;
                m_context->OMSetRenderTargets(1, &m_colorView, m_depthView);
            }

            Effect createEffect(EffectKind kind, const char* source, char* error, size_t errorSize) override
            {
                using namespace shaders;
                EffectSlot slot = {};
                slot.kind = kind;
                std::string pixel, vertex;
                if (kind == EffectKind::Picture)
                {
                    pixel = std::string(kEffectHlslWords) +
                            "Texture2D image : register(t0);\nSamplerState imageSampler : register(s0);\n"
                            "float4 picture(float2 uv) { return image.SampleLevel(imageSampler, effectArea.xy + saturate(uv) * effectArea.zw, 0); }\n#line 1\n" +
                            source +
                            "\nfloat4 effectMain(float2 at : TEXCOORD0) : SV_Target { float2 uv = float2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5); return float4(effect(uv, picture(uv)).rgb, 1.0); }\n";
                }
                else
                {
                    const std::string start = std::string(kMeshSource) + kEffectHlslWords;
                    pixel = start + "float4 picture(float2 uv) { return image.Sample(imageSampler, uv); }\n#line 1\n" + source +
                            "\nfloat4 effectMain(MeshOut input) : SV_Target { float glow = surface.x; float4 base = paint(image.Sample(imageSampler, input.uv) * input.color, input.uv, input.place, normalize(input.normal), glow); "
                            "if (base.a < 0.004) discard; return finish(input, base, glow); }\n";
                    // Corners have no image to look at.
                    vertex = start + "float4 picture(float2 uv) { return float4(1.0, 1.0, 1.0, 1.0); }\n#line 1\n" + source +
                             "\nMeshOut effectMain(MeshIn input) {\n#ifdef MOVES\ninput.position = move(input.position, input.normal, input.uv);\n#endif\nreturn meshVs(input); }\n";
                }
                ID3DBlob* code = compile(pixel, "ps_4_0", error, errorSize);
                bool fine = code && ok(m_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &slot.pixel), "effect");
                release(code);
                if (fine && kind == EffectKind::Surface)
                {
                    code = compile(vertex, "vs_4_0", error, errorSize);
                    fine = code && ok(m_device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &slot.vertex), "effect");
                    release(code);
                }
                const Effect effect = fine ? m_effects.add(slot) : Effect();
                if (!effect)
                {
                    release(slot.pixel);
                    release(slot.vertex);
                }
                return effect;
            }

            void destroyEffect(Effect handle) override
            {
                if (EffectSlot* slot = m_effects.get(handle))
                {
                    release(slot->pixel);
                    release(slot->vertex);
                    m_effects.remove(handle);
                }
            }

            bool applyEffect(Effect handle, const float (&values)[16], float time, const Scissor& area) override
            {
                const EffectSlot* slot = m_effects.get(handle);
                if (!m_inFrame || slot == nullptr || slot->kind != EffectKind::Picture || !copyTargets())
                {
                    return false;
                }
                m_context->CopyResource(m_copy, m_color);
                setValues(values, time, area);
                pass(slot->pixel, m_copyView, m_colorView, area, m_width, m_height, false);
                m_context->OMSetRenderTargets(1, &m_colorView, m_depthView);
                return true;
            }

            bool bloom(const Bloom& bloom, const Scissor& area) override
            {
                if (!m_inFrame || bloom.strength <= 0 || !copyTargets())
                {
                    return false;
                }
                m_context->CopyResource(m_copy, m_color);
                const Scissor all;
                const auto step = [&](float what, u32 width, u32 height, float strength)
                {
                    float values[16] = { what, bloom.threshold, strength, 0, bloom.spread / static_cast<float>(width), bloom.spread / static_cast<float>(height) };
                    setValues(values, 0, all);
                };
                for (u32 i = 0; i < kBlurs; i++)
                {
                    step(i == 0 ? 0.0f : 1.0f, i == 0 ? m_width : m_blurs[i - 1].width, i == 0 ? m_height : m_blurs[i - 1].height, 1);
                    pass(m_blurPs, i == 0 ? m_copyView : m_blurs[i - 1].view, m_blurs[i].target, all, m_blurs[i].width, m_blurs[i].height, false);
                }
                for (u32 i = kBlurs - 1; i > 0; i--)
                {
                    step(2, m_blurs[i].width, m_blurs[i].height, 0.8f);
                    pass(m_blurPs, m_blurs[i].view, m_blurs[i - 1].target, all, m_blurs[i - 1].width, m_blurs[i - 1].height, true);
                }
                step(2, m_blurs[0].width, m_blurs[0].height, bloom.strength * 0.5f);
                pass(m_blurPs, m_blurs[0].view, m_colorView, all, m_width, m_height, true, area);
                m_context->OMSetRenderTargets(1, &m_colorView, m_depthView);
                return true;
            }

            void endFrame() override
            {
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                ID3D11Texture2D* backBuffer = nullptr;
                if (SUCCEEDED(m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
                {
                    m_context->CopyResource(backBuffer, m_color);
                    release(backBuffer);
                }
                m_swapchain->Present(m_wait ? 1 : 0, 0);
                m_last = m_current;
            }

            bool capture(u8* pixels, size_t bytes) override
            {
                const size_t row = static_cast<size_t>(m_width) * 4;
                if (pixels == nullptr || bytes < row * m_height || m_inFrame || m_color == nullptr)
                {
                    return false;
                }
                D3D11_TEXTURE2D_DESC desc;
                m_color->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                ID3D11Texture2D* staging = nullptr;
                if (!ok(m_device->CreateTexture2D(&desc, nullptr, &staging), "capture texture"))
                {
                    return false;
                }
                m_context->CopyResource(staging, m_color);
                D3D11_MAPPED_SUBRESOURCE mapped;
                const bool read = SUCCEEDED(m_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
                if (read)
                {
                    for (u32 y = 0; y < m_height; y++)
                    {
                        memcpy(pixels + y * row, static_cast<const u8*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, row);
                    }
                    m_context->Unmap(staging, 0);
                }
                release(staging);
                return read;
            }

            void setWait(bool wait) override { m_wait = wait; }

            Stats stats() const override
            {
                Stats s = m_last;
                s.bufferBytes = m_bufferBytes;
                s.textureBytes = m_textureBytes;
                return s;
            }

        private:
            static bool ok(HRESULT result, const char* what)
            {
                if (FAILED(result))
                {
                    NX_ERROR("d3d11: %s failed (0x%08lx)", what, static_cast<unsigned long>(result));
                }
                return SUCCEEDED(result);
            }

            enum class Mode : u8 { None, Flat, Mesh, Shadow, Screen };

            void upload(ID3D11Buffer* buffer, const void* data, size_t size)
            {
                D3D11_MAPPED_SUBRESOURCE mapped;
                if (SUCCEEDED(m_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    memcpy(mapped.pData, data, size);
                    m_context->Unmap(buffer, 0);
                }
            }

            // Whatever a kind of drawing needs that isn't set for each draw.
            ID3DBlob* compile(const std::string& source, const char* profile, char* error, size_t errorSize)
            {
                ID3DBlob* code = nullptr;
                ID3DBlob* why = nullptr;
                D3DCompile(source.data(), source.size(), "effect", nullptr, nullptr, "effectMain", profile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, 0, &code, &why);
                if (code == nullptr && errorSize)
                {
                    snprintf(error, errorSize, "%s", why ? static_cast<const char*>(why->GetBufferPointer()) : "it can't be compiled");
                }
                release(why);
                return code;
            }

            void setValues(const float* values, float time, const Scissor& area)
            {
                shaders::EffectUniforms u = {};
                if (values)
                {
                    memcpy(u.values, values, sizeof(u.values));
                }
                const float w = static_cast<float>(m_width), h = static_cast<float>(m_height);
                u.area[2] = u.area[3] = 1;
                u.info[0] = w;
                u.info[1] = h;
                u.info[2] = time;
                if (area.enabled)
                {
                    u.area[0] = static_cast<float>(area.x) / w;
                    u.area[1] = static_cast<float>(area.y) / h;
                    u.area[2] = static_cast<float>(area.width) / w;
                    u.area[3] = static_cast<float>(area.height) / h;
                    u.info[0] = static_cast<float>(area.width);
                    u.info[1] = static_cast<float>(area.height);
                }
                upload(m_effectUniforms, &u, sizeof(u));
                m_context->PSSetConstantBuffers(3, 1, &m_effectUniforms);
            }

            // Covers a target with a picture. Only part of it is changed, if there is a clip.
            void pass(ID3D11PixelShader* shader, ID3D11ShaderResourceView* from, ID3D11RenderTargetView* to, const Scissor& area, u32 width, u32 height, bool adds, const Scissor& clip = {})
            {
                m_mode = Mode::None;
                m_bound = {};
                m_boundTexture = {};
                ID3D11ShaderResourceView* none = nullptr;
                m_context->PSSetShaderResources(0, 1, &none);
                m_context->OMSetRenderTargets(1, &to, nullptr);
                D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1 };
                if (area.enabled)
                {
                    viewport = { static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width), static_cast<float>(area.height), 0, 1 };
                }
                m_context->RSSetViewports(1, &viewport);
                m_context->RSSetState(clip.enabled ? m_clipRaster : m_meshRaster[0]);
                if (clip.enabled)
                {
                    const D3D11_RECT rect = { clip.x, clip.y, clip.x + clip.width, clip.y + clip.height };
                    m_context->RSSetScissorRects(1, &rect);
                }
                m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_context->IASetInputLayout(nullptr);
                m_context->VSSetShader(m_screenVs, nullptr, 0);
                m_context->PSSetShader(shader, nullptr, 0);
                m_context->PSSetShaderResources(0, 1, &from);
                m_context->PSSetSamplers(0, 1, &m_linear);
                m_context->OMSetBlendState(m_meshBlend[adds ? 2 : 0], nullptr, 0xFFFFFFFF);
                m_context->OMSetDepthStencilState(m_noDepth, 0);
                m_context->Draw(3, 0);
                m_context->PSSetShaderResources(0, 1, &none);
                m_current.draws++;
            }

            // Room for what effects read from, made when the first is used.
            bool copyTargets()
            {
                if (m_copyView)
                {
                    return true;
                }
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = m_width;
                desc.Height = m_height;
                desc.MipLevels = desc.ArraySize = 1;
                desc.SampleDesc.Count = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                bool fine = ok(m_device->CreateTexture2D(&desc, nullptr, &m_copy), "effect picture") && ok(m_device->CreateShaderResourceView(m_copy, nullptr, &m_copyView), "effect picture");
                desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                for (Blur& b : m_blurs)
                {
                    desc.Width = desc.Width > 2 ? desc.Width / 2 : 1;
                    desc.Height = desc.Height > 2 ? desc.Height / 2 : 1;
                    b.width = desc.Width;
                    b.height = desc.Height;
                    fine = fine && ok(m_device->CreateTexture2D(&desc, nullptr, &b.texture), "blur") && ok(m_device->CreateRenderTargetView(b.texture, nullptr, &b.target), "blur") &&
                           ok(m_device->CreateShaderResourceView(b.texture, nullptr, &b.view), "blur");
                }
                return fine;
            }

            void use(Mode mode)
            {
                if (m_mode == mode)
                {
                    return;
                }
                m_mode = mode;
                m_bound = {};
                m_boundTexture = {};
                D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(m_width), static_cast<float>(m_height), 0, 1 };
                if (mode == Mode::Shadow)
                {
                    viewport.Width = viewport.Height = static_cast<float>(kShadowSize);
                }
                else if (mode != Mode::Flat && m_area.enabled)
                {
                    viewport = { static_cast<float>(m_area.x), static_cast<float>(m_area.y), static_cast<float>(m_area.width), static_cast<float>(m_area.height), 0, 1 };
                }
                m_context->RSSetViewports(1, &viewport);
                if (mode == Mode::Flat)
                {
                    m_context->IASetInputLayout(m_layout);
                    m_context->VSSetShader(m_vertexShader, nullptr, 0);
                    m_context->VSSetConstantBuffers(0, 1, &m_uniforms);
                    m_context->PSSetShader(m_pixelShader, nullptr, 0);
                    return;
                }
                ID3D11Buffer* blocks[2] = { m_viewUniforms, m_objectUniforms };
                m_context->VSSetConstantBuffers(0, 2, blocks);
                m_context->PSSetConstantBuffers(0, 2, blocks);
                m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_context->IASetInputLayout(mode == Mode::Screen ? nullptr : m_meshLayout);
                m_context->VSSetShader(mode == Mode::Shadow ? m_shadowVs : mode == Mode::Screen ? m_screenVs : m_meshVs, nullptr, 0);
                m_context->PSSetShader(mode == Mode::Mesh ? m_meshPs : nullptr, nullptr, 0);
                if (mode == Mode::Shadow)
                {
                    m_context->RSSetState(m_shadowRaster);
                    m_context->OMSetDepthStencilState(m_meshDepth[1], 0);
                    m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
                }
                else
                {
                    m_context->RSSetState(m_meshRaster[0]);
                    m_context->PSSetShaderResources(1, 1, &m_shadowView);
                    m_context->PSSetSamplers(1, 1, &m_shadowSampler);
                }
            }

            bool createMeshState()
            {
                using namespace shaders;
                const D3D11_INPUT_ELEMENT_DESC layout[] = {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, nx), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(MeshVertex, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(MeshVertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };
                bool fine = ok(m_device->CreateVertexShader(meshVs, sizeof(meshVs), nullptr, &m_meshVs), "mesh shader") &&
                            ok(m_device->CreatePixelShader(meshPs, sizeof(meshPs), nullptr, &m_meshPs), "mesh shader") &&
                            ok(m_device->CreateVertexShader(shadowVs, sizeof(shadowVs), nullptr, &m_shadowVs), "shadow shader") &&
                            ok(m_device->CreateVertexShader(screenVs, sizeof(screenVs), nullptr, &m_screenVs), "screen shader") &&
                            ok(m_device->CreatePixelShader(skyPs, sizeof(skyPs), nullptr, &m_skyPs), "sky shader") &&
                            ok(m_device->CreatePixelShader(fieldPs, sizeof(fieldPs), nullptr, &m_fieldPs), "field shader") &&
                            ok(m_device->CreateInputLayout(layout, ARRAYSIZE(layout), meshVs, sizeof(meshVs), &m_meshLayout), "mesh layout");
                const auto uniforms = [&](UINT size, ID3D11Buffer** buffer)
                {
                    D3D11_BUFFER_DESC desc = {};
                    desc.ByteWidth = size;
                    desc.Usage = D3D11_USAGE_DYNAMIC;
                    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                    fine = fine && ok(m_device->CreateBuffer(&desc, nullptr, buffer), "uniform buffer");
                };
                uniforms(sizeof(ViewUniforms), &m_viewUniforms);
                uniforms(sizeof(ObjectUniforms), &m_objectUniforms);
                uniforms(sizeof(FieldUniforms), &m_fieldUniforms);
                uniforms(sizeof(EffectUniforms), &m_effectUniforms);
                fine = fine && ok(m_device->CreatePixelShader(blurPs, sizeof(blurPs), nullptr, &m_blurPs), "blur shader");
                for (u32 i = 0; i < 3; i++)
                {
                    D3D11_BLEND_DESC desc = {};
                    D3D11_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[0];
                    rt.BlendEnable = i != 0;
                    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
                    rt.DestBlend = static_cast<Blend>(i) == Blend::Additive ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
                    rt.BlendOp = rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
                    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
                    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                    fine = fine && ok(m_device->CreateBlendState(&desc, &m_meshBlend[i]), "blend state");
                }
                D3D11_DEPTH_STENCIL_DESC depth = {};
                depth.DepthEnable = TRUE;
                depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
                for (u32 i = 0; i < 2; i++)
                {
                    depth.DepthWriteMask = i ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                    fine = fine && ok(m_device->CreateDepthStencilState(&depth, &m_meshDepth[i]), "depth state");
                }
                depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
                fine = fine && ok(m_device->CreateDepthStencilState(&depth, &m_skyDepth), "depth state");
                D3D11_DEPTH_STENCIL_DESC none = {};
                none.DepthFunc = D3D11_COMPARISON_ALWAYS;
                fine = fine && ok(m_device->CreateDepthStencilState(&none, &m_noDepth), "depth state");
                D3D11_RASTERIZER_DESC raster = {};
                raster.FillMode = D3D11_FILL_SOLID;
                raster.FrontCounterClockwise = TRUE;
                raster.DepthClipEnable = TRUE;
                for (u32 i = 0; i < 2; i++)
                {
                    raster.CullMode = static_cast<Cull>(i) == Cull::Back ? D3D11_CULL_BACK : D3D11_CULL_NONE;
                    fine = fine && ok(m_device->CreateRasterizerState(&raster, &m_meshRaster[i]), "raster state");
                }
                raster.CullMode = D3D11_CULL_NONE;
                raster.FillMode = D3D11_FILL_WIREFRAME;
                fine = fine && ok(m_device->CreateRasterizerState(&raster, &m_meshRaster[2]), "raster state");
                raster.FillMode = D3D11_FILL_SOLID;
                raster.ScissorEnable = TRUE;
                fine = fine && ok(m_device->CreateRasterizerState(&raster, &m_clipRaster), "raster state");
                raster.ScissorEnable = FALSE;
                // Pushed away a little, so that a surface doesn't shade itself.
                raster.DepthBias = 60;
                raster.SlopeScaledDepthBias = 2.0f;
                raster.DepthClipEnable = FALSE;
                fine = fine && ok(m_device->CreateRasterizerState(&raster, &m_shadowRaster), "raster state");

                D3D11_TEXTURE2D_DESC map = {};
                map.Width = map.Height = kShadowSize;
                map.MipLevels = map.ArraySize = 1;
                map.Format = DXGI_FORMAT_R32_TYPELESS;
                map.SampleDesc.Count = 1;
                map.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
                D3D11_DEPTH_STENCIL_VIEW_DESC target = {};
                target.Format = DXGI_FORMAT_D32_FLOAT;
                target.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
                view.Format = DXGI_FORMAT_R32_FLOAT;
                view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view.Texture2D.MipLevels = 1;
                D3D11_SAMPLER_DESC sampler = {};
                sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
                sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
                D3D11_SAMPLER_DESC wrap = {};
                wrap.Filter = D3D11_FILTER_ANISOTROPIC;
                wrap.MaxAnisotropy = 8;
                wrap.AddressU = wrap.AddressV = wrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
                wrap.ComparisonFunc = D3D11_COMPARISON_NEVER;
                wrap.MaxLOD = D3D11_FLOAT32_MAX;
                return fine && ok(m_device->CreateTexture2D(&map, nullptr, &m_shadowMap), "shadow map") &&
                       ok(m_device->CreateDepthStencilView(m_shadowMap, &target, &m_shadowTarget), "shadow target") &&
                       ok(m_device->CreateShaderResourceView(m_shadowMap, &view, &m_shadowView), "shadow view") &&
                       ok(m_device->CreateSamplerState(&sampler, &m_shadowSampler), "shadow sampler") &&
                       ok(m_device->CreateSamplerState(&wrap, &m_wrap), "sampler");
            }

            void setScissor(const Scissor& s)
            {
                D3D11_RECT rect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
                if (s.enabled)
                {
                    rect.left = s.x;
                    rect.top = s.y;
                    rect.right = s.x + (s.width > 0 ? s.width : 0);
                    rect.bottom = s.y + (s.height > 0 ? s.height : 0);
                }
                m_context->RSSetScissorRects(1, &rect);
            }

            bool createDevice()
            {
                const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
                HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
                if (FAILED(result))
                {
                    // Software rendering, for machines without a GPU such as build servers.
                    m_warp = true;
                    result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
                }
                return ok(result, "device");
            }

            bool createSwapchain()
            {
                IDXGIDevice* dxgiDevice = nullptr;
                IDXGIAdapter* adapter = nullptr;
                IDXGIFactory2* factory = nullptr;
                bool created = ok(m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)), "dxgi device") &&
                               ok(dxgiDevice->GetAdapter(&adapter), "adapter") &&
                               ok(adapter->GetParent(IID_PPV_ARGS(&factory)), "factory");
                if (created)
                {
                    DXGI_SWAP_CHAIN_DESC1 desc = {};
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.SampleDesc.Count = 1;
                    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    desc.BufferCount = 2;
                    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#if defined(NX_UWP)
                    created = ok(factory->CreateSwapChainForCoreWindow(m_device, m_window->window, &desc, nullptr, &m_swapchain), "swap chain");
#else
                    created = ok(factory->CreateSwapChainForHwnd(m_device, m_window, &desc, nullptr, nullptr, &m_swapchain), "swap chain");
                    factory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER);
#endif
                }
                release(factory);
                release(adapter);
                release(dxgiDevice);
                return created;
            }

            bool createShaders()
            {
                using namespace shaders;
                const D3D11_INPUT_ELEMENT_DESC layout[] = {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Vertex, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };
                D3D11_BUFFER_DESC uniforms = {};
                uniforms.ByteWidth = sizeof(Mat4);
                uniforms.Usage = D3D11_USAGE_DYNAMIC;
                uniforms.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                uniforms.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                D3D11_BUFFER_DESC stream = uniforms;
                stream.ByteWidth = kStreamVertices * static_cast<UINT>(sizeof(Vertex));
                stream.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                D3D11_SAMPLER_DESC sampler = {};
                sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
                sampler.MaxLOD = D3D11_FLOAT32_MAX;
                D3D11_SAMPLER_DESC nearest = sampler;
                nearest.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                return ok(m_device->CreateVertexShader(basicVs, sizeof(basicVs), nullptr, &m_vertexShader), "vertex shader") &&
                       ok(m_device->CreatePixelShader(basicPs, sizeof(basicPs), nullptr, &m_pixelShader), "pixel shader") &&
                       ok(m_device->CreateInputLayout(layout, ARRAYSIZE(layout), basicVs, sizeof(basicVs), &m_layout), "input layout") &&
                       ok(m_device->CreateBuffer(&uniforms, nullptr, &m_uniforms), "uniform buffer") &&
                       ok(m_device->CreateBuffer(&stream, nullptr, &m_stream), "stream buffer") &&
                       ok(m_device->CreateSamplerState(&sampler, &m_linear), "sampler") &&
                       ok(m_device->CreateSamplerState(&nearest, &m_nearest), "sampler");
            }

            void releaseTargets()
            {
                release(m_copyView);
                release(m_copy);
                for (Blur& b : m_blurs)
                {
                    release(b.view);
                    release(b.target);
                    release(b.texture);
                }
                release(m_colorView);
                release(m_depthView);
                release(m_color);
                release(m_depth);
            }

            void windowSize(u32& width, u32& height)
            {
#if defined(NX_UWP)
                m_window->size(m_window->user, width, height);
#else
                RECT rect = {};
                GetClientRect(m_window, &rect);
                width = static_cast<u32>(rect.right);
                height = static_cast<u32>(rect.bottom);
#endif
            }

            // False when the window has no area.
            bool resize()
            {
                u32 width = 0, height = 0;
                windowSize(width, height);
                if (width == 0 || height == 0)
                {
                    return false;
                }
                if (width == m_width && height == m_height && m_depthView)
                {
                    return true;
                }
                m_context->OMSetRenderTargets(0, nullptr, nullptr);
                releaseTargets();
                m_width = width;
                m_height = height;

                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.SampleDesc.Count = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.BindFlags = D3D11_BIND_RENDER_TARGET;
                D3D11_TEXTURE2D_DESC depth = desc;
                depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                return ok(m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "resize") &&
                       ok(m_device->CreateTexture2D(&desc, nullptr, &m_color), "colour target") &&
                       ok(m_device->CreateRenderTargetView(m_color, nullptr, &m_colorView), "colour view") &&
                       ok(m_device->CreateTexture2D(&depth, nullptr, &m_depth), "depth target") &&
                       ok(m_device->CreateDepthStencilView(m_depth, nullptr, &m_depthView), "depth view");
            }

            NativeWindow m_window;
            ID3D11Device* m_device = nullptr;
            ID3D11DeviceContext* m_context = nullptr;
            IDXGISwapChain1* m_swapchain = nullptr;
            ID3D11VertexShader* m_vertexShader = nullptr;
            ID3D11PixelShader* m_pixelShader = nullptr;
            ID3D11InputLayout* m_layout = nullptr;
            ID3D11Buffer* m_uniforms = nullptr;
            ID3D11Buffer* m_stream = nullptr;
            ID3D11SamplerState* m_linear = nullptr;
            ID3D11SamplerState* m_nearest = nullptr;
            ID3D11Texture2D* m_color = nullptr;
            ID3D11Texture2D* m_depth = nullptr;
            ID3D11RenderTargetView* m_colorView = nullptr;
            ID3D11DepthStencilView* m_depthView = nullptr;
            HandlePool<BufferSlot, BufferTag> m_buffers;
            HandlePool<PipelineSlot, PipelineTag> m_pipelines;
            HandlePool<TextureSlot, TextureTag> m_textures;
            HandlePool<MeshSlot, MeshTag> m_meshes;
            HandlePool<EffectSlot, EffectTag> m_effects;
            ID3D11Buffer* m_effectUniforms = nullptr;
            ID3D11PixelShader* m_blurPs = nullptr;
            ID3D11DepthStencilState* m_noDepth = nullptr;
            ID3D11RasterizerState* m_clipRaster = nullptr;
            ID3D11Texture2D* m_copy = nullptr;
            ID3D11ShaderResourceView* m_copyView = nullptr;
            Blur m_blurs[kBlurs] = {};
            float m_viewTime = 0;
            ID3D11VertexShader* m_meshVs = nullptr;
            ID3D11PixelShader* m_meshPs = nullptr;
            ID3D11VertexShader* m_shadowVs = nullptr;
            ID3D11VertexShader* m_screenVs = nullptr;
            ID3D11PixelShader* m_skyPs = nullptr;
            ID3D11PixelShader* m_fieldPs = nullptr;
            ID3D11InputLayout* m_meshLayout = nullptr;
            ID3D11Buffer* m_viewUniforms = nullptr;
            ID3D11Buffer* m_objectUniforms = nullptr;
            ID3D11Buffer* m_fieldUniforms = nullptr;
            ID3D11BlendState* m_meshBlend[3] = {};
            ID3D11DepthStencilState* m_meshDepth[2] = {};
            ID3D11DepthStencilState* m_skyDepth = nullptr;
            ID3D11RasterizerState* m_meshRaster[3] = {};  // the last, of lines
            ID3D11RasterizerState* m_shadowRaster = nullptr;
            ID3D11Texture2D* m_shadowMap = nullptr;
            ID3D11DepthStencilView* m_shadowTarget = nullptr;
            ID3D11ShaderResourceView* m_shadowView = nullptr;
            ID3D11SamplerState* m_shadowSampler = nullptr;
            ID3D11SamplerState* m_wrap = nullptr;
            Scissor m_area;
            Mode m_mode = Mode::None;
            bool m_shadowing = false;
            Texture m_white;
            Pipeline m_bound;
            Texture m_boundTexture;
            Stats m_current;
            Stats m_last;
            size_t m_bufferBytes = 0;
            size_t m_textureBytes = 0;
            u32 m_streamed = 0;
            u32 m_width = 0;
            u32 m_height = 0;
            bool m_wait = true;
            bool m_inFrame = false;
            bool m_warp = false;
        };
    }

    Device* createDevice(void* nativeWindow)
    {
        D3D11Device* device = create<D3D11Device>(MemTag::General, static_cast<NativeWindow>(nativeWindow));
        if (device && !device->init())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
