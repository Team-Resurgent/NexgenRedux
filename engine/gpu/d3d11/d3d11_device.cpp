#include "gpu/gpu.h"
#include "gpu/d3d11/shaders/basic_shaders.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>

#include <string.h>

// Direct3D 11, for Windows and later Xbox One / Series. Frames are drawn to our own target and then
// copied to the window, so they can be captured and later scaled.
namespace nx::gpu
{
    namespace
    {
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

        void releaseTexture(TextureSlot& t)
        {
            release(t.view);
            release(t.texture);
        }

        class D3D11Device final : public Device
        {
        public:
            explicit D3D11Device(HWND window) : m_window(window) {}

            ~D3D11Device() override
            {
                m_buffers.forEach([](BufferSlot& b) { release(b.buffer); });
                m_pipelines.forEach([](PipelineSlot& p) { releasePipeline(p); });
                m_textures.forEach([](TextureSlot& t) { releaseTexture(t); });
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
                if (!m_buffers.init(4096, MemTag::Buffer) || !m_pipelines.init(256, MemTag::Buffer) || !m_textures.init(4096, MemTag::Texture))
                {
                    return false;
                }
                if (!createDevice() || !createSwapchain() || !createShaders() || !resize())
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
                TextureSlot t = {};
                t.info.width = source.width;
                t.info.height = source.height;
                t.info.uMax = static_cast<float>(source.width) / static_cast<float>(source.fullWidth);
                t.info.vMax = static_cast<float>(source.height) / static_cast<float>(source.fullHeight);
                t.info.bytes = pixelBytes(source.format, source.fullWidth, source.fullHeight);
                if (!ok(m_device->CreateTexture2D(&desc, &data, &t.texture), "texture") ||
                    !ok(m_device->CreateShaderResourceView(t.texture, nullptr, &t.view), "texture view"))
                {
                    releaseTexture(t);
                    return {};
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

                const float color[4] = { clear.r, clear.g, clear.b, clear.a };
                m_context->ClearRenderTargetView(m_colorView, color);
                m_context->ClearDepthStencilView(m_depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
                m_context->OMSetRenderTargets(1, &m_colorView, m_depthView);
                const D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(m_width), static_cast<float>(m_height), 0, 1 };
                m_context->RSSetViewports(1, &viewport);
                m_context->IASetInputLayout(m_layout);
                m_context->VSSetShader(m_vertexShader, nullptr, 0);
                m_context->VSSetConstantBuffers(0, 1, &m_uniforms);
                m_context->PSSetShader(m_pixelShader, nullptr, 0);
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
                m_swapchain->Present(1, 0);
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
                    created = ok(factory->CreateSwapChainForHwnd(m_device, m_window, &desc, nullptr, nullptr, &m_swapchain), "swap chain");
                    factory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER);
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
                release(m_colorView);
                release(m_depthView);
                release(m_color);
                release(m_depth);
            }

            // False when the window has no area.
            bool resize()
            {
                RECT rect;
                if (!GetClientRect(m_window, &rect) || rect.right <= 0 || rect.bottom <= 0)
                {
                    return false;
                }
                const u32 width = static_cast<u32>(rect.right);
                const u32 height = static_cast<u32>(rect.bottom);
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

            HWND m_window;
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
            bool m_inFrame = false;
            bool m_warp = false;
        };
    }

    Device* createDevice(void* nativeWindow)
    {
        D3D11Device* device = create<D3D11Device>(MemTag::General, static_cast<HWND>(nativeWindow));
        if (device && !device->init())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
