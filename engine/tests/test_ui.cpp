#include "test.h"
#include "rig.h"

#include "ui/ui.h"

using namespace nx;
using namespace nx::test;

namespace
{
    bool openPage(Rig& w, const std::string& body, const char* more = "")
    {
        w.platform.extraPath = "showcase/shapes/main.as";
        w.platform.extraText = std::string("class S : Screen { void note(const string &in k, const string &in v) { Save::set(k, v); } ") + more +
            " void onEnter() { " + body + " } void draw(Canvas@ c) { c.designHeight(720); Ui::draw(c); } }";
        w.screens.init(w.services, "showcase", "main");
        w.step(2);
        w.screens.push("shapes");
        w.step(4);
        if (w.screens.failed())
        {
            NX_ERROR("%s", w.screens.error());
        }
        return !w.screens.failed();
    }

    Widget* named(Rig& w, const char* id) { return w.services.ui->find(id); }
    bool is(Rig& w, const char* key, const char* value) { return strcmp(w.save.get(key, ""), value) == 0; }
    bool near(float a, float b) { return a - b < 0.6f && b - a < 0.6f; }

    void at(Rig& w, const char* id, float& x, float& y)
    {
        Widget* found = named(w, id);
        w.services.ui->place(*found, x, y);
        x += found->w * 0.5f;
        y += found->h * 0.5f;
    }
}

NX_TEST(uiSharesOutRoom)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ p = Ui::root; p.padding = 20; p.gap = 10;"
        "Widget@ r = p.row(10); r.id = \"row\"; Widget@ a = r.space(); a.id = \"a\"; Widget@ b = r.space(); b.id = \"b\"; b.grow = 3; Widget@ f = r.button(\"x\"); f.id = \"f\"; f.width = 100;"
        "Widget@ s = p.scroll(0); s.id = \"scroll\"; for (int i = 0; i < 40; i++) s.button(\"b\");"
        "Widget@ g = p.grid(3, 10); g.id = \"grid\"; for (int i = 0; i < 4; i++) g.button(\"c\");"));
    const Widget* a = named(w, "a"); const Widget* b = named(w, "b"); const Widget* f = named(w, "f"); const Widget* s = named(w, "scroll"); const Widget* g = named(w, "grid");
    NX_EXPECT(near(b->w, a->w * 3) && near(a->w + b->w + f->w + 20, 1240) && near(f->x + f->w, 1240));
    NX_EXPECT(s->contentH > s->h && near(s->y + s->h + 10, g->y) && near(g->y + g->h, 700));
    NX_EXPECT(g->children.count() == 4 && near(g->children[0]->w, (1240 - 20) / 3.0f) && near(g->children[3]->y, g->children[0]->h + 10) && near(g->children[3]->x, 0));
}

NX_TEST(uiFocusGoesWhereItIsPushed)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ p = Ui::root; Widget@ r = p.row(10); r.button(\"1\").id = \"1\"; r.button(\"2\", WidgetEvent(this.hit)).id = \"2\";"
        "Widget@ off = p.button(\"off\"); off.enabled = false; p.toggle(\"t\", false, WidgetEvent(this.flip)).id = \"t\"; p.slider(0.5f, 0, 1, WidgetEvent(this.slid)).id = \"s\";",
        "void hit(Widget@ x) { note(\"hit\", x.id); } void flip(Widget@ x) { note(\"flip\", x.checked ? \"on\" : \"off\"); } void slid(Widget@ x) { note(\"slid\", formatFloat(x.value, \"\", 0, 2)); }"));
    Ui& ui = *w.services.ui;
    NX_EXPECT(ui.focused() == named(w, "1"));
    w.press(Action::Right);
    NX_EXPECT(ui.focused() == named(w, "2"));
    w.press(Action::Accept);
    NX_EXPECT(is(w, "hit", "2"));
    w.press(Action::Down);   // over the one that can't be used
    NX_EXPECT(ui.focused() == named(w, "t"));
    w.press(Action::Accept);
    NX_EXPECT(is(w, "flip", "on"));
    w.press(Action::Down);
    w.press(Action::Right);
    NX_EXPECT(is(w, "slid", "0.55") && ui.focused() == named(w, "s"));
    w.press(Action::Down);   // nothing there
    NX_EXPECT(ui.focused() == named(w, "s"));
}

NX_TEST(uiMouseWorksToo)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ p = Ui::root; p.button(\"1\").id = \"1\"; p.button(\"2\", WidgetEvent(this.hit)).id = \"2\"; p.slider(0, 0, 100).id = \"s\";", "void hit(Widget@ x) { note(\"hit\", x.id); }"));
    float x, y;
    at(w, "2", x, y);
    w.platform.state.hasMouse = true;
    w.platform.state.mouseX = x;
    w.platform.state.mouseY = y;
    w.step();
    NX_EXPECT(w.services.ui->focused() == named(w, "2"));
    w.platform.state.mouseButtons[0] = true;
    w.step(2);
    NX_EXPECT(is(w, "hit", ""));
    w.platform.state.mouseButtons[0] = false;
    w.step(2);
    NX_EXPECT(is(w, "hit", "2"));
    at(w, "s", x, y);
    w.platform.state.mouseX = x;
    w.platform.state.mouseY = y;
    w.platform.state.mouseButtons[0] = true;
    w.step(2);
    NX_EXPECT(near(named(w, "s")->value, 50));
}

NX_TEST(uiListsMakeOnlyWhatIsSeen)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ l = Ui::root.list(5000, 50, WidgetRow(this.fill)); l.id = \"l\"; l.onClick(WidgetEvent(this.hit));",
        "int made = 0; void fill(Widget@ r, int i) { made++; r.label(formatInt(i)); } void hit(Widget@ x) { note(\"hit\", formatInt(x.chosen)); note(\"made\", formatInt(made)); }"));
    Widget* list = named(w, "l");
    NX_EXPECT(list->children.count() > 8 && list->children.count() < 24);
    for (int i = 0; i < 60; i++)
    {
        w.press(Action::Down);
    }
    w.step(60);
    w.press(Action::Accept);
    NX_EXPECT(is(w, "hit", "60") && list->children.count() < 40);
    NX_EXPECT(w.services.ui->focused() && w.services.ui->focused()->index == 60);
    w.services.ui->choose(*list, 4999, false);
    w.step(120);
    w.press(Action::Accept);
    NX_EXPECT(is(w, "hit", "4999") && atoi(w.save.get("made", "0")) < 400);
    list->count = 0;
    list->changed();
    w.step(3);
    NX_EXPECT(!w.screens.failed());
}

NX_TEST(uiDialogsComeFirst)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ p = Ui::root; p.button(\"ask\", WidgetEvent(this.ask)).id = \"ask\"; p.choice(array<string> = { \"a\", \"b\", \"c\" }, 0, WidgetEvent(this.picked)).id = \"c\";",
        "void ask(Widget@ x) { Ui::dialog(\"Sure?\", \"Some text that says why.\", array<string> = { \"No\", \"Yes\" }, WidgetAnswer(this.answer)); } void answer(int i) { note(\"answer\", formatInt(i)); } void picked(Widget@ x) { note(\"picked\", x.chosenText); }"));
    Ui& ui = *w.services.ui;
    w.press(Action::Accept);
    w.step(2);
    NX_EXPECT(ui.busy() && ui.focused() != named(w, "ask"));
    w.press(Action::Right);
    w.press(Action::Accept);
    w.step(2);
    NX_EXPECT(is(w, "answer", "1") && !ui.busy() && ui.focused() == named(w, "ask"));
    w.press(Action::Accept);
    w.step(2);
    w.press(Action::Cancel);
    w.step(2);
    NX_EXPECT(is(w, "answer", "-1") && w.screens.depth() == 2);
    w.press(Action::Down);
    w.press(Action::Accept);
    w.step(2);
    NX_EXPECT(ui.busy());
    w.press(Action::Down);
    w.press(Action::Down);
    w.press(Action::Accept);
    w.step(2);
    NX_EXPECT(is(w, "picked", "c") && !ui.busy());
}

NX_TEST(uiTyping)
{
    Rig w;
    NX_EXPECT(openPage(w, "Widget@ f = Ui::root.field(\"\", \"hint\", WidgetEvent(this.typed)); f.id = \"f\"; f.limit = 5; f.onDone(WidgetEvent(this.done)); Widget@ n = Ui::root.field(); n.id = \"n\"; n.numeric = true;",
        "void typed(Widget@ x) { note(\"text\", x.text); } void done(Widget@ x) { note(\"done\", x.text); }"));
    Ui& ui = *w.services.ui;
    w.press(Action::Accept);
    NX_EXPECT(ui.typing());
    const auto type = [&](const char* text)
    {
        for (const char* p = text; *p; p++)
        {
            w.platform.state.type(static_cast<u8>(*p));
        }
        w.step(2);
    };
    type("cat\xE9sw");   // é, and one too many
    NX_EXPECT(named(w, "f")->text == "cat\xC3\xA9s" && is(w, "text", "cat\xC3\xA9s"));
    type("\b\b");
    NX_EXPECT(named(w, "f")->text == "cat");
    // W is a letter now, not a way to go.
    w.platform.state.keys[static_cast<size_t>(Key::W)] = true;
    w.step(2);
    w.platform.state.keys[static_cast<size_t>(Key::W)] = false;
    NX_EXPECT(ui.focused() == named(w, "f"));
    type("\r");
    NX_EXPECT(!ui.typing() && is(w, "done", "cat"));
    w.press(Action::Down);
    w.press(Action::Accept);
    type("1a.5");
    NX_EXPECT(named(w, "n")->text == "1.5");
    type("\r");

    // And with a gamepad, on the screen.
    ui.keyboard(*named(w, "f"));
    w.step(2);
    NX_EXPECT(ui.busy() && ui.typing() && ui.focused());
    for (const char* key : { "key-1", "key72", "key73" })   // Shift, H, I
    {
        Widget* found = named(w, key);
        NX_EXPECT(found != nullptr);
        if (found)
        {
            ui.act(*found);
        }
        w.step(2);
    }
    NX_EXPECT(named(w, "f")->text == "catHi");
    ui.act(*named(w, "key-2"));
    w.step();
    NX_EXPECT(named(w, "key72") == nullptr);
    w.platform.state.pads[0].connected = true;
    w.platform.state.pads[0].buttons = 1u << static_cast<u32>(Button::B);
    w.step(2);
    NX_EXPECT(!ui.busy() && !ui.typing() && is(w, "done", "catHi"));
}

NX_TEST(uiFilesAreChecked)
{
    Rig w;
    NX_EXPECT(openPage(w, ""));
    Ui& ui = *w.services.ui;
    const auto load = [&](const char* text)
    {
        w.platform.extraPath = "global/page.json";
        w.platform.extraText = text;
        ui.root().removeAll();
        return ui.load("global:page.json", ui.root(), w.screens.object());
    };
    NX_EXPECT(load("{ \"column\": [ { \"label\": \"@hello\", \"id\": \"l\" }, { \"tabs\": [ { \"title\": \"A\", \"page\": [ { \"button\": \"b\", \"id\": \"b\" } ] }, { \"title\": \"B\", \"page\": [] } ] } ], \"grow\": 1 }"));
    NX_EXPECT(named(w, "b") && named(w, "l"));
    w.step(2);
    NX_EXPECT(!load("{ \"column\": [\n{ \"buton\": \"b\" } ] }") && strstr(ui.error(), ":2:"));
    NX_EXPECT(!load("{ \"button\": \"b\",\n \"clik\": \"x\" }") && strstr(ui.error(), "clik"));
    NX_EXPECT(!load("{ \"button\": \"b\", \"click\": \"nothing\" }"));
    NX_EXPECT(!load("{ \"button\": "));
    NX_EXPECT(!ui.load("secret.json", ui.root(), nullptr));
    w.platform.extraText = "{ \"hello\": \"Bonjour\" }";
    NX_EXPECT(ui.words("global:page.json") && strcmp(ui.word("hello"), "Bonjour") == 0 && strcmp(ui.word("other"), "other") == 0);
    for (const char* bad : { "{ \"colors\": { \"accent\": \"#12\" } }", "{ \"styles\": { \"button\": { \"bakc\": \"#ffffff\" } } }", "{ \"styles\": { \"x\": { \"base\": \"nothing\" } } }", "{ \"stiles\": {} }", "{ \"styles\": { \"x\": { \"back\": \"$nothing\" } } }", "{" })
    {
        w.platform.extraText = bad;
        NX_EXPECT(!ui.loadTheme("global:page.json") && ui.error()[0]);
    }
    w.platform.extraText = "{ \"colors\": { \"accent\": \"#ff0000\" } }";
    NX_EXPECT(ui.loadTheme("global:page.json"));
    w.step(2);
    NX_EXPECT(!w.screens.failed());
}

NX_TEST(uiLeavesNothingBehind)
{
    Rig w;
    // Everything holds everything: the screen its widgets, and they its methods.
    const char* more = "Widget@ kept; array<Widget@> all; void hit(Widget@ x) {} void fill(Widget@ r, int i) { r.label(\"r\"); all.insertLast(r); } void draw2(Canvas@ c, Widget@ x) {}";
    NX_EXPECT(openPage(w, "@kept = Ui::root.button(\"b\", WidgetEvent(this.hit)); kept.onFocus(WidgetEvent(this.hit)); Ui::root.list(50, 40, WidgetRow(this.fill)); Ui::root.custom(WidgetDraw(this.draw2)).height = 20;"
        "Widget@ p = Ui::popup(); p.button(\"x\", WidgetEvent(this.hit)); all.insertLast(p); Ui::dialog(\"a\", \"b\", array<string> = { \"c\" }); Widget@ loose = Ui::root.label(\"l\"); all.insertLast(loose); loose.remove(); kept.press();", more));
    w.step(5);
    const size_t during = memStats(MemTag::UI).count;
    w.screens.pop();
    w.step(3);
    const size_t after = memStats(MemTag::UI).count;
    NX_EXPECT(!w.screens.failed() && after < during);
    for (int i = 0; i < 3; i++)
    {
        w.screens.push("shapes");
        w.step(5);
        w.screens.pop();
        w.step(3);
    }
    NX_EXPECT(memStats(MemTag::UI).count == after);
}

NX_TEST(uiScriptsMayPullTheRugOut)
{
    Rig w;
    NX_EXPECT(openPage(w, "Ui::root.button(\"b\", WidgetEvent(this.hit)); Ui::root.list(50, 40, WidgetRow(this.fill));",
        "int rows = 0; void hit(Widget@ x) { Ui::clear(); x.text = \"gone\"; x.focus(); Ui::root.button(\"new\").id = \"new\"; } void fill(Widget@ r, int i) { r.label(\"r\"); rows++; if (rows == 5) { r.parent.remove(); r.clear(); } }"));
    w.step(3);
    w.press(Action::Accept);
    w.step(3);
    NX_EXPECT(!w.screens.failed() && named(w, "new"));
    NX_EXPECT(w.services.ui->focused() == named(w, "new"));
}
