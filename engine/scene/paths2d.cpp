#include "scene/world.h"

#include <math.h>
#include <stdlib.h>

// Finding a way on the plane, round solid tiles: A* over the grid of the last map, then straightened wherever a
// straight line is clear.
namespace nx
{
    namespace
    {
        constexpr u32 kUnseen = 0xFFFFFFFFu;
        constexpr u32 kStraight = 10, kAcross = 14;    // a diagonal step costs about root two of a straight one
        constexpr u32 kMostCells = 1u << 20;
    }

    bool World::blocked(int x, int y) const
    {
        if (m_mapCount == 0)
        {
            return true;
        }
        const TileMap& grid = m_maps[m_mapCount - 1];
        if (x < 0 || y < 0 || static_cast<u32>(x) >= grid.width || static_cast<u32>(y) >= grid.height)
        {
            return true;
        }
        // Where the cell's middle is in each map, which may be another size or elsewhere.
        const Vec2 middle(grid.origin.x + (static_cast<float>(x) + 0.5f) * grid.size, grid.origin.y + (static_cast<float>(y) + 0.5f) * grid.size);
        for (u32 m = 0; m < m_mapCount; m++)
        {
            const TileMap& map = m_maps[m];
            const float fx = floorf((middle.x - map.origin.x) / map.size), fy = floorf((middle.y - map.origin.y) / map.size);
            if (fx < 0 || fy < 0 || fx >= static_cast<float>(map.width) || fy >= static_cast<float>(map.height))
            {
                continue;
            }
            const u16 tile = map.tiles[static_cast<size_t>(fy) * map.width + static_cast<size_t>(fx)];
            if (tile && tile < 64 && map.solid[tile] == 1)
            {
                return true;
            }
        }
        return false;
    }

    bool World::blockedAt(Vec2 place) const
    {
        if (m_mapCount == 0)
        {
            return false;
        }
        const TileMap& grid = m_maps[m_mapCount - 1];
        return blocked(static_cast<int>(floorf((place.x - grid.origin.x) / grid.size)), static_cast<int>(floorf((place.y - grid.origin.y) / grid.size)));
    }

    bool World::findPath(Vec2 from, Vec2 to, bool diagonal, GrowArray<Vec2>& way)
    {
        way.clear();
        if (m_mapCount == 0)
        {
            return false;
        }
        const TileMap& grid = m_maps[m_mapCount - 1];
        const u32 w = grid.width, h = grid.height;
        if (w == 0 || h == 0 || w * h > kMostCells)
        {
            return false;
        }
        const auto cellOf = [&](Vec2 p, int& x, int& y)
        {
            x = static_cast<int>(floorf((p.x - grid.origin.x) / grid.size));
            y = static_cast<int>(floorf((p.y - grid.origin.y) / grid.size));
        };
        int sx, sy, gx, gy;
        cellOf(from, sx, sy);
        cellOf(to, gx, gy);
        if (blocked(sx, sy) || blocked(gx, gy))
        {
            return false;
        }
        const u32 cells = w * h;
        if (!m_pathCost.reserve(cells) || !m_pathFrom.reserve(cells))
        {
            return false;
        }
        m_pathCost.resize(cells);
        m_pathFrom.resize(cells);
        for (u32 i = 0; i < cells; i++)
        {
            m_pathCost[i] = kUnseen;
        }
        const u32 start = static_cast<u32>(sy) * w + static_cast<u32>(sx), goal = static_cast<u32>(gy) * w + static_cast<u32>(gx);
        const auto guess = [&](u32 cell)
        {
            const u32 dx = static_cast<u32>(abs(static_cast<int>(cell % w) - gx)), dy = static_cast<u32>(abs(static_cast<int>(cell / w) - gy));
            return diagonal ? kStraight * (dx > dy ? dx : dy) + (kAcross - kStraight) * (dx < dy ? dx : dy) : kStraight * (dx + dy);
        };
        // The open cells, a heap by the cost so far and a guess of what is left. A cell may be in it more than once,
        // from when it was reached more dearly; those are passed over.
        m_pathOpen.clear();
        GrowArray<u32>& open = m_pathOpen;
        GrowArray<u32> scores;
        const auto swap = [&](u32 a, u32 b)
        {
            const u32 c = open[a], s = scores[a];
            open[a] = open[b];
            scores[a] = scores[b];
            open[b] = c;
            scores[b] = s;
        };
        const auto add = [&](u32 cell)
        {
            if (!open.push(cell) || !scores.push(m_pathCost[cell] + guess(cell)))
            {
                return false;
            }
            for (u32 at = open.count() - 1; at > 0 && scores[(at - 1) / 2] > scores[at]; at = (at - 1) / 2)
            {
                swap(at, (at - 1) / 2);
            }
            return true;
        };
        const auto take = [&]()
        {
            const u32 cell = open[0], score = scores[0];
            open[0] = open[open.count() - 1];
            scores[0] = scores[scores.count() - 1];
            open.resize(open.count() - 1);
            scores.resize(scores.count() - 1);
            for (u32 at = 0;;)
            {
                const u32 l = at * 2 + 1, r = l + 1;
                u32 least = at;
                least = l < open.count() && scores[l] < scores[least] ? l : least;
                least = r < open.count() && scores[r] < scores[least] ? r : least;
                if (least == at)
                {
                    break;
                }
                swap(at, least);
                at = least;
            }
            return score == m_pathCost[cell] + guess(cell) ? cell : kUnseen;   // stale if it was reached for less since
        };
        m_pathCost[start] = 0;
        m_pathFrom[start] = start;
        if (!add(start))
        {
            return false;
        }
        bool found = false;
        while (open.count())
        {
            const u32 cell = take();
            if (cell == kUnseen)
            {
                continue;
            }
            if (cell == goal)
            {
                found = true;
                break;
            }
            const int cx = static_cast<int>(cell % w), cy = static_cast<int>(cell / w);
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    const bool across = dx && dy;
                    if ((!dx && !dy) || (across && !diagonal))
                    {
                        continue;
                    }
                    // Not into a solid tile, nor round the corner of one.
                    if (blocked(cx + dx, cy + dy) || (across && (blocked(cx + dx, cy) || blocked(cx, cy + dy))))
                    {
                        continue;
                    }
                    const u32 next = static_cast<u32>(cy + dy) * w + static_cast<u32>(cx + dx);
                    const u32 cost = m_pathCost[cell] + (across ? kAcross : kStraight);
                    if (cost < m_pathCost[next])
                    {
                        m_pathCost[next] = cost;
                        m_pathFrom[next] = cell;
                        if (!add(next))
                        {
                            return false;
                        }
                    }
                }
            }
        }
        if (!found)
        {
            return false;
        }

        // Back from the goal, then straightened: from each corner, as far on as a clear straight line reaches.
        m_pathCells.clear();
        for (u32 cell = goal;; cell = m_pathFrom[cell])
        {
            m_pathCells.push(cell);
            if (cell == start)
            {
                break;
            }
        }
        const auto middleOf = [&](u32 cell)
        {
            return Vec2(grid.origin.x + (static_cast<float>(cell % w) + 0.5f) * grid.size, grid.origin.y + (static_cast<float>(cell / w) + 0.5f) * grid.size);
        };
        // Clear if every cell that a line, and a little either side of it, passes over is open.
        const auto clear = [&](Vec2 a, Vec2 b)
        {
            const Vec2 d = b - a;
            const float far = sqrtf(d.x * d.x + d.y * d.y);
            const Vec2 side = far > 0 ? Vec2(-d.y / far, d.x / far) * (grid.size * 0.45f) : Vec2();
            const u32 steps = static_cast<u32>(far / (grid.size * 0.25f)) + 1;
            for (u32 i = 0; i <= steps; i++)
            {
                const Vec2 p = a + d * (static_cast<float>(i) / static_cast<float>(steps));
                const Vec2 points[3] = { p, p + side, p - side };
                for (const Vec2& q : points)
                {
                    int x, y;
                    cellOf(q, x, y);
                    if (blocked(x, y))
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        Vec2 at = from;
        u32 i = m_pathCells.count() - 1;   // the start
        while (i > 0)
        {
            u32 reach = i - 1;
            for (u32 k = i - 1;; k--)
            {
                const Vec2 there = k == 0 ? to : middleOf(m_pathCells[k]);
                if (clear(at, there))
                {
                    reach = k;
                }
                if (k == 0)
                {
                    break;
                }
            }
            at = reach == 0 ? to : middleOf(m_pathCells[reach]);
            if (!way.push(at))
            {
                return false;
            }
            i = reach;
        }
        if (way.count() == 0)
        {
            way.push(to);
        }
        return true;
    }
}
