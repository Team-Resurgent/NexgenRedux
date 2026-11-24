// The cursor holds four runes. Turn them one way or the other; four or more of a colour that touch
// vanish, the rest fall, and what that joins vanishes too. Each turn costs a move, and each step of a
// chain after the first gives one back. Reach the target before the moves run out.

const int Size = 8;
const float Cell = 72;
const float Left = 60;
const float Top = 72;

class Rune
{
    int kind;
    Tween x, y, size;

    Rune(int kind_, float column, float row)
    {
        kind = kind_;
        x = Tween(column);
        y = Tween(row);
        size = Tween(1);
    }
}

class RunesScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Font@ big = assets::font("global:freesans.sfn", 52);
    Sound@ move = assets::sound("global:sound/move.wav");
    Sound@ turn = assets::sound("global:sound/whoosh.wav");
    Sound@ pop = assets::sound("global:sound/pop.wav");
    Sound@ bell = assets::sound("global:sound/bell.wav");
    Sound@ win = assets::sound("global:sound/win.wav");
    Sound@ lose = assets::sound("global:sound/lose.wav");

    array<Rune@> runes(Size * Size);
    int column = 3, row = 3;
    Tween cursorX(3), cursorY(3);
    int score = 0, target = 60, level = 1, moves = 20, best = 0;
    bool busy = true;
    bool over = false;
    string banner;
    Tween bannerIn;
    array<bool> marked(Size * Size);

    void onEnter()
    {
        best = int(parseInt(save::get("runes.best", "0")));
        start(Coroutine(this.begin));
    }

    void say(const string &in text)
    {
        banner = text;
        bannerIn = Tween(0, 1, 0.4f, EaseOvershoot);
    }

    int kinds() { return level < 3 ? 4 : 5; }

    void begin()
    {
        busy = true;
        for (int i = 0; i < Size * Size; i++)
            @runes[i] = null;
        // Filled so that nothing is joined yet.
        for (int i = 0; i < Size * Size; i++)
        {
            Rune@ r = Rune(0, float(i % Size), -2);
            do
            {
                r.kind = randomInt(0, kinds() - 1);
                @runes[i] = r;
            }
            while (group(i % Size, i / Size, false) >= 3);
            r.y = Tween(float(i / Size) - 9, float(i / Size), 0.5f + float(i / Size) * 0.03f, EaseBounce);
        }
        say("Level " + formatInt(level));
        wait(0.9f);
        banner = "";
        busy = false;
    }

    // How many runes of one colour touch this one. Marks them if asked.
    int group(int column, int row, bool mark)
    {
        array<bool> seen(Size * Size);
        array<int> open = { row * Size + column };
        int kind = runes[open[0]].kind;
        seen[open[0]] = true;
        int count = 0;
        while (open.length() > 0)
        {
            int at = open[open.length() - 1];
            open.removeLast();
            count++;
            if (mark) marked[at] = true;
            for (int d = 0; d < 4; d++)
            {
                int c = at % Size + (d == 0 ? 1 : d == 1 ? -1 : 0);
                int r = at / Size + (d == 2 ? 1 : d == 3 ? -1 : 0);
                if (c < 0 || r < 0 || c >= Size || r >= Size) continue;
                int next = r * Size + c;
                if (!seen[next] && runes[next] !is null && runes[next].kind == kind)
                {
                    seen[next] = true;
                    open.insertLast(next);
                }
            }
        }
        return count;
    }

    void rotate(bool clockwise)
    {
        array<int> cells = { row * Size + column, row * Size + column + 1, (row + 1) * Size + column + 1, (row + 1) * Size + column };
        array<Rune@> old;
        for (uint i = 0; i < 4; i++) old.insertLast(runes[cells[i]]);
        for (uint i = 0; i < 4; i++)
        {
            int cell = cells[(i + (clockwise ? 1 : 3)) % 4];
            @runes[cell] = old[i];
            old[i].x.go(float(cell % Size), 0.16f, EaseOut);
            old[i].y.go(float(cell / Size), 0.16f, EaseOut);
        }
        turn.play(0.5f, 0, clockwise ? 1.2f : 1.0f);
        moves--;
        start(Coroutine(this.settle));
    }

    void settle()
    {
        busy = true;
        wait(0.18f);
        int chain = 0;
        while (true)
        {
            int joined = 0;
            for (int i = 0; i < Size * Size; i++) marked[i] = false;
            for (int i = 0; i < Size * Size; i++)
                if (!marked[i] && group(i % Size, i / Size, false) >= 4)
                    group(i % Size, i / Size, true);
            for (int i = 0; i < Size * Size; i++)
                if (marked[i])
                {
                    joined++;
                    runes[i].size.go(0, 0.22f, EaseIn);
                }
            if (joined == 0) break;

            chain++;
            score += joined * chain;
            if (chain > 1)
            {
                moves++;
                say("Chain of " + formatInt(chain) + "!");
                bell.play(0.4f, 0, 0.8f + float(chain) * 0.15f);
            }
            for (int i = 0; i < joined && i < 6; i++)
            {
                pop.play(0.5f, 0, 0.7f + float(chain) * 0.12f + float(i) * 0.06f);
                wait(0.035f);
            }
            wait(0.1f);
            drop();
            wait(0.42f);
        }
        banner = "";
        if (score >= target)
        {
            win.play(0.8f);
            say("Target reached");
            wait(1.3f);
            level++;
            target += 60 + level * 15;
            moves += 12;
            start(Coroutine(this.begin));
            return;
        }
        if (moves <= 0)
        {
            over = true;
            lose.play(0.8f);
            if (score > best)
            {
                best = score;
                save::set("runes.best", formatInt(best));
                say("New best: " + formatInt(best));
            }
            else
                say("Out of moves");
            while (!input::pressed("accept")) nextUpdate();
            over = false;
            score = 0;
            level = 1;
            target = 60;
            moves = 20;
            start(Coroutine(this.begin));
            return;
        }
        busy = false;
    }

    void drop()
    {
        for (int c = 0; c < Size; c++)
        {
            int to = Size - 1;
            for (int r = Size - 1; r >= 0; r--)
            {
                Rune@ rune = runes[r * Size + c];
                if (marked[r * Size + c]) continue;
                @runes[to * Size + c] = rune;
                if (to != r) rune.y.go(float(to), 0.36f, EaseBounce);
                to--;
            }
            for (int fresh = 1; to >= 0; to--, fresh++)
            {
                Rune@ rune = Rune(randomInt(0, kinds() - 1), float(c), -float(fresh));
                @runes[to * Size + c] = rune;
                rune.y.go(float(to), 0.36f, EaseBounce);
            }
        }
    }

    void update(float dt)
    {
        int c = column, r = row;
        if (input::repeated("left") && column > 0) column--;
        if (input::repeated("right") && column < Size - 2) column++;
        if (input::repeated("up") && row > 0) row--;
        if (input::repeated("down") && row < Size - 2) row++;
        if (c != column || r != row)
        {
            move.play(0.3f);
            cursorX.go(float(column), 0.08f, EaseOut);
            cursorY.go(float(row), 0.08f, EaseOut);
        }
        if (!busy)
        {
            if (input::pressed("accept")) rotate(true);
            else if (input::pressed("alternate") || input::pressed("cancel")) rotate(false);
        }
    }

    Color colorOf(int kind) { return hsv(float(kind) * 0.2f + 0.02f, 0.62f, 0.98f); }

    void drawRune(Canvas@ c, Rune@ r)
    {
        float s = r.size.value;
        if (s <= 0.01f) return;
        c.push();
        c.translate(Left + (r.x.value + 0.5f) * Cell, Top + (r.y.value + 0.5f) * Cell);
        c.scale(s, s);
        Color color = colorOf(r.kind);
        c.roundRect(-31, -31, 62, 62, 16, Color(color.r * 0.45f, color.g * 0.45f, color.b * 0.45f));
        c.roundRect(-29, -31, 58, 56, 14, color);
        Color ink = Color(1, 1, 1, 0.9f);
        // A mark for each colour, so they can be told apart without it.
        if (r.kind == 0) c.circle(0, -3, 13, ink);
        else if (r.kind == 1) c.ring(0, -3, 15, 6, ink);
        else if (r.kind == 2) c.triangle(Vec2(0, -18), Vec2(15, 9), Vec2(-15, 9), ink, ink, ink);
        else if (r.kind == 3) c.roundRect(-12, -15, 24, 24, 4, ink);
        else { c.rect(-4, -18, 8, 30, ink); c.rect(-15, -7, 30, 8, ink); }
        c.pop();
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.06f, 0.1f, 0.16f), Color(0.1f, 0.05f, 0.13f));
        c.roundRect(Left - 14, Top - 14, Cell * Size + 28, Cell * Size + 28, 20, Color(0, 0, 0, 0.4f));
        c.push();
        c.clip(Left - 8, Top - 8, Cell * Size + 16, Cell * Size + 16);
        for (uint i = 0; i < runes.length(); i++)
            if (runes[i] !is null) drawRune(c, runes[i]);
        c.pop();

        float pulse = 4 + sin(app::time() * 6);
        float x = Left + cursorX.value * Cell, y = Top + cursorY.value * Cell;
        c.rectLine(x - 2, y - 2, Cell * 2 + 4, Cell * 2 + 4, pulse, busy ? Gray : White);

        c.text(font, "Score", 730, 70, Gray);
        c.text(big, formatInt(score) + " / " + formatInt(target), 730, 96, White);
        c.roundRect(730, 170, 480, 14, 7, Color(0.15f, 0.18f, 0.25f));
        float w = 480 * clamp(float(score) / float(target), 0, 1);
        c.roundRect(730, 170, w < 14 ? 14 : w, 14, 7, Green);
        c.text(font, "Moves", 730, 220, Gray);
        c.text(big, formatInt(moves), 730, 246, moves < 5 ? Red : White);
        c.text(font, "Level", 930, 220, Gray);
        c.text(big, formatInt(level), 930, 246, White);
        c.text(font, "Best", 1080, 220, Gray);
        c.text(big, formatInt(best), 1080, 246, White);
        c.paragraph(font, "Turn four runes at a time. Four or more of one kind that touch will vanish; chains score more and give moves back.", 730, 350, 480, Color(0.75f, 0.8f, 0.9f));
        c.text(font, input::usingPad() ? "A  turn right     X  turn left" : "Enter  turn right     Shift  turn left", 730, 500, Yellow);
        if (banner != "")
        {
            c.push();
            c.translate(970, 590);
            c.scale(bannerIn.value, bannerIn.value);
            c.text(big, banner, 0, -30, Yellow, AlignCenter);
            c.pop();
        }
        if (over) c.text(font, "Accept to play again", 970, 640, White, AlignCenter);
    }
}
