#include "scene/space.h"
#include "scene/world.h"
#include "assets/assets.h"
#include "assets/model.h"

#include <math.h>
#include <string.h>

// Grass over the land near the camera, and pictures that stand upright and turn to face it: impostors, which are
// pictures of far things made from their models, and sprites, which are pictures a script gives.
namespace nx
{
    namespace
    {
        constexpr float kCell = 8;              // metres, of a square of grass
        constexpr u32 kCardQuads = 1024;        // in one batch
        constexpr u32 kViews = 4;               // of an impostor, round it
        constexpr u32 kViewWide = 64, kViewHigh = 128, kFine = 2;  // pixels of each, and drawn larger then made smaller

        u32 mixBits(u32 h)
        {
            h ^= h >> 16;
            h *= 0x7feb352du;
            h ^= h >> 15;
            h *= 0x846ca68bu;
            return h ^ (h >> 16);
        }

        // Random numbers that are the same each time for the same square.
        struct Dice
        {
            u32 state;
            float next()
            {
                state = mixBits(state + 0x9e3779b9u);
                return static_cast<float>(state >> 8) / 16777216.0f;
            }
        };

        float clampFloat(float v, float low, float high) { return v < low ? low : v > high ? high : v; }

        u8 byte(float v) { return static_cast<u8>(clampFloat(v, 0, 1) * 255 + 0.5f); }
    }

    // --- Grass ---

    void Space::setGrass(Asset* tuft, float perSquareMetre, float distance)
    {
        if (tuft)
        {
            m_assets.addRef(tuft);
        }
        if (m_grassTuft)
        {
            m_assets.release(m_grassTuft);
        }
        m_grassTuft = tuft;
        m_grassDensity = perSquareMetre < 0 ? 0 : perSquareMetre > 12 ? 12 : perSquareMetre;
        m_grassFar = distance < 4 ? 4 : distance > 120 ? 120 : distance;
        // Made again, as they are now asked for.
        gpu::Device* device = m_solids ? m_solids->device() : nullptr;
        for (GrassCell& c : m_grass)
        {
            if (device)
            {
                device->destroyMesh(c.mesh);
            }
        }
        m_grass.clear();
    }

    void Space::grassChanged(float fromX, float fromZ, float toX, float toZ)
    {
        for (GrassCell& c : m_grass)
        {
            const float x = static_cast<float>(c.x) * kCell, z = static_cast<float>(c.z) * kCell;
            if (x + kCell >= fromX && x <= toX && z + kCell >= fromZ && z <= toZ)
            {
                c.tier = 255;
            }
        }
    }

    gpu::Color Space::groundColor(float x, float z) const
    {
        const Vec3 n = normalAt(x, z);
        gpu::Color c = landColor(heightAt(x, z), n, x, z);
        const int ix = static_cast<int>(floorf(x / m_landSpacing + 0.5f)), iz = static_cast<int>(floorf(z / m_landSpacing + 0.5f));
        if (const u8* paint = painted(ix, iz))
        {
            const float t = static_cast<float>(paint[3]) / 255;
            c.r += (static_cast<float>(paint[0]) / 255 - c.r) * t;
            c.g += (static_cast<float>(paint[1]) / 255 - c.g) * t;
            c.b += (static_cast<float>(paint[2]) / 255 - c.b) * t;
        }
        return c;
    }

    // Tufts where the land is green, flat enough and dry, each turned and sized at random, in one mesh.
    bool Space::buildGrass(GrassCell& cell)
    {
        gpu::Device& device = *m_solids->device();
        device.destroyMesh(cell.mesh);
        cell.mesh = {};
        cell.indices = 0;
        const ModelData* model = m_grassTuft && m_grassTuft->ready ? m_grassTuft->model : nullptr;
        if (model == nullptr || !m_landOn)
        {
            // Again later, once the tuft has come.
            cell.tier = 255;
            return false;
        }
        const u32 detail = cell.tier ? (kModelDetails > 1 ? 1 : 0) : 0;
        Dice dice = { mixBits(static_cast<u32>(cell.x) * 73856093u ^ static_cast<u32>(cell.z) * 19349663u) };
        const u32 tries = static_cast<u32>(m_grassDensity * kCell * kCell * (cell.tier ? 0.45f : 1.0f));
        const gpu::Color middle = m_landColors[1];
        MeshData d;
        cell.low = 1e9f;
        cell.high = -1e9f;
        for (u32 i = 0; i < tries; i++)
        {
            const float lx = dice.next() * kCell, lz = dice.next() * kCell;
            const float turn = dice.next() * 6.2832f, size = 0.55f + dice.next() * 0.5f, keep = dice.next();
            const float wx = static_cast<float>(cell.x) * kCell + lx, wz = static_cast<float>(cell.z) * kCell + lz;
            const float y = heightAt(wx, wz);
            if (y < -9e5f || (m_hasWater && y < m_water + 0.1f) || normalAt(wx, wz).y < 0.82f)
            {
                continue;
            }
            // Only where the land is green: not sand, rock, snow or a painted path.
            const gpu::Color ground = groundColor(wx, wz);
            if (keep > clampFloat((ground.g - fmaxf(ground.r, ground.b)) * 10, 0, 1))
            {
                continue;
            }
            // Coloured as the land is there, so that it grows out of it.
            const float tr = clampFloat(0.55f + 0.45f * ground.r / fmaxf(middle.r, 0.05f), 0.4f, 1.6f), tg = clampFloat(0.55f + 0.45f * ground.g / fmaxf(middle.g, 0.05f), 0.4f, 1.6f),
                        tb = clampFloat(0.55f + 0.45f * ground.b / fmaxf(middle.b, 0.05f), 0.4f, 1.6f);
            const float c = cosf(turn) * size, s = sinf(turn) * size;
            const u32 base = d.vertices.count();
            bool full = false;
            for (const ModelPart& part : model->parts)
            {
                const u32 first = part.first[detail], count = part.count[detail];
                for (u32 k = 0; k < count && !full; k++)
                {
                    const gpu::MeshVertex& v = model->mesh.vertices[model->mesh.indices[first + k]];
                    const Vec3 at = { v.x * c - v.z * s + lx, v.y * size + y, v.x * s + v.z * c + lz };
                    gpu::MeshVertex out = meshVertex(at, { v.nx, v.ny, v.nz },
                        { static_cast<float>(v.r) / 255 * part.color.r * tr, static_cast<float>(v.g) / 255 * part.color.g * tg, static_cast<float>(v.b) / 255 * part.color.b * tb, 1 }, 0,
                        fmaxf(v.y, 0) * size);
                    full = !d.vertices.push(out) || !d.indices.push(static_cast<u16>(d.vertices.count() - 1)) || d.vertices.count() > 65000;
                    cell.low = fminf(cell.low, at.y);
                    cell.high = fmaxf(cell.high, at.y);
                }
            }
            if (full)
            {
                d.vertices.resize(base);
                break;
            }
        }
        if (d.indices.count() > d.vertices.count())
        {
            d.indices.resize(d.vertices.count());
        }
        if (d.vertices.count() == 0)
        {
            return true;
        }
        cell.mesh = device.createMesh(d.vertices.begin(), d.vertices.count(), d.indices.begin(), d.indices.count(), false);
        cell.indices = cell.mesh ? d.indices.count() : 0;
        return static_cast<bool>(cell.mesh);
    }

    void Space::drawGrass(const gpu::View& view, const Frustum& frustum)
    {
        if (!m_grassTuft || m_grassDensity <= 0 || !m_landOn || !m_solids)
        {
            return;
        }
        gpu::Device& device = *m_solids->device();
        for (GrassCell& c : m_grass)
        {
            c.keep = false;
        }
        // Those near enough, nearest made first; a few a frame, so that moving on doesn't stall.
        const int reach = static_cast<int>(ceilf(m_grassFar / kCell));
        const int mx = static_cast<int>(floorf(m_from.x / kCell)), mz = static_cast<int>(floorf(m_from.z / kCell));
        u32 made = 0;
        const u32 most = m_grass.count() == 0 ? 64 : 3;
        for (int ring = 0; ring <= reach; ring++)
        {
            for (int z = mz - ring; z <= mz + ring; z++)
            {
                for (int x = mx - ring; x <= mx + ring; x++)
                {
                    if (abs(x - mx) != ring && abs(z - mz) != ring)
                    {
                        continue;
                    }
                    const float dx = (static_cast<float>(x) + 0.5f) * kCell - m_from.x, dz = (static_cast<float>(z) + 0.5f) * kCell - m_from.z;
                    const float far = sqrtf(dx * dx + dz * dz);
                    if (far > m_grassFar + kCell * 0.7f)
                    {
                        continue;
                    }
                    const u8 tier = far > m_grassFar * 0.55f ? 1 : 0;
                    GrassCell* found = nullptr;
                    for (GrassCell& c : m_grass)
                    {
                        if (c.x == x && c.z == z)
                        {
                            found = &c;
                            break;
                        }
                    }
                    if (found == nullptr)
                    {
                        if (made >= most)
                        {
                            continue;
                        }
                        GrassCell fresh = {};
                        fresh.x = x;
                        fresh.z = z;
                        fresh.tier = tier;
                        if (!m_grass.push(fresh))
                        {
                            continue;
                        }
                        found = &m_grass[m_grass.count() - 1];
                        buildGrass(*found);
                        made++;
                    }
                    else if (found->tier != tier && made < most)
                    {
                        found->tier = tier;
                        buildGrass(*found);
                        made++;
                    }
                    found->keep = true;
                }
            }
        }
        for (u32 i = 0; i < m_grass.count();)
        {
            if (!m_grass[i].keep)
            {
                device.destroyMesh(m_grass[i].mesh);
                m_grass.swapRemove(i);
                continue;
            }
            i++;
        }
        for (const GrassCell& c : m_grass)
        {
            if (!c.mesh)
            {
                continue;
            }
            const float half = kCell * 0.5f;
            const Vec3 middle = { static_cast<float>(c.x) * kCell + half, (c.low + c.high) * 0.5f, static_cast<float>(c.z) * kCell + half };
            if (!frustum.sees(middle, half * 1.42f + (c.high - c.low) * 0.5f + 1))
            {
                continue;
            }
            gpu::MeshDraw d;
            d.mesh = c.mesh;
            d.count = c.indices;
            d.world = Mat4::translation({ static_cast<float>(c.x) * kCell, 0, static_cast<float>(c.z) * kCell });
            d.cull = gpu::Cull::None;
            d.bendX = m_wind.x * 0.15f;
            d.bendZ = m_wind.z * 0.15f;
            (void)view;
            device.drawMesh(d);
        }
    }

    // --- Pictures that face the camera ---

    void Space::setImpostor(u32 id, bool on)
    {
        if (Appearance* a = appearance(id))
        {
            a->impostor = on;
        }
    }

    void Space::setSprite(u32 id, Asset* sheet, u32 angles, u32 frames)
    {
        if (Appearance* a = appearance(id))
        {
            if (sheet)
            {
                m_assets.addRef(sheet);
            }
            if (a->sprite)
            {
                m_assets.release(a->sprite);
            }
            a->sprite = sheet;
            a->spriteAngles = static_cast<u8>(angles < 1 ? 1 : angles > 16 ? 16 : angles);
            a->spriteFrames = static_cast<u8>(frames < 1 ? 1 : frames > 64 ? 64 : frames);
            a->spriteFrame = 0;
        }
    }

    void Space::setSpriteFrame(u32 id, u32 frame)
    {
        if (Appearance* a = appearance(id))
        {
            a->spriteFrame = static_cast<u16>(a->spriteFrames ? frame % a->spriteFrames : 0);
        }
    }

    // A picture of a model from four sides, drawn here rather than on the GPU, so that it is made the same way
    // everywhere: larger, then made smaller for soft edges. Colours spread past the edges, so that what is clear
    // doesn't bleed dark into what is drawn.
    const Space::Impostor* Space::impostorOf(Asset* asset)
    {
        for (const Impostor& i : m_impostorList)
        {
            if (i.model == asset)
            {
                return i.texture ? &i : nullptr;
            }
        }
        const ModelData* model = asset->model;
        Impostor made = {};
        made.model = asset;
        m_assets.addRef(asset);
        if (model && !model->weights.count() && model->high.y > model->low.y)
        {
            float half = 0.01f;
            for (const gpu::MeshVertex& v : model->mesh.vertices)
            {
                half = fmaxf(half, sqrtf(v.x * v.x + v.z * v.z));
            }
            made.half = half;
            made.low = model->low.y;
            made.high = model->high.y;
            const u32 fw = kViewWide * kFine, fh = kViewHigh * kFine;
            const u32 wide = kViewWide * kViews;
            GrowArray<u8> pixels;
            GrowArray<float> fine;   // r, g, b, a, depth
            if (pixels.reserve(wide * kViewHigh * 4) && fine.reserve(fw * fh * 5))
            {
                pixels.resize(wide * kViewHigh * 4);
                fine.resize(fw * fh * 5);
                for (u32 view = 0; view < kViews; view++)
                {
                    const float a = static_cast<float>(view) * 6.2832f / kViews;
                    const Vec3 right = { cosf(a), 0, -sinf(a) }, toward = { sinf(a), 0, cosf(a) };
                    const Vec3 light = (toward * 0.5f + Vec3(0, 0.85f, 0) + right * 0.35f).normalized();
                    for (u32 i = 0; i < fw * fh; i++)
                    {
                        fine[i * 5 + 0] = fine[i * 5 + 1] = fine[i * 5 + 2] = fine[i * 5 + 3] = 0;
                        fine[i * 5 + 4] = -1e9f;
                    }
                    for (const ModelPart& part : model->parts)
                    {
                        if (part.see)
                        {
                            continue;
                        }
                        for (u32 t = 0; t + 2 < part.count[0]; t += 3)
                        {
                            float sx[3], sy[3], sz[3];
                            float r = 0, g = 0, b = 0;
                            Vec3 n;
                            for (u32 k = 0; k < 3; k++)
                            {
                                const gpu::MeshVertex& v = model->mesh.vertices[model->mesh.indices[part.first[0] + t + k]];
                                const Vec3 p = { v.x, v.y, v.z };
                                sx[k] = (p.dot(right) / (2 * half) + 0.5f) * static_cast<float>(fw);
                                sy[k] = (1 - (p.y - made.low) / (made.high - made.low)) * static_cast<float>(fh);
                                sz[k] = p.dot(toward);
                                r += static_cast<float>(v.r) / 765;
                                g += static_cast<float>(v.g) / 765;
                                b += static_cast<float>(v.b) / 765;
                                n = n + Vec3(v.nx, v.ny, v.nz);
                            }
                            const float facing = n.dot(n) > 0 ? n.normalized().dot(light) : 0;
                            const float shade = 0.56f + 0.46f * (part.twoSided ? fabsf(facing) : fmaxf(facing, 0));
                            r *= part.color.r * shade;
                            g *= part.color.g * shade;
                            b *= part.color.b * shade;
                            const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
                            if (fabsf(area) < 1e-6f)
                            {
                                continue;
                            }
                            const int x0 = static_cast<int>(fmaxf(0, floorf(fminf(sx[0], fminf(sx[1], sx[2]))))), x1 = static_cast<int>(fminf(static_cast<float>(fw - 1), ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])))));
                            const int y0 = static_cast<int>(fmaxf(0, floorf(fminf(sy[0], fminf(sy[1], sy[2]))))), y1 = static_cast<int>(fminf(static_cast<float>(fh - 1), ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])))));
                            for (int y = y0; y <= y1; y++)
                            {
                                for (int x = x0; x <= x1; x++)
                                {
                                    const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
                                    const float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                                    const float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                                    const float w2 = 1 - w0 - w1;
                                    if (w0 < 0 || w1 < 0 || w2 < 0)
                                    {
                                        continue;
                                    }
                                    const float depth = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                                    float* at = &fine[(static_cast<u32>(y) * fw + static_cast<u32>(x)) * 5];
                                    if (depth > at[4])
                                    {
                                        at[0] = r;
                                        at[1] = g;
                                        at[2] = b;
                                        at[3] = 1;
                                        at[4] = depth;
                                    }
                                }
                            }
                        }
                    }
                    // Smaller, with how much of each pixel was covered as its alpha.
                    for (u32 y = 0; y < kViewHigh; y++)
                    {
                        for (u32 x = 0; x < kViewWide; x++)
                        {
                            float sum[4] = {};
                            for (u32 j = 0; j < kFine * kFine; j++)
                            {
                                const float* f = &fine[((y * kFine + j / kFine) * fw + x * kFine + j % kFine) * 5];
                                sum[0] += f[0] * f[3];
                                sum[1] += f[1] * f[3];
                                sum[2] += f[2] * f[3];
                                sum[3] += f[3];
                            }
                            u8* out = &pixels[(y * wide + view * kViewWide + x) * 4];
                            const float cover = sum[3] / (kFine * kFine);
                            out[0] = byte(sum[3] > 0 ? sum[0] / sum[3] : 0);
                            out[1] = byte(sum[3] > 0 ? sum[1] / sum[3] : 0);
                            out[2] = byte(sum[3] > 0 ? sum[2] / sum[3] : 0);
                            out[3] = byte(cover);
                        }
                    }
                }
                // Colour into the clear pixels next to drawn ones, a few times over.
                for (u32 pass = 0; pass < 4; pass++)
                {
                    for (u32 y = 0; y < kViewHigh; y++)
                    {
                        for (u32 x = 0; x < wide; x++)
                        {
                            u8* p = &pixels[(y * wide + x) * 4];
                            if (p[3] != 0 || (p[0] | p[1] | p[2]))
                            {
                                continue;
                            }
                            static const int around[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                            for (const auto& o : around)
                            {
                                const int nx = static_cast<int>(x) + o[0], ny = static_cast<int>(y) + o[1];
                                if (nx < 0 || ny < 0 || nx >= static_cast<int>(wide) || ny >= static_cast<int>(kViewHigh))
                                {
                                    continue;
                                }
                                const u8* q = &pixels[(static_cast<u32>(ny) * wide + static_cast<u32>(nx)) * 4];
                                if (q[0] | q[1] | q[2])
                                {
                                    p[0] = q[0];
                                    p[1] = q[1];
                                    p[2] = q[2];
                                    break;
                                }
                            }
                        }
                    }
                }
                gpu::TextureData data;
                data.width = data.fullWidth = wide;
                data.height = data.fullHeight = kViewHigh;
                data.pixels = pixels.begin();
                data.mips = true;
                made.texture = m_solids->device()->createTexture(data);
            }
        }
        if (!m_impostorList.push(made))
        {
            m_assets.release(asset);
            return nullptr;
        }
        return made.texture ? &m_impostorList[m_impostorList.count() - 1] : nullptr;
    }

    // A picture of it, standing where it stands and turned to the camera, into the batch of its texture. False if
    // it is to be drawn as it is.
    bool Space::card(const Appearance& a, const Mat4& world, const gpu::View& view)
    {
        gpu::Texture texture;
        float half = 0.5f, low = 0, high = 1;
        u32 columns = 1, rows = 1, column = 0, row = 0;
        float uMax = 1, vMax = 1;
        Vec3 normalLean;
        const Vec3 place = { world.m[12], world.m[13], world.m[14] };
        Vec3 look = m_from - place;
        look.y = 0;
        const float far = sqrtf(look.dot(look));
        look = far > 1e-4f ? look * (1 / far) : Vec3(0, 0, 1);
        const Vec3 axisX = { world.m[0], world.m[1], world.m[2] }, axisY = { world.m[4], world.m[5], world.m[6] }, axisZ = { world.m[8], world.m[9], world.m[10] };
        const float sizeX = sqrtf(axisX.dot(axisX)), sizeY = sqrtf(axisY.dot(axisY));
        // Seen from which side of it: 0 in front, going round to its left.
        const float side = atan2f(look.dot(axisX), look.dot(axisZ));
        if (a.sprite)
        {
            if (!a.sprite->ready || a.sprite->failed)
            {
                return true;
            }
            texture = a.sprite->texture;
            const gpu::TextureInfo info = m_solids->device()->textureInfo(texture);
            uMax = info.uMax;
            vMax = info.vMax;
            columns = a.spriteAngles;
            rows = a.spriteFrames;
            row = a.spriteFrame;
            half = 0.5f;
            normalLean = look;
        }
        else
        {
            if (!a.impostor || m_impostorFar <= 0 || !a.model || !a.model->ready || far < m_impostorFar)
            {
                return false;
            }
            const Impostor* imp = impostorOf(a.model);
            if (imp == nullptr)
            {
                return false;
            }
            texture = imp->texture;
            columns = kViews;
            half = imp->half;
            low = imp->low;
            high = imp->high;
            normalLean = (look * 0.35f + Vec3(0, 1, 0)).normalized();
        }
        const float step = 6.2832f / static_cast<float>(columns);
        column = static_cast<u32>(static_cast<int>(floorf((side < 0 ? side + 6.2832f : side) / step + 0.5f)) % static_cast<int>(columns));
        if (m_batches.count() == 0 || m_cards.count() >= kCardQuads * 8)
        {
            if (m_cards.count() >= kCardQuads * 8)
            {
                return true;
            }
        }
        u32 batch = m_batches.count();
        for (u32 i = 0; i < m_batches.count(); i++)
        {
            if (m_batches[i].texture == texture && m_batches[i].glow == a.glow)
            {
                batch = i;
                break;
            }
        }
        if (batch == m_batches.count())
        {
            CardBatch fresh = {};
            fresh.texture = texture;
            fresh.glow = a.glow;
            if (!m_batches.push(fresh))
            {
                return true;
            }
        }
        m_batches[batch].used = true;
        const Vec3 right = Vec3(look.z, 0, -look.x) * (half * sizeX);
        const Vec3 bottom = place + Vec3(0, low * sizeY, 0), top = place + Vec3(0, high * sizeY, 0);
        const float u0 = static_cast<float>(column) / static_cast<float>(columns) * uMax, u1 = static_cast<float>(column + 1) / static_cast<float>(columns) * uMax;
        const float v0 = static_cast<float>(row) / static_cast<float>(rows) * vMax, v1 = static_cast<float>(row + 1) / static_cast<float>(rows) * vMax;
        Card c;
        c.batch = batch;
        c.v[0] = meshVertex(bottom - right, normalLean, a.color, u0, v1);
        c.v[1] = meshVertex(bottom + right, normalLean, a.color, u1, v1);
        c.v[2] = meshVertex(top + right, normalLean, a.color, u1, v0);
        c.v[3] = meshVertex(top - right, normalLean, a.color, u0, v0);
        (void)view;
        m_cards.push(c);
        return true;
    }

    void Space::drawCards(const gpu::View& view)
    {
        gpu::Device& device = *m_solids->device();
        m_cardsDrawn = m_cards.count();
        GrowArray<gpu::MeshVertex> vertices;
        for (u32 b = 0; b < m_batches.count(); b++)
        {
            CardBatch& batch = m_batches[b];
            vertices.clear();
            for (const Card& c : m_cards)
            {
                if (c.batch == b && vertices.count() < kCardQuads * 4)
                {
                    for (const gpu::MeshVertex& v : c.v)
                    {
                        vertices.push(v);
                    }
                }
            }
            if (vertices.count() == 0)
            {
                continue;
            }
            if (!batch.mesh)
            {
                GrowArray<u16> indices;
                GrowArray<gpu::MeshVertex> empty;
                if (!indices.reserve(kCardQuads * 6) || !empty.reserve(kCardQuads * 4))
                {
                    continue;
                }
                for (u32 q = 0; q < kCardQuads; q++)
                {
                    const u16 at = static_cast<u16>(q * 4);
                    const u16 six[6] = { at, static_cast<u16>(at + 1), static_cast<u16>(at + 2), at, static_cast<u16>(at + 2), static_cast<u16>(at + 3) };
                    for (u16 i : six)
                    {
                        indices.push(i);
                    }
                }
                empty.resize(kCardQuads * 4);
                memset(empty.begin(), 0, sizeof(gpu::MeshVertex) * kCardQuads * 4);
                batch.mesh = device.createMesh(empty.begin(), kCardQuads * 4, indices.begin(), kCardQuads * 6, true);
                if (!batch.mesh)
                {
                    continue;
                }
            }
            device.updateMesh(batch.mesh, vertices.begin(), vertices.count());
            gpu::MeshDraw d;
            d.mesh = batch.mesh;
            d.count = vertices.count() / 4 * 6;
            d.texture = batch.texture;
            d.glow = batch.glow;
            d.cull = gpu::Cull::None;
            d.cutout = 0.5f;
            (void)view;
            device.drawMesh(d);
        }
        m_cards.clear();
        // Batches that weren't needed go.
        for (u32 b = 0; b < m_batches.count();)
        {
            if (!m_batches[b].used)
            {
                device.destroyMesh(m_batches[b].mesh);
                m_batches.swapRemove(b);
                continue;
            }
            m_batches[b].used = false;
            b++;
        }
    }

    void Space::freeCards()
    {
        gpu::Device* device = m_solids ? m_solids->device() : nullptr;
        for (GrassCell& c : m_grass)
        {
            if (device)
            {
                device->destroyMesh(c.mesh);
            }
        }
        m_grass.clear();
        if (m_grassTuft)
        {
            m_assets.release(m_grassTuft);
            m_grassTuft = nullptr;
        }
        for (Impostor& i : m_impostorList)
        {
            if (device && i.texture)
            {
                device->destroyTexture(i.texture);
            }
            m_assets.release(i.model);
        }
        m_impostorList.clear();
        for (CardBatch& b : m_batches)
        {
            if (device)
            {
                device->destroyMesh(b.mesh);
            }
        }
        m_batches.clear();
        m_cards.clear();
    }
}
