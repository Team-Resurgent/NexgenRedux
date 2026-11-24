#include "gpu/gpu.h"

namespace nx::gpu
{
    namespace
    {
        struct NullBuffer
        {
            u32 count;
        };

        struct NullPipeline
        {
            PipelineDesc desc;
        };

        struct NullTexture
        {
            TextureInfo info;
        };

        // Draws nothing but checks everything, so logic can be tested on any machine.
        class NullDevice final : public Device
        {
        public:
            NullDevice(u32 width, u32 height) : m_width(width), m_height(height)
            {
                m_ready = m_buffers.init(1024, MemTag::Buffer) && m_pipelines.init(64, MemTag::Buffer) && m_textures.init(1024, MemTag::Texture);
            }

            bool ready() const { return m_ready; }
            const char* name() const override { return "null"; }

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
                const Buffer buffer = m_buffers.add({ count });
                if (buffer)
                {
                    m_bufferBytes += count * sizeof(Vertex);
                }
                return buffer;
            }

            void destroyBuffer(Buffer buffer) override
            {
                if (const NullBuffer* b = m_buffers.get(buffer))
                {
                    m_bufferBytes -= b->count * sizeof(Vertex);
                    m_buffers.remove(buffer);
                }
            }

            Pipeline createPipeline(const PipelineDesc& desc) override { return m_pipelines.add({ desc }); }
            void destroyPipeline(Pipeline pipeline) override { m_pipelines.remove(pipeline); }

            Texture createTexture(const TextureData& data) override
            {
                if (!validTexture(data))
                {
                    return {};
                }
                TextureInfo info;
                info.width = data.width;
                info.height = data.height;
                info.uMax = static_cast<float>(data.width) / static_cast<float>(data.fullWidth);
                info.vMax = static_cast<float>(data.height) / static_cast<float>(data.fullHeight);
                info.bytes = pixelBytes(data.format, data.fullWidth, data.fullHeight);
                const Texture texture = m_textures.add({ info });
                if (texture)
                {
                    m_textureBytes += info.bytes;
                }
                return texture;
            }

            void destroyTexture(Texture texture) override
            {
                if (const NullTexture* t = m_textures.get(texture))
                {
                    m_textureBytes -= t->info.bytes;
                    m_textures.remove(texture);
                }
            }

            TextureInfo textureInfo(Texture texture) override
            {
                const NullTexture* t = m_textures.get(texture);
                return t ? t->info : TextureInfo();
            }

            bool beginFrame(const Color&) override
            {
                NX_ASSERT(!m_inFrame);
                m_inFrame = true;
                m_current = {};
                m_streamed = 0;
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
                m_streamed += count;
                return first;
            }

            void draw(const Draw& draw) override
            {
                NX_ASSERT(m_inFrame);
                const NullBuffer* buffer = m_buffers.get(draw.vertices);
                const u32 available = draw.vertices ? (buffer ? buffer->count : 0) : m_streamed;
                if ((draw.vertices && buffer == nullptr) || m_pipelines.get(draw.pipeline) == nullptr ||
                    (draw.texture && m_textures.get(draw.texture) == nullptr) || draw.first > available || draw.count > available - draw.first)
                {
                    m_current.rejected++;
                    return;
                }
                m_current.draws++;
                m_current.vertices += draw.count;
            }

            void endFrame() override
            {
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                m_last = m_current;
            }

            bool capture(u8*, size_t) override { return false; }

            Stats stats() const override
            {
                Stats s = m_last;
                s.bufferBytes = m_bufferBytes;
                s.textureBytes = m_textureBytes;
                return s;
            }

        private:
            HandlePool<NullBuffer, BufferTag> m_buffers;
            HandlePool<NullPipeline, PipelineTag> m_pipelines;
            HandlePool<NullTexture, TextureTag> m_textures;
            Stats m_current;
            Stats m_last;
            size_t m_bufferBytes = 0;
            size_t m_textureBytes = 0;
            u32 m_streamed = 0;
            u32 m_width;
            u32 m_height;
            bool m_inFrame = false;
            bool m_ready = false;
        };
    }

    Device* createNullDevice(u32 width, u32 height)
    {
        NullDevice* device = create<NullDevice>(MemTag::General, width, height);
        if (device && !device->ready())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
