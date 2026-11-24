// A 3D shape turned and projected in script, and drawn with 2D lines and triangles.

class WireframeScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    array<Vec3> points;
    array<uint> edges;
    float t = 0;
    float spin = 0;
    uint shape = 0;

    void onEnter() { build(); }

    void edge(uint a, uint b)
    {
        edges.insertLast(a);
        edges.insertLast(b);
    }

    void build()
    {
        points.resize(0);
        edges.resize(0);
        if (shape == 0)
            cube();
        else if (shape == 1)
            ball(10, 16);
        else
            ring(28, 12);
    }

    void cube()
    {
        for (uint i = 0; i < 8; i++)
            points.insertLast(Vec3((i & 1) != 0 ? 1 : -1, (i & 2) != 0 ? 1 : -1, (i & 4) != 0 ? 1 : -1));
        for (uint i = 0; i < 8; i++)
        {
            for (uint bit = 1; bit < 8; bit *= 2)
            {
                if ((i & bit) == 0)
                    edge(i, i | bit);
            }
        }
    }

    void ball(uint rings, uint around)
    {
        for (uint r = 1; r < rings; r++)
        {
            float a = float(r) * 3.14159f / float(rings);
            for (uint s = 0; s < around; s++)
            {
                float b = float(s) * 6.28318f / float(around);
                points.insertLast(Vec3(sin(a) * cos(b), cos(a), sin(a) * sin(b)) * 1.4f);
                uint row = (r - 1) * around;
                edge(row + s, row + (s + 1) % around);
                if (r + 1 < rings)
                    edge(row + s, row + around + s);
            }
        }
    }

    void ring(uint around, uint tube)
    {
        for (uint i = 0; i < around; i++)
        {
            float a = float(i) * 6.28318f / float(around);
            for (uint j = 0; j < tube; j++)
            {
                float b = float(j) * 6.28318f / float(tube);
                float r = 1.2f + 0.45f * cos(b);
                points.insertLast(Vec3(cos(a) * r, sin(b) * 0.45f, sin(a) * r));
                edge(i * tube + j, i * tube + (j + 1) % tube);
                edge(i * tube + j, ((i + 1) % around) * tube + j);
            }
        }
    }

    void update(float dt)
    {
        t += dt;
        Vec2 move = input::move();
        spin += dt * (0.7f + move.x * 3);
        if (input::pressed("accept"))
        {
            shape = (shape + 1) % 3;
            build();
        }
    }

    Vec3 turn(const Vec3 &in p)
    {
        float cy = cos(spin), sy = sin(spin);
        Vec3 q(p.x * cy + p.z * sy, p.y, p.z * cy - p.x * sy);
        float tilt = 0.5f + 0.3f * sin(t * 0.6f);
        float cx = cos(tilt), sx = sin(tilt);
        return Vec3(q.x, q.y * cx - q.z * sx, q.y * sx + q.z * cx);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.02f, 0.04f, 0.08f), Color(0.02f, 0.1f, 0.12f));

        // A floor that runs to the horizon.
        for (int i = -12; i <= 12; i++)
            c.line(640 + float(i) * 30, 470, 640 + float(i) * 260, 720, 1, Color(0.2f, 0.6f, 0.7f, 0.4f));
        for (int i = 0; i < 10; i++)
        {
            float z = (float(i) + (t * 0.6f) % 1) / 10;
            c.rect(0, 470 + z * z * 250, 1280, 1 + z, Color(0.2f, 0.6f, 0.7f, 0.5f * z));
        }

        array<Vec3> seen(points.length());
        for (uint i = 0; i < points.length(); i++)
        {
            Vec3 p = turn(points[i]);
            float depth = 620 / (4 + p.z);
            seen[i] = Vec3(640 + p.x * depth, 320 - p.y * depth, p.z);
        }
        c.setBlend(BlendAdd);
        for (uint i = 0; i < edges.length(); i += 2)
        {
            Vec3 a = seen[edges[i]];
            Vec3 b = seen[edges[i + 1]];
            float near = clamp(0.6f - (a.z + b.z) * 0.2f, 0.12f, 1);
            c.line(a.x, a.y, b.x, b.y, 1.5f + near * 2, fade(hsv(0.45f + a.z * 0.06f, 0.7f, 1), near));
        }
        c.setBlend(BlendAlpha);
        c.text(font, input::usingPad() ? "A  next shape      Left and right  spin" : "Enter  next shape      Left and right  spin", 640, 670, Color(0.7f, 0.85f, 0.9f), AlignCenter);
    }
}
