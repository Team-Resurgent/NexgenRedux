#include "gpu/gl/gl_window.h"
#include "gpu/effect_source.h"
#include "gpu/gl/mesh_shaders.h"
#include "gpu/mesh_uniforms.h"
#include "gpu/gpu.h"
#include "base/log.h"

#include <stddef.h>
#include <string>

// OpenGL 3.3 core, for Linux and macOS. Only what is used is declared here and loaded at start-up, so
// there are no GL headers to find. Frames are drawn to our own target and then copied to the window.
namespace nx::gpu
{
    namespace
    {
        using GLenum = unsigned int;
        using GLuint = unsigned int;
        using GLint = int;
        using GLsizei = int;
        using GLbitfield = unsigned int;
        using GLboolean = unsigned char;
        using GLfloat = float;
        using GLsizeiptr = ptrdiff_t;
        using GLubyte = unsigned char;

        enum : GLenum
        {
            GL_ONE = 1, GL_LINES = 0x0001, GL_TRIANGLES = 0x0004, GL_LEQUAL = 0x0203, GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303,
            GL_ALWAYS = 0x0207, GL_BACK = 0x0405, GL_CCW = 0x0901, GL_CULL_FACE = 0x0B44, GL_FRONT_AND_BACK = 0x0408, GL_LINE = 0x1B01, GL_FILL = 0x1B02, GL_DEPTH_TEST = 0x0B71, GL_BLEND = 0x0BE2,
            GL_SCISSOR_TEST = 0x0C11, GL_UNPACK_ALIGNMENT = 0x0CF5, GL_PACK_ALIGNMENT = 0x0D05, GL_TEXTURE_2D = 0x0DE1, GL_UNSIGNED_BYTE = 0x1401,
            GL_FLOAT_ = 0x1406, GL_RGBA = 0x1908, GL_RGB = 0x1907, GL_R11F_G11F_B10F = 0x8C3A, GL_VERSION = 0x1F02, GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_TEXTURE_MAG_FILTER = 0x2800,
            GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803, GL_CLAMP_TO_EDGE = 0x812F,
            GL_TEXTURE_MAX_LEVEL = 0x813D, GL_RGBA8 = 0x8058, GL_TEXTURE0 = 0x84C0, GL_DEPTH24_STENCIL8 = 0x88F0, GL_ARRAY_BUFFER = 0x8892,
            GL_STREAM_DRAW = 0x88E0, GL_STATIC_DRAW = 0x88E4, GL_FRAGMENT_SHADER = 0x8B30, GL_VERTEX_SHADER = 0x8B31, GL_COMPILE_STATUS = 0x8B81,
            GL_LINK_STATUS = 0x8B82, GL_READ_FRAMEBUFFER = 0x8CA8, GL_DRAW_FRAMEBUFFER = 0x8CA9, GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
            GL_COLOR_ATTACHMENT0 = 0x8CE0, GL_FRAMEBUFFER = 0x8D40, GL_RENDERBUFFER = 0x8D41, GL_DEPTH_STENCIL_ATTACHMENT = 0x821A,
            GL_CULL_NONE = 0, GL_UNSIGNED_SHORT = 0x1403, GL_REPEAT = 0x2901, GL_LINEAR_MIPMAP_LINEAR = 0x2703, GL_POLYGON_OFFSET_FILL = 0x8037,
            GL_DEPTH_CLAMP = 0x864F, GL_TEXTURE1 = 0x84C1, GL_TEXTURE2 = 0x84C2, GL_TEXTURE_3D = 0x806F, GL_TEXTURE_WRAP_R = 0x8072, GL_DEPTH_COMPONENT = 0x1902, GL_DEPTH_COMPONENT32F = 0x8CAC, GL_DEPTH_ATTACHMENT = 0x8D00,
            GL_TEXTURE_COMPARE_MODE = 0x884C, GL_TEXTURE_COMPARE_FUNC = 0x884D, GL_COMPARE_REF_TO_TEXTURE = 0x884E, GL_ELEMENT_ARRAY_BUFFER = 0x8893,
            GL_UNIFORM_BUFFER = 0x8A11, GL_DYNAMIC_DRAW = 0x88E8,
            GL_BC1 = 0x83F0, GL_BC3 = 0x83F3  // EXT_texture_compression_s3tc, which every desktop driver has
        };
        enum : GLbitfield
        {
            GL_DEPTH_BUFFER_BIT = 0x0100,
            GL_COLOR_BUFFER_BIT = 0x4000
        };

        // Return type, name, arguments.
#define NX_GL_FUNCTIONS(F) \
        F(void, ActiveTexture, (GLenum)) \
        F(void, AttachShader, (GLuint, GLuint)) \
        F(void, BindBuffer, (GLenum, GLuint)) \
        F(void, BindBufferBase, (GLenum, GLuint, GLuint)) \
        F(void, BindFramebuffer, (GLenum, GLuint)) \
        F(void, BindRenderbuffer, (GLenum, GLuint)) \
        F(void, BindSampler, (GLuint, GLuint)) \
        F(void, BindTexture, (GLenum, GLuint)) \
        F(void, BindVertexArray, (GLuint)) \
        F(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
        F(void, BlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
        F(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
        F(void, BufferSubData, (GLenum, GLsizeiptr, GLsizeiptr, const void*)) \
        F(GLenum, CheckFramebufferStatus, (GLenum)) \
        F(void, Clear, (GLbitfield)) \
        F(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
        F(void, CompileShader, (GLuint)) \
        F(void, CompressedTexImage2D, (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*)) \
        F(GLuint, CreateProgram, ()) \
        F(GLuint, CreateShader, (GLenum)) \
        F(void, CullFace, (GLenum)) \
        F(void, PolygonMode, (GLenum, GLenum)) \
        F(void, DeleteBuffers, (GLsizei, const GLuint*)) \
        F(void, DeleteFramebuffers, (GLsizei, const GLuint*)) \
        F(void, DeleteProgram, (GLuint)) \
        F(void, DeleteRenderbuffers, (GLsizei, const GLuint*)) \
        F(void, DeleteSamplers, (GLsizei, const GLuint*)) \
        F(void, DeleteShader, (GLuint)) \
        F(void, DeleteTextures, (GLsizei, const GLuint*)) \
        F(void, DeleteVertexArrays, (GLsizei, const GLuint*)) \
        F(void, DepthFunc, (GLenum)) \
        F(void, DepthMask, (GLboolean)) \
        F(void, Disable, (GLenum)) \
        F(void, DrawArrays, (GLenum, GLint, GLsizei)) \
        F(void, DrawBuffer, (GLenum)) \
        F(void, DrawElements, (GLenum, GLsizei, GLenum, const void*)) \
        F(void, Enable, (GLenum)) \
        F(void, EnableVertexAttribArray, (GLuint)) \
        F(void, FramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
        F(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
        F(void, FrontFace, (GLenum)) \
        F(void, GenBuffers, (GLsizei, GLuint*)) \
        F(void, GenFramebuffers, (GLsizei, GLuint*)) \
        F(void, GenRenderbuffers, (GLsizei, GLuint*)) \
        F(void, GenSamplers, (GLsizei, GLuint*)) \
        F(void, GenTextures, (GLsizei, GLuint*)) \
        F(void, GenVertexArrays, (GLsizei, GLuint*)) \
        F(void, GenerateMipmap, (GLenum)) \
        F(GLenum, GetError, ()) \
        F(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, char*)) \
        F(void, GetProgramiv, (GLuint, GLenum, GLint*)) \
        F(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, char*)) \
        F(void, GetShaderiv, (GLuint, GLenum, GLint*)) \
        F(const GLubyte*, GetString, (GLenum)) \
        F(GLuint, GetUniformBlockIndex, (GLuint, const char*)) \
        F(GLint, GetUniformLocation, (GLuint, const char*)) \
        F(void, LinkProgram, (GLuint)) \
        F(void, PixelStorei, (GLenum, GLint)) \
        F(void, PolygonOffset, (GLfloat, GLfloat)) \
        F(void, ReadBuffer, (GLenum)) \
        F(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
        F(void, RenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
        F(void, SamplerParameteri, (GLuint, GLenum, GLint)) \
        F(void, Scissor, (GLint, GLint, GLsizei, GLsizei)) \
        F(void, ShaderSource, (GLuint, GLsizei, const char* const*, const GLint*)) \
        F(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
        F(void, TexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
        F(void, TexParameteri, (GLenum, GLenum, GLint)) \
        F(void, Uniform1i, (GLint, GLint)) \
        F(void, UniformBlockBinding, (GLuint, GLuint, GLuint)) \
        F(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
        F(void, UseProgram, (GLuint)) \
        F(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
        F(void, Viewport, (GLint, GLint, GLsizei, GLsizei))

        struct Gl
        {
#define NX_GL_DECLARE(result, name, arguments) result (*name) arguments = nullptr;
            NX_GL_FUNCTIONS(NX_GL_DECLARE)
#undef NX_GL_DECLARE

            bool load(void* (*get)(const char*))
            {
                bool ok = true;
#define NX_GL_LOAD(result, name, arguments) \
                name = reinterpret_cast<result (*) arguments>(get("gl" #name)); \
                if (name == nullptr) \
                { \
                    NX_ERROR("OpenGL has no gl" #name); \
                    ok = false; \
                }
                NX_GL_FUNCTIONS(NX_GL_LOAD)
#undef NX_GL_LOAD
                return ok;
            }
        };

        // Depth runs from 0 to 1 in our clip space, and from -1 to 1 in OpenGL's.
        const char kVertexShader[] = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;
uniform mat4 transform;
out vec4 vertexColor;
out vec2 vertexUv;
void main()
{
    gl_Position = transform * vec4(position, 1.0);
    gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;
    vertexColor = color;
    vertexUv = uv;
}
)";

        const char kFragmentShader[] = R"(#version 330 core
in vec4 vertexColor;
in vec2 vertexUv;
uniform sampler2D image;
layout(location = 0) out vec4 result;
void main()
{
    result = texture(image, vertexUv) * vertexColor;
}
)";

        struct BufferSlot
        {
            GLuint array;
            GLuint buffer;
            u32 count;
        };

        struct EffectSlot
        {
            EffectKind kind;
            GLuint program;
        };

        // Half the size each time.
        constexpr u32 kBlurs = 5;
        struct Blur
        {
            GLuint texture, target;
            u32 width, height;
        };

        struct MeshSlot
        {
            GLuint array, vertices, indices;
            u32 vertexCount, indexCount;
            bool dynamic;
        };

        struct TextureSlot
        {
            GLuint texture;
            TextureInfo info;
        };

        class GlDevice final : public Device
        {
        public:
            explicit GlDevice(GlWindow* window) : m_window(window) {}

            ~GlDevice() override
            {
                if (!m_loaded)
                {
                    return;
                }
                m_buffers.forEach([&](BufferSlot& b) { releaseBuffer(b); });
                m_textures.forEach([&](TextureSlot& t) { gl.DeleteTextures(1, &t.texture); });
                releaseTargets();
                releaseBuffer(m_stream);
                gl.DeleteSamplers(3, m_samplers);
                gl.DeleteSamplers(1, &m_volumeSampler);
                gl.DeleteProgram(m_program);
                m_meshes.forEach([&](MeshSlot& m) { releaseMesh(m); });
                m_effects.forEach([&](EffectSlot& e) { gl.DeleteProgram(e.program); });
                gl.DeleteProgram(m_blurProgram);
                gl.DeleteProgram(m_gradeProgram);
                for (const GLuint program : m_programs)
                {
                    gl.DeleteProgram(program);
                }
                gl.DeleteBuffers(4, m_blocks);
                gl.DeleteFramebuffers(1, &m_shadowTarget);
                gl.DeleteTextures(1, &m_shadowMap);
                gl.DeleteVertexArrays(1, &m_empty);
            }

            bool init()
            {
                if (m_window == nullptr || !gl.load(m_window->load))
                {
                    return false;
                }
                m_loaded = true;
                if (!m_buffers.init(4096, MemTag::Buffer) || !m_pipelines.init(256, MemTag::Buffer) || !m_textures.init(4096, MemTag::Texture) || !m_meshes.init(4096, MemTag::Buffer) || !m_effects.init(128, MemTag::Buffer))
                {
                    return false;
                }
                NX_INFO("OpenGL %s", reinterpret_cast<const char*>(gl.GetString(GL_VERSION)));
                if (!createProgram() || !createMeshState() || !resize())
                {
                    return false;
                }
                gl.GenSamplers(3, m_samplers);
                for (u32 i = 0; i < 3; i++)
                {
                    // The third is for meshes: repeated, with smaller copies further away.
                    const GLint filter = i == static_cast<u32>(Filter::Nearest) ? GL_NEAREST : GL_LINEAR;
                    const GLint wrap = i == 2 ? GL_REPEAT : GL_CLAMP_TO_EDGE;
                    gl.SamplerParameteri(m_samplers[i], GL_TEXTURE_MIN_FILTER, i == 2 ? static_cast<GLint>(GL_LINEAR_MIPMAP_LINEAR) : filter);
                    gl.SamplerParameteri(m_samplers[i], GL_TEXTURE_MAG_FILTER, filter);
                    gl.SamplerParameteri(m_samplers[i], GL_TEXTURE_WRAP_S, wrap);
                    gl.SamplerParameteri(m_samplers[i], GL_TEXTURE_WRAP_T, wrap);
                    gl.SamplerParameteri(m_samplers[i], GL_TEXTURE_WRAP_R, wrap);
                }
                // For volumes: repeated every way.
                gl.GenSamplers(1, &m_volumeSampler);
                gl.SamplerParameteri(m_volumeSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl.SamplerParameteri(m_volumeSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.SamplerParameteri(m_volumeSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.SamplerParameteri(m_volumeSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
                gl.SamplerParameteri(m_volumeSampler, GL_TEXTURE_WRAP_R, GL_REPEAT);
                m_stream = makeBuffer(nullptr, kStreamVertices, GL_STREAM_DRAW);
                const u8 white[4] = { 255, 255, 255, 255 };
                m_white = createTexture(rgbaTexture(white, 1, 1));
                return static_cast<bool>(m_white) && ok("start");
            }

            const char* name() const override { return "opengl"; }

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
                BufferSlot b = makeBuffer(vertices, count, GL_STATIC_DRAW);
                const Buffer handle = ok("vertex buffer") ? m_buffers.add(b) : Buffer();
                if (!handle)
                {
                    releaseBuffer(b);
                    return {};
                }
                m_bufferBytes += count * sizeof(Vertex);
                return handle;
            }

            void destroyBuffer(Buffer handle) override
            {
                if (BufferSlot* b = m_buffers.get(handle))
                {
                    m_bufferBytes -= b->count * sizeof(Vertex);
                    releaseBuffer(*b);
                    m_buffers.remove(handle);
                    m_boundArray = 0;
                }
            }

            Pipeline createPipeline(const PipelineDesc& desc) override { return m_pipelines.add(desc); }

            void destroyPipeline(Pipeline handle) override
            {
                if (m_pipelines.get(handle))
                {
                    m_pipelines.remove(handle);
                    m_bound = {};
                }
            }

            Texture createVolume(const u8* pixels, u32 size) override
            {
                if (pixels == nullptr || size == 0 || size > 256 || (size & (size - 1)) != 0)
                {
                    return {};
                }
                TextureSlot t = {};
                t.info.width = t.info.height = size;
                t.info.bytes = size * size * size * 4;
                gl.GenTextures(1, &t.texture);
                gl.ActiveTexture(GL_TEXTURE2);
                gl.BindTexture(GL_TEXTURE_3D, t.texture);
                gl.TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
                gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
                const GLsizei n = static_cast<GLsizei>(size);
                gl.TexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, n, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                gl.ActiveTexture(GL_TEXTURE0);
                const Texture handle = ok("volume") ? m_textures.add(t) : Texture();
                if (!handle)
                {
                    gl.DeleteTextures(1, &t.texture);
                    return {};
                }
                m_textureBytes += t.info.bytes;
                return handle;
            }

            Texture createTexture(const TextureData& source) override
            {
                if (!validTexture(source))
                {
                    return {};
                }
                TextureSlot t = {};
                t.info.width = source.width;
                t.info.height = source.height;
                t.info.uMax = static_cast<float>(source.width) / static_cast<float>(source.fullWidth);
                t.info.vMax = static_cast<float>(source.height) / static_cast<float>(source.fullHeight);
                t.info.bytes = pixelBytes(source.format, source.fullWidth, source.fullHeight);
                const GLsizei width = static_cast<GLsizei>(source.fullWidth);
                const GLsizei height = static_cast<GLsizei>(source.fullHeight);
                gl.GenTextures(1, &t.texture);
                gl.BindTexture(GL_TEXTURE_2D, t.texture);
                const bool mips = source.mips && source.format == PixelFormat::Rgba8;
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mips ? 12 : 0);
                if (source.format == PixelFormat::Rgba8)
                {
                    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
                    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, source.pixels);
                    if (mips)
                    {
                        gl.GenerateMipmap(GL_TEXTURE_2D);
                        t.info.bytes += t.info.bytes / 3;
                    }
                }
                else
                {
                    gl.CompressedTexImage2D(GL_TEXTURE_2D, 0, source.format == PixelFormat::Bc1 ? GL_BC1 : GL_BC3, width, height, 0, static_cast<GLsizei>(t.info.bytes), source.pixels);
                }
                m_boundTexture = {};
                const Texture handle = ok("texture") ? m_textures.add(t) : Texture();
                if (!handle)
                {
                    gl.DeleteTextures(1, &t.texture);
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
                    gl.DeleteTextures(1, &t->texture);  // the driver keeps it until the GPU has finished with it
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
                m_boundArray = 0;
                m_streamed = 0;
                m_mode = Mode::None;

                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
                gl.Viewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
                gl.Disable(GL_SCISSOR_TEST);
                gl.DepthMask(1);
                gl.ClearColor(clear.r, clear.g, clear.b, clear.a);
                gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                gl.FrontFace(GL_CCW);
                gl.CullFace(GL_BACK);
                gl.UseProgram(m_program);
                gl.ActiveTexture(GL_TEXTURE0);
                // Last frame's vertices may still be in use: the driver is given a new store.
                gl.BindBuffer(GL_ARRAY_BUFFER, m_stream.buffer);
                gl.BufferData(GL_ARRAY_BUFFER, kStreamVertices * sizeof(Vertex), nullptr, GL_STREAM_DRAW);
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
                gl.BindBuffer(GL_ARRAY_BUFFER, m_stream.buffer);
                gl.BufferSubData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(first * sizeof(Vertex)), static_cast<GLsizeiptr>(count * sizeof(Vertex)), vertices);
                m_streamed += count;
                return first;
            }

            void draw(const Draw& draw) override
            {
                NX_ASSERT(m_inFrame);
                BufferSlot* buffer = m_buffers.get(draw.vertices);
                const PipelineDesc* pipeline = m_pipelines.get(draw.pipeline);
                const Texture handle = draw.texture ? draw.texture : m_white;
                TextureSlot* texture = m_textures.get(handle);
                const u32 available = draw.vertices ? (buffer ? buffer->count : 0) : m_streamed;
                if ((draw.vertices && buffer == nullptr) || pipeline == nullptr || texture == nullptr || draw.first > available || draw.count > available - draw.first)
                {
                    m_current.rejected++;
                    return;
                }

                use(Mode::Flat);
                if (m_bound != draw.pipeline)
                {
                    set(GL_BLEND, pipeline->blend != Blend::None);
                    gl.BlendFuncSeparate(GL_SRC_ALPHA, pipeline->blend == Blend::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    // With the test off, OpenGL also stops writing depth, so writes keep it on and always pass.
                    set(GL_DEPTH_TEST, pipeline->depthTest || pipeline->depthWrite);
                    gl.DepthFunc(pipeline->depthTest ? GL_LEQUAL : GL_ALWAYS);
                    gl.DepthMask(pipeline->depthWrite);
                    set(GL_CULL_FACE, pipeline->cull == Cull::Back);
                    m_bound = draw.pipeline;
                }
                if (m_boundTexture != handle)
                {
                    gl.BindTexture(GL_TEXTURE_2D, texture->texture);
                    m_boundTexture = handle;
                }
                gl.BindSampler(0, m_samplers[static_cast<u32>(draw.filter)]);
                setScissor(draw.scissor);
                gl.UniformMatrix4fv(m_transform, 1, 0, draw.transform.m);
                const GLuint array = buffer ? buffer->array : m_stream.array;
                if (m_boundArray != array)
                {
                    gl.BindVertexArray(array);
                    m_boundArray = array;
                }
                gl.DrawArrays(pipeline->topology == Topology::Lines ? GL_LINES : GL_TRIANGLES, static_cast<GLint>(draw.first), static_cast<GLsizei>(draw.count));

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
                MeshSlot m = {};
                m.vertexCount = vertexCount;
                m.indexCount = indexCount;
                m.dynamic = dynamic;
                gl.GenVertexArrays(1, &m.array);
                gl.BindVertexArray(m.array);
                m_boundArray = m.array;
                gl.GenBuffers(1, &m.vertices);
                gl.BindBuffer(GL_ARRAY_BUFFER, m.vertices);
                gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexCount * sizeof(MeshVertex)), vertices, dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
                gl.GenBuffers(1, &m.indices);
                gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.indices);
                gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexCount * sizeof(u16)), indices, GL_STATIC_DRAW);
                const auto attribute = [&](GLuint index, GLint size, GLenum type, GLboolean normalised, size_t offset)
                {
                    gl.EnableVertexAttribArray(index);
                    gl.VertexAttribPointer(index, size, type, normalised, sizeof(MeshVertex), reinterpret_cast<const void*>(offset));
                };
                attribute(0, 3, GL_FLOAT_, 0, offsetof(MeshVertex, x));
                attribute(1, 3, GL_FLOAT_, 0, offsetof(MeshVertex, nx));
                attribute(2, 4, GL_UNSIGNED_BYTE, 1, offsetof(MeshVertex, r));
                attribute(3, 2, GL_FLOAT_, 0, offsetof(MeshVertex, u));
                const Mesh handle = ok("mesh") ? m_meshes.add(m) : Mesh();
                if (!handle)
                {
                    releaseMesh(m);
                    return {};
                }
                m_bufferBytes += vertexCount * sizeof(MeshVertex) + indexCount * sizeof(u16);
                return handle;
            }

            void updateMesh(Mesh handle, const MeshVertex* vertices, u32 count) override
            {
                const MeshSlot* m = m_meshes.get(handle);
                if (m && m->dynamic && count <= m->vertexCount)
                {
                    gl.BindBuffer(GL_ARRAY_BUFFER, m->vertices);
                    gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m->vertexCount * sizeof(MeshVertex)), nullptr, GL_STREAM_DRAW);
                    gl.BufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(count * sizeof(MeshVertex)), vertices);
                }
            }

            void destroyMesh(Mesh handle) override
            {
                if (MeshSlot* m = m_meshes.get(handle))
                {
                    m_bufferBytes -= m->vertexCount * sizeof(MeshVertex) + m->indexCount * sizeof(u16);
                    releaseMesh(*m);
                    m_meshes.remove(handle);
                    m_boundArray = 0;
                }
            }

            void beginView(const View& view) override
            {
                NX_ASSERT(m_inFrame && !m_shadowing);
                ViewUniforms u;
                fillView(u, view, true);
                upload(0, &u, sizeof(u));
                m_viewTime = view.time;
                gl.Disable(GL_SCISSOR_TEST);
                gl.DepthMask(1);
                gl.Clear(GL_DEPTH_BUFFER_BIT);
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
                upload(1, &u, sizeof(u));
                const EffectSlot* effect = draw.effect && !m_shadowing ? m_effects.get(draw.effect) : nullptr;
                effect = effect && effect->kind == EffectKind::Surface ? effect : nullptr;
                if (effect)
                {
                    const Scissor all;
                    setValues(draw.values, m_viewTime, all);
                    gl.UseProgram(effect->program);
                }
                if (!m_shadowing)
                {
                    set(GL_BLEND, draw.blend != Blend::None);
                    gl.BlendFuncSeparate(GL_SRC_ALPHA, draw.blend == Blend::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    gl.DepthMask(draw.depthWrite);
                    set(GL_CULL_FACE, draw.cull == Cull::Back && !draw.lines);
                    if (draw.lines)
                    {
                        gl.PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    }
                    gl.BindSampler(0, m_samplers[draw.repeat ? 2 : 0]);
                    if (m_boundTexture != handle)
                    {
                        gl.BindTexture(GL_TEXTURE_2D, texture->texture);
                        m_boundTexture = handle;
                    }
                    if (const TextureSlot* volume = draw.volume ? m_textures.get(draw.volume) : nullptr)
                    {
                        gl.ActiveTexture(GL_TEXTURE2);
                        gl.BindTexture(GL_TEXTURE_3D, volume->texture);
                        gl.BindSampler(2, m_volumeSampler);
                        gl.ActiveTexture(GL_TEXTURE0);
                    }
                }
                if (m_boundArray != m->array)
                {
                    gl.BindVertexArray(m->array);
                    m_boundArray = m->array;
                }
                gl.DrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT, reinterpret_cast<const void*>(static_cast<size_t>(draw.first) * sizeof(u16)));
                if (draw.lines && !m_shadowing)
                {
                    gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
                if (effect)
                {
                    gl.UseProgram(m_programs[kMesh]);
                }
                m_current.draws++;
                m_current.triangles += count / 3;
            }

            bool drawSky() override
            {
                screen(m_programs[kSky], false);
                return true;
            }

            bool drawField(const Field& field) override
            {
                FieldUniforms u;
                fillField(u, field);
                upload(2, &u, sizeof(u));
                screen(m_programs[kField], true);
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
                upload(0, &u, sizeof(u));
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_shadowTarget);
                gl.Disable(GL_SCISSOR_TEST);
                gl.DepthMask(1);
                gl.Clear(GL_DEPTH_BUFFER_BIT);
                return true;
            }

            void endShadows() override
            {
                NX_ASSERT(m_shadowing);
                m_shadowing = false;
                m_mode = Mode::None;
                gl.Disable(GL_POLYGON_OFFSET_FILL);
                gl.Disable(GL_DEPTH_CLAMP);
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
            }

            Effect createEffect(EffectKind kind, const char* source, char* error, size_t errorSize) override
            {
                using namespace shaders;
                EffectSlot slot = {};
                slot.kind = kind;
                if (kind == EffectKind::Picture)
                {
                    const char* vertex[2] = { kMeshCommon, kScreenVertex };
                    const char* fragment[6] = { "#version 330 core\n", kEffectGlslWords, kEffectPictureStart, "\n#line 1\n", source, kEffectPictureMain };
                    slot.program = effectProgram(vertex, 2, fragment, 6, error, errorSize);
                }
                else
                {
                    const char* vertex[6] = { kMeshCommon, kEffectGlslWords, kEffectVertexStart, "\n#line 1\n", source, kEffectVertexMain };
                    const char* fragment[8] = { kMeshCommon, kShade, kMeshFinish, kEffectGlslWords, kEffectSurfaceStart, "\n#line 1\n", source, kEffectSurfaceMain };
                    slot.program = effectProgram(vertex, 6, fragment, 8, error, errorSize);
                }
                const Effect effect = slot.program ? m_effects.add(slot) : Effect();
                if (!effect && slot.program)
                {
                    gl.DeleteProgram(slot.program);
                }
                m_mode = Mode::None;
                return effect;
            }

            void destroyEffect(Effect handle) override
            {
                if (EffectSlot* slot = m_effects.get(handle))
                {
                    gl.DeleteProgram(slot->program);
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
                copyPicture();
                setValues(values, time, area);
                pass(slot->program, m_copy, m_target, area, m_width, m_height, false);
                return true;
            }

            bool bloom(const Bloom& bloom, const Scissor& area) override
            {
                if (!m_inFrame || bloom.strength <= 0 || !copyTargets())
                {
                    return false;
                }
                copyPicture();
                const Scissor all;
                const auto step = [&](float what, u32 width, u32 height, float strength)
                {
                    float values[16] = { what, bloom.threshold, strength, 0, bloom.spread / static_cast<float>(width), bloom.spread / static_cast<float>(height) };
                    setValues(values, 0, all);
                };
                for (u32 i = 0; i < kBlurs; i++)
                {
                    step(i == 0 ? 0.0f : 1.0f, i == 0 ? m_width : m_blurs[i - 1].width, i == 0 ? m_height : m_blurs[i - 1].height, 1);
                    pass(m_blurProgram, i == 0 ? m_copy : m_blurs[i - 1].texture, m_blurs[i].target, all, m_blurs[i].width, m_blurs[i].height, false);
                }
                for (u32 i = kBlurs - 1; i > 0; i--)
                {
                    step(2, m_blurs[i].width, m_blurs[i].height, 0.8f);
                    pass(m_blurProgram, m_blurs[i].texture, m_blurs[i - 1].target, all, m_blurs[i - 1].width, m_blurs[i - 1].height, true);
                }
                step(2, m_blurs[0].width, m_blurs[0].height, bloom.strength * 0.5f);
                pass(m_blurProgram, m_blurs[0].texture, m_target, all, m_width, m_height, true, area);
                return true;
            }

            bool grade(Texture table, float strength, const Scissor& area) override
            {
                const TextureSlot* t = m_textures.get(table);
                if (!m_inFrame || t == nullptr || !copyTargets())
                {
                    return false;
                }
                if (m_gradeProgram == 0)
                {
                    char error[256];
                    const char* vertex[2] = { shaders::kMeshCommon, shaders::kScreenVertex };
                    const char* fragment[2] = { "#version 330 core
", shaders::kGradeFragment };
                    m_gradeProgram = effectProgram(vertex, 2, fragment, 2, error, sizeof(error));
                    if (m_gradeProgram == 0)
                    {
                        NX_ERROR("opengl: grade: %s", error);
                        return false;
                    }
                }
                copyPicture();
                const float values[16] = { strength, static_cast<float>(t->info.width) };
                setValues(values, 0, {});
                gl.ActiveTexture(GL_TEXTURE2);
                gl.BindTexture(GL_TEXTURE_3D, t->texture);
                gl.BindSampler(2, m_samplers[0]);
                gl.ActiveTexture(GL_TEXTURE0);
                pass(m_gradeProgram, m_copy, m_target, {}, m_width, m_height, false, area);
                return true;
            }

            void endFrame() override
            {
                NX_ASSERT(m_inFrame);
                m_inFrame = false;
                gl.Disable(GL_SCISSOR_TEST);
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, m_target);
                gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                const GLint width = static_cast<GLint>(m_width);
                const GLint height = static_cast<GLint>(m_height);
                gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                m_window->swap(m_window->user);
                m_last = m_current;
                ok("frame");
            }

            bool capture(u8* pixels, size_t bytes) override
            {
                const size_t row = static_cast<size_t>(m_width) * 4;
                if (pixels == nullptr || bytes < row * m_height || m_inFrame || m_target == 0)
                {
                    return false;
                }
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, m_target);
                gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
                // OpenGL's first row is the bottom one.
                for (u32 y = 0; y < m_height; y++)
                {
                    gl.ReadPixels(0, static_cast<GLint>(m_height - 1 - y), static_cast<GLsizei>(m_width), 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels + y * row);
                }
                return ok("capture");
            }

            Stats stats() const override
            {
                Stats s = m_last;
                s.bufferBytes = m_bufferBytes;
                s.textureBytes = m_textureBytes;
                return s;
            }

        private:
            bool ok(const char* what)
            {
                bool fine = true;
                for (GLenum error = gl.GetError(); error && m_errors < 100; error = gl.GetError())
                {
                    NX_ERROR("opengl: %s failed (0x%04x)", what, error);
                    m_errors++;
                    fine = false;
                }
                return fine;
            }

            void set(GLenum what, bool on)
            {
                if (on)
                {
                    gl.Enable(what);
                }
                else
                {
                    gl.Disable(what);
                }
            }

            enum class Mode : u8 { None, Flat, Mesh, Shadow, Screen };
            enum { kMesh, kShadow, kSky, kField, kPrograms };

            void upload(u32 block, const void* data, size_t size)
            {
                gl.BindBuffer(GL_UNIFORM_BUFFER, m_blocks[block]);
                gl.BufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
            }

            // Whatever a kind of drawing needs that isn't set for each draw.
            void use(Mode mode)
            {
                if (m_mode == mode)
                {
                    return;
                }
                m_mode = mode;
                m_bound = {};
                m_boundTexture = {};
                gl.ActiveTexture(GL_TEXTURE0);
                set(GL_SCISSOR_TEST, mode == Mode::Flat);
                if (mode == Mode::Shadow)
                {
                    gl.Viewport(0, 0, kShadowSize, kShadowSize);
                    gl.UseProgram(m_programs[kShadow]);
                    gl.Enable(GL_DEPTH_TEST);
                    gl.DepthFunc(GL_LEQUAL);
                    gl.DepthMask(1);
                    gl.Disable(GL_BLEND);
                    gl.Disable(GL_CULL_FACE);
                    // Pushed away a little, so that a surface doesn't shade itself.
                    gl.Enable(GL_POLYGON_OFFSET_FILL);
                    gl.PolygonOffset(2.0f, 4.0f);
                    gl.Enable(GL_DEPTH_CLAMP);
                    return;
                }
                if (mode != Mode::Flat && m_area.enabled)
                {
                    gl.Viewport(m_area.x, static_cast<GLint>(m_height) - m_area.y - m_area.height, m_area.width, m_area.height);
                }
                else
                {
                    gl.Viewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
                }
                if (mode == Mode::Flat)
                {
                    gl.UseProgram(m_program);
                    return;
                }
                gl.Enable(GL_DEPTH_TEST);
                gl.DepthFunc(GL_LEQUAL);
                gl.ActiveTexture(GL_TEXTURE1);
                gl.BindTexture(GL_TEXTURE_2D, m_shadowMap);
                gl.BindSampler(1, 0);
                gl.ActiveTexture(GL_TEXTURE0);
                if (mode == Mode::Mesh)
                {
                    gl.UseProgram(m_programs[kMesh]);
                }
            }

            GLuint effectProgram(const char* const* vertex, GLsizei vertexCount, const char* const* fragment, GLsizei fragmentCount, char* error, size_t errorSize)
            {
                if (errorSize)
                {
                    error[0] = 0;
                }
                const GLuint program = gl.CreateProgram();
                const GLenum kinds[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
                bool fine = true;
                for (u32 i = 0; i < 2; i++)
                {
                    const GLuint shader = gl.CreateShader(kinds[i]);
                    gl.ShaderSource(shader, i ? fragmentCount : vertexCount, i ? fragment : vertex, nullptr);
                    gl.CompileShader(shader);
                    GLint compiled = 0;
                    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                    if (!compiled && fine)
                    {
                        gl.GetShaderInfoLog(shader, static_cast<GLsizei>(errorSize), nullptr, error);
                        fine = false;
                    }
                    gl.AttachShader(program, shader);
                    gl.DeleteShader(shader);
                }
                gl.LinkProgram(program);
                GLint linked = 0;
                gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
                if (!linked || !fine)
                {
                    if (fine)
                    {
                        gl.GetProgramInfoLog(program, static_cast<GLsizei>(errorSize), nullptr, error);
                    }
                    gl.DeleteProgram(program);
                    return 0;
                }
                static const char* const blocks[] = { "View", "Object", "Field", "EffectValues" };
                for (u32 i = 0; i < 4; i++)
                {
                    const GLuint index = gl.GetUniformBlockIndex(program, blocks[i]);
                    if (index != 0xFFFFFFFFu)
                    {
                        gl.UniformBlockBinding(program, index, i);
                    }
                }
                gl.UseProgram(program);
                const GLint image = gl.GetUniformLocation(program, "image");
                const GLint shadow = gl.GetUniformLocation(program, "shadowMap");
                const GLint volume = gl.GetUniformLocation(program, "volumeImage");
                if (image >= 0)
                {
                    gl.Uniform1i(image, 0);
                }
                if (shadow >= 0)
                {
                    gl.Uniform1i(shadow, 1);
                }
                if (volume >= 0)
                {
                    gl.Uniform1i(volume, 2);
                }
                return program;
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
                upload(3, &u, sizeof(u));
            }

            // Covers a target with a picture. Only part of it is changed, if there is a clip.
            void pass(GLuint program, GLuint from, GLuint to, const Scissor& area, u32 width, u32 height, bool adds, const Scissor& clip = {})
            {
                use(Mode::None);
                m_mode = Mode::None;
                m_bound = {};
                m_boundTexture = {};
                gl.BindFramebuffer(GL_FRAMEBUFFER, to);
                if (area.enabled)
                {
                    gl.Viewport(area.x, static_cast<GLint>(height) - area.y - area.height, area.width, area.height);
                }
                else
                {
                    gl.Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
                }
                set(GL_SCISSOR_TEST, clip.enabled);
                if (clip.enabled)
                {
                    gl.Scissor(clip.x, static_cast<GLint>(height) - clip.y - clip.height, clip.width, clip.height);
                }
                gl.UseProgram(program);
                gl.Disable(GL_DEPTH_TEST);
                gl.Disable(GL_CULL_FACE);
                gl.DepthMask(0);
                set(GL_BLEND, adds);
                gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                gl.ActiveTexture(GL_TEXTURE0);
                gl.BindTexture(GL_TEXTURE_2D, from);
                gl.BindSampler(0, m_samplers[0]);
                gl.BindVertexArray(m_empty);
                m_boundArray = m_empty;
                gl.DrawArrays(GL_TRIANGLES, 0, 3);
                gl.DepthMask(1);
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
                m_current.draws++;
            }

            void copyPicture()
            {
                gl.Disable(GL_SCISSOR_TEST);
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, m_target);
                gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_copyTarget);
                const GLint width = static_cast<GLint>(m_width), height = static_cast<GLint>(m_height);
                gl.BlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
            }

            bool makeTarget(GLuint& texture, GLuint& target, GLint format, GLenum type, u32 width, u32 height)
            {
                gl.GenTextures(1, &texture);
                gl.BindTexture(GL_TEXTURE_2D, texture);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gl.TexImage2D(GL_TEXTURE_2D, 0, format, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, type == GL_UNSIGNED_BYTE ? GL_RGBA : GL_RGB, type, nullptr);
                gl.GenFramebuffers(1, &target);
                gl.BindFramebuffer(GL_FRAMEBUFFER, target);
                gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                const bool fine = gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
                m_boundTexture = {};
                return fine;
            }

            // Room for what effects read from, made when the first is used.
            bool copyTargets()
            {
                if (m_copy)
                {
                    return m_copyFine;
                }
                if (m_blurProgram == 0)
                {
                    char error[256];
                    const char* vertex[2] = { shaders::kMeshCommon, shaders::kScreenVertex };
                    const char* fragment[2] = { "#version 330 core\n", shaders::kBlurFragment };
                    m_blurProgram = effectProgram(vertex, 2, fragment, 2, error, sizeof(error));
                    if (m_blurProgram == 0)
                    {
                        NX_ERROR("opengl: blur: %s", error);
                    }
                }
                m_copyFine = m_blurProgram && makeTarget(m_copy, m_copyTarget, GL_RGBA8, GL_UNSIGNED_BYTE, m_width, m_height);
                u32 width = m_width, height = m_height;
                for (Blur& b : m_blurs)
                {
                    width = width > 2 ? width / 2 : 1;
                    height = height > 2 ? height / 2 : 1;
                    b.width = width;
                    b.height = height;
                    m_copyFine = m_copyFine && makeTarget(b.texture, b.target, GL_R11F_G11F_B10F, GL_FLOAT_, width, height);
                }
                return m_copyFine;
            }

            void screen(GLuint program, bool depthWrite)
            {
                use(Mode::Screen);
                gl.UseProgram(program);
                gl.Disable(GL_BLEND);
                gl.Disable(GL_CULL_FACE);
                gl.DepthMask(depthWrite);
                gl.BindVertexArray(m_empty);
                m_boundArray = m_empty;
                gl.DrawArrays(GL_TRIANGLES, 0, 3);
                m_current.draws++;
            }

            GLuint link(const char* vertex, const char* shading, const char* fragment)
            {
                const GLuint program = gl.CreateProgram();
                const char* vertexParts[2] = { shaders::kMeshCommon, vertex };
                const char* fragmentParts[3] = { shaders::kMeshCommon, shading, fragment };
                const GLuint parts[2] = { compile(GL_VERTEX_SHADER, vertexParts, 2), compile(GL_FRAGMENT_SHADER, fragmentParts, 3) };
                for (const GLuint part : parts)
                {
                    gl.AttachShader(program, part);
                }
                gl.LinkProgram(program);
                for (const GLuint part : parts)
                {
                    gl.DeleteShader(part);
                }
                GLint linked = 0;
                gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
                if (!linked)
                {
                    char text[512] = {};
                    gl.GetProgramInfoLog(program, sizeof(text), nullptr, text);
                    NX_ERROR("opengl: program: %s", text);
                    gl.DeleteProgram(program);
                    return 0;
                }
                static const char* const blocks[] = { "View", "Object", "Field" };
                for (u32 i = 0; i < 3; i++)
                {
                    const GLuint index = gl.GetUniformBlockIndex(program, blocks[i]);
                    if (index != 0xFFFFFFFFu)
                    {
                        gl.UniformBlockBinding(program, index, i);
                    }
                }
                gl.UseProgram(program);
                const GLint image = gl.GetUniformLocation(program, "image");
                const GLint shadow = gl.GetUniformLocation(program, "shadowMap");
                const GLint volume = gl.GetUniformLocation(program, "volumeImage");
                if (image >= 0)
                {
                    gl.Uniform1i(image, 0);
                }
                if (shadow >= 0)
                {
                    gl.Uniform1i(shadow, 1);
                }
                if (volume >= 0)
                {
                    gl.Uniform1i(volume, 2);
                }
                return program;
            }

            GLuint linkMesh()
            {
                const std::string shading = std::string(shaders::kShade) + shaders::kMeshFinish;
                return link(shaders::kMeshVertex, shading.c_str(), shaders::kMeshFragment);
            }

            bool createMeshState()
            {
                using namespace shaders;
                gl.GenBuffers(4, m_blocks);
                const size_t sizes[4] = { sizeof(ViewUniforms), sizeof(ObjectUniforms), sizeof(FieldUniforms), sizeof(EffectUniforms) };
                for (GLuint i = 0; i < 4; i++)
                {
                    gl.BindBuffer(GL_UNIFORM_BUFFER, m_blocks[i]);
                    gl.BufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizes[i]), nullptr, GL_DYNAMIC_DRAW);
                    gl.BindBufferBase(GL_UNIFORM_BUFFER, i, m_blocks[i]);
                }
                gl.GenVertexArrays(1, &m_empty);
                m_programs[kMesh] = linkMesh();
                m_programs[kShadow] = link(kShadowVertex, "", kShadowFragment);
                m_programs[kSky] = link(kScreenVertex, kShade, kSkyFragment);
                m_programs[kField] = link(kScreenVertex, kShade, kFieldFragment);
                for (const GLuint program : m_programs)
                {
                    if (program == 0)
                    {
                        return false;
                    }
                }
                gl.GenTextures(1, &m_shadowMap);
                gl.BindTexture(GL_TEXTURE_2D, m_shadowMap);
                gl.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT_, nullptr);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                gl.GenFramebuffers(1, &m_shadowTarget);
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_shadowTarget);
                gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMap, 0);
                gl.DrawBuffer(0);
                gl.ReadBuffer(0);
                if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    NX_ERROR("opengl: no shadow map");
                    return false;
                }
                return ok("mesh state");
            }

            void releaseMesh(MeshSlot& m)
            {
                gl.DeleteVertexArrays(1, &m.array);
                gl.DeleteBuffers(1, &m.vertices);
                gl.DeleteBuffers(1, &m.indices);
                m = {};
            }

            void setScissor(const Scissor& s)
            {
                i32 left = 0, top = 0, right = static_cast<i32>(m_width), bottom = static_cast<i32>(m_height);
                if (s.enabled)
                {
                    const i32 r = s.x + (s.width > 0 ? s.width : 0);
                    const i32 b = s.y + (s.height > 0 ? s.height : 0);
                    left = s.x > 0 ? s.x : 0;
                    top = s.y > 0 ? s.y : 0;
                    right = r < right ? r : right;
                    bottom = b < bottom ? b : bottom;
                }
                const i32 width = right > left ? right - left : 0;
                const i32 height = bottom > top ? bottom - top : 0;
                gl.Scissor(left, static_cast<GLint>(m_height) - top - height, width, height);
            }

            BufferSlot makeBuffer(const Vertex* vertices, u32 count, GLenum use)
            {
                BufferSlot b = {};
                b.count = count;
                gl.GenVertexArrays(1, &b.array);
                gl.BindVertexArray(b.array);
                m_boundArray = b.array;
                gl.GenBuffers(1, &b.buffer);
                gl.BindBuffer(GL_ARRAY_BUFFER, b.buffer);
                gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(Vertex)), vertices, use);
                const auto attribute = [&](GLuint index, GLint size, GLenum type, GLboolean normalised, size_t offset)
                {
                    gl.EnableVertexAttribArray(index);
                    gl.VertexAttribPointer(index, size, type, normalised, sizeof(Vertex), reinterpret_cast<const void*>(offset));
                };
                attribute(0, 3, GL_FLOAT_, 0, offsetof(Vertex, x));
                attribute(1, 4, GL_UNSIGNED_BYTE, 1, offsetof(Vertex, r));
                attribute(2, 2, GL_FLOAT_, 0, offsetof(Vertex, u));
                return b;
            }

            void releaseBuffer(BufferSlot& b)
            {
                gl.DeleteVertexArrays(1, &b.array);
                gl.DeleteBuffers(1, &b.buffer);
                b = {};
            }

            GLuint compile(GLenum kind, const char* const* source, GLsizei count)
            {
                const GLuint shader = gl.CreateShader(kind);
                gl.ShaderSource(shader, count, source, nullptr);
                gl.CompileShader(shader);
                GLint compiled = 0;
                gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                if (!compiled)
                {
                    char text[512] = {};
                    gl.GetShaderInfoLog(shader, sizeof(text), nullptr, text);
                    NX_ERROR("opengl: shader: %s", text);
                }
                return shader;
            }

            bool createProgram()
            {
                const char* vertexSource = kVertexShader;
                const char* fragmentSource = kFragmentShader;
                const GLuint vertex = compile(GL_VERTEX_SHADER, &vertexSource, 1);
                const GLuint fragment = compile(GL_FRAGMENT_SHADER, &fragmentSource, 1);
                m_program = gl.CreateProgram();
                gl.AttachShader(m_program, vertex);
                gl.AttachShader(m_program, fragment);
                gl.LinkProgram(m_program);
                gl.DeleteShader(vertex);
                gl.DeleteShader(fragment);
                GLint linked = 0;
                gl.GetProgramiv(m_program, GL_LINK_STATUS, &linked);
                if (!linked)
                {
                    char text[512] = {};
                    gl.GetProgramInfoLog(m_program, sizeof(text), nullptr, text);
                    NX_ERROR("opengl: program: %s", text);
                    return false;
                }
                m_transform = gl.GetUniformLocation(m_program, "transform");
                gl.UseProgram(m_program);
                gl.Uniform1i(gl.GetUniformLocation(m_program, "image"), 0);
                return true;
            }

            void releaseTargets()
            {
                gl.DeleteFramebuffers(1, &m_target);
                gl.DeleteTextures(1, &m_color);
                gl.DeleteRenderbuffers(1, &m_depth);
                m_target = m_color = m_depth = 0;
                gl.DeleteFramebuffers(1, &m_copyTarget);
                gl.DeleteTextures(1, &m_copy);
                m_copy = m_copyTarget = 0;
                for (Blur& b : m_blurs)
                {
                    gl.DeleteFramebuffers(1, &b.target);
                    gl.DeleteTextures(1, &b.texture);
                    b = {};
                }
            }

            // False when the window has no area.
            bool resize()
            {
                u32 width = 0, height = 0;
                m_window->size(m_window->user, width, height);
                if (width == 0 || height == 0)
                {
                    return false;
                }
                if (width == m_width && height == m_height && m_target)
                {
                    return true;
                }
                releaseTargets();
                m_width = width;
                m_height = height;
                gl.GenTextures(1, &m_color);
                gl.BindTexture(GL_TEXTURE_2D, m_color);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                gl.GenRenderbuffers(1, &m_depth);
                gl.BindRenderbuffer(GL_RENDERBUFFER, m_depth);
                gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
                gl.GenFramebuffers(1, &m_target);
                gl.BindFramebuffer(GL_FRAMEBUFFER, m_target);
                gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
                gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depth);
                if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    NX_ERROR("opengl: cannot make a %ux%u target", static_cast<unsigned>(width), static_cast<unsigned>(height));
                    releaseTargets();
                    return false;
                }
                return ok("resize");
            }

            Gl gl;
            GlWindow* m_window;
            GLuint m_program = 0;
            GLint m_transform = -1;
            GLuint m_samplers[3] = {};
            GLuint m_volumeSampler = 0;
            BufferSlot m_stream = {};
            GLuint m_target = 0, m_color = 0, m_depth = 0;
            GLuint m_boundArray = 0;
            HandlePool<BufferSlot, BufferTag> m_buffers;
            HandlePool<PipelineDesc, PipelineTag> m_pipelines;
            HandlePool<TextureSlot, TextureTag> m_textures;
            HandlePool<EffectSlot, EffectTag> m_effects;
            GLuint m_copy = 0, m_copyTarget = 0, m_blurProgram = 0, m_gradeProgram = 0;
            Blur m_blurs[kBlurs] = {};
            bool m_copyFine = false;
            float m_viewTime = 0;
            HandlePool<MeshSlot, MeshTag> m_meshes;
            GLuint m_programs[kPrograms] = {};
            GLuint m_blocks[4] = {};
            GLuint m_empty = 0;
            GLuint m_shadowMap = 0, m_shadowTarget = 0;
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
            u32 m_errors = 0;
            bool m_inFrame = false;
            bool m_loaded = false;
        };
    }

    Device* createDevice(void* nativeWindow)
    {
        GlDevice* device = create<GlDevice>(MemTag::General, static_cast<GlWindow*>(nativeWindow));
        if (device && !device->init())
        {
            destroy(device);
            return nullptr;
        }
        return device;
    }
}
