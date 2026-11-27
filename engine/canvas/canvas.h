#pragma once

#include "assets/assets.h"
#include "base/math.h"
#include "canvas/look.h"
#include "gpu/gpu.h"

namespace nx
{
    using gpu::Color;

    enum class Align : u8 { Left, Center, Right };

    // Immediate 2D drawing. Positions are in design units from the top left, y down, and are scaled to
    // whatever screen there is. Everything that can share a draw call does.
    class Canvas : NonCopyable
    {
    public:
        ~Canvas() { shutdown(); }

        bool init(gpu::Device& device, Assets& assets);
        void shutdown();

        // Around each frame's drawing, between the device's beginFrame and endFrame.
        void begin();
        void end();

        // --- Screen ---

        // Everything after this is drawn on a screen of this size, fitted inside the real one.
        void design(float width, float height);
        // As above, but the width follows the shape of the real screen, so there are no borders.
        void designHeight(float height);
        float width() const { return m_width; }
        float height() const { return m_height; }
        // Real pixels to design units.
        Vec2 toDesign(float x, float y) const;

        // --- State: set until changed, and saved by push() ---

        void push();
        void pop();
        void translate(float x, float y);
        void rotate(float degrees);
        void scale(float x, float y);
        // x' = m0 x + m2 y + m4, y' = m1 x + m3 y + m5.
        void transform(const float (&m)[6]);
        void setAlpha(float alpha) { m_state.alpha = alpha; }
        float alpha() const { return m_state.alpha; }
        gpu::Blend blend() const { return m_state.blend; }
        gpu::Filter filter() const { return m_state.filter; }
        void setBlend(gpu::Blend blend) { m_state.blend = blend; }
        void setFilter(gpu::Filter filter) { m_state.filter = filter; }
        // Nothing outside is drawn. Within another clip, only the overlap. Ignores rotation.
        void clip(float x, float y, float width, float height);
        void noClip();

        // --- Shapes ---

        void rect(float x, float y, float w, float h, const Color& color);
        void rectLine(float x, float y, float w, float h, float thickness, const Color& color);
        void gradient(float x, float y, float w, float h, const Color& top, const Color& bottom);
        void roundRect(float x, float y, float w, float h, float radius, const Color& color);
        void roundLine(float x, float y, float w, float h, float radius, float thickness, const Color& color);
        void line(float x1, float y1, float x2, float y2, float thickness, const Color& color);
        void circle(float x, float y, float radius, const Color& color);
        void ring(float x, float y, float radius, float thickness, const Color& color);
        void arc(float x, float y, float radius, float thickness, float startDegrees, float endDegrees, const Color& color);
        void triangle(const Vec2& a, const Vec2& b, const Vec2& c, const Color& ca, const Color& cb, const Color& cc);

        // --- Images ---

        void image(const Asset& texture, float x, float y, float w, float h, const Color& tint);
        // Part of an image, in its own pixels.
        void imagePart(const Asset& texture, float x, float y, float w, float h, float sx, float sy, float sw, float sh, const Color& tint);
        // One frame of a sheet of equal frames, counting along each row.
        void sprite(const Asset& texture, u32 columns, u32 rows, u32 frame, float x, float y, float w, float h, const Color& tint);
        // Corners keep their size, edges stretch one way and the middle both ways.
        void nineSlice(const Asset& texture, float x, float y, float w, float h, float corner, const Color& tint);
        // Repeats an image across an area, starting offset by (ox, oy).
        void tile(const Asset& texture, float x, float y, float w, float h, float tileW, float tileH, float ox, float oy, const Color& tint);

        // An image or, with none, a plain rectangle: centred on the origin of a transform, with the
        // part of the image given in 0 to 1. For the scene, which has a great many.
        void placed(const Asset* texture, const float (&m)[6], float w, float h, float u0, float v0, float u1, float v1, const Color& color);
        // A round glow, fading to nothing at its edge. Any number share a draw call.
        void glow(float x, float y, float radius, const Color& color);

        // --- Text: UTF-8, '\n' starts a new line. Returns the width. ---

        float text(const Asset& font, const char* text, float x, float y, const Color& color, Align align = Align::Left);
        Vec2 measure(const Asset& font, const char* text) const;
        // Breaks lines at spaces to fit a width. Returns the height.
        float paragraph(const Asset& font, const char* text, float x, float y, float width, const Color& color, Align align = Align::Left);
        // How tall that would be, with nothing drawn.
        float paragraphHeight(const Asset& font, const char* text, float width) const;
        // A paragraph with some of it marked: [color=#ff8000] or [color=orange] ... [/color], [b] bold [/b], [u] underlined
        // [/u], [s] struck through [/s]; [[ is a [ itself. Returns the height. rich.cpp.
        float rich(const Asset& font, const char* text, float x, float y, float width, const Color& color, Align align = Align::Left);
        float richHeight(const Asset& font, const char* text, float width) const;

        // Draws into a picture made with Assets::target until endTarget, cleared to a colour first, at its size: the design is
        // its pixels until it is changed. One at a time. False if it can't be, and then drawing goes on to the screen.
        bool beginTarget(const Asset& target, const Color& clear);
        void endTarget();
        bool inTarget() const { return m_inTarget; }
        // The size drawn into, in pixels, and what turns design units into them: x' = m0 x + m2 y + m4.
        float pixelWidth() const { return m_pixelWidth; }
        float pixelHeight() const { return m_pixelHeight; }
        void getTransform(float (&m)[6]) const;
        void setTransform(const float (&m)[6]);

        // Before something else draws: everything so far goes first.
        void finish() { flush(); }
        // Where an area of the design is on the real screen.
        gpu::Scissor toPixels(float x, float y, float w, float h) const;

        // All that has been drawn so far, through a shader. False where there are none.
        bool effect(const Asset& effect, const float (&values)[16], float time);
        // Light that spills from what is bright.
        bool bloom(float strength, float threshold, float spread);
        // All drawn so far, blurred about so many pixels across (at 720 high), within the clip if there is one.
        bool blur(float pixels);
        // Each colour of it as a look says, mixed with what it was by strength.
        bool grade(ColorLook& look, float strength);

        // Draws so far this frame.
        u32 draws() const { return m_draws; }

    private:
        struct State
        {
            float m[6] = { 1, 0, 0, 1, 0, 0 };  // x' = m0 x + m2 y + m4
            float alpha = 1;
            gpu::Blend blend = gpu::Blend::Alpha;
            gpu::Filter filter = gpu::Filter::Linear;
            gpu::Scissor scissor;
        };

        struct Point
        {
            float x, y, u, v;
            u32 color;
        };

        static constexpr u32 kMaxVertices = 6144;
        int cornerSteps(float radius) const;
        static constexpr u32 kMaxStates = 32;

        u32 pack(const Color& color) const;
        void use(gpu::Texture texture, float uMax, float vMax);
        void addTriangle(const Point& a, const Point& b, const Point& c);
        void addQuad(const Point& a, const Point& b, const Point& c, const Point& d);
        void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, u32 color);
        void flush();
        float lineWidth(const FontData& font, const char* text, const char* end) const;
        static const char* lineEnd(const Canvas& canvas, const FontData& font, const char* line, float width, const char*& next);
        float textRun(const Asset& font, const char* text, const char* end, float x, float y, u32 color);

        gpu::Device* m_device = nullptr;
        Assets* m_assets = nullptr;
        gpu::Pipeline m_pipelines[static_cast<u32>(gpu::Blend::Count)];
        gpu::Texture m_soft;
        gpu::Vertex* m_vertices = nullptr;
        u32 m_count = 0;
        u32 m_draws = 0;

        State m_state;
        State m_states[kMaxStates];
        u32 m_depth = 0;

        // What the collected vertices will be drawn with.
        gpu::Texture m_texture;
        gpu::Blend m_batchBlend = gpu::Blend::Alpha;
        gpu::Filter m_batchFilter = gpu::Filter::Linear;
        gpu::Scissor m_batchScissor;
        float m_uMax = 1, m_vMax = 1;

        float m_pixelWidth = 1, m_pixelHeight = 1;
        float m_width = 1, m_height = 1;
        float m_scale = 1, m_offsetX = 0, m_offsetY = 0;
        bool m_overflowed = false;

        // The screen's, while a picture is drawn into.
        struct Saved
        {
            State state;
            State states[kMaxStates];
            u32 depth;
            float pixelWidth, pixelHeight, width, height, scale, offsetX, offsetY;
        };
        Saved* m_saved = nullptr;
        bool m_inTarget = false;
    };
}
