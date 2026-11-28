#include "bindings/script_refs.h"
#include "screens/screens.h"
#include "ui/ui.h"

#include <scriptarray/scriptarray.h>

namespace nx
{
    namespace
    {
        Ui& ui() { return *services().ui; }

        // --- Widgets. One whose screen has gone does nothing. ---

        Widget* held(Widget* w)
        {
            if (w)
            {
                w->addRef();
            }
            return w;
        }

        Widget* added(Widget* w, WidgetKind kind, const char* style = "")
        {
            Widget* child = w->add(kind, style);
            if (child == nullptr)
            {
                scriptError("this widget's screen has closed");
            }
            return child;
        }

        // An event from a script is the widget's from here on.
        Widget* with(Widget* w, UiEvent event, asIScriptFunction* f)
        {
            if (w && f)
            {
                w->setEvent(event, f);
            }
            else if (f)
            {
                f->Release();
            }
            return w;
        }

        Widget* addColumn(float gap, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Column);
            if (c) c->gap = gap;
            return c;
        }
        Widget* addRow(float gap, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Row);
            if (c) c->gap = gap;
            return c;
        }
        Widget* addGrid(u32 columns, float gap, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Grid);
            if (c)
            {
                c->columns = columns;
                c->gap = gap;
            }
            return c;
        }
        Widget* addStack(Widget* w) { return added(w, WidgetKind::Stack); }
        Widget* addScroll(float gap, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Scroll);
            if (c)
            {
                c->gap = gap;
                c->grow = 1;
            }
            return c;
        }
        Widget* addPanel(Widget* w) { return added(w, WidgetKind::Column, "panel"); }
        Widget* addLabel(const std::string& text, const std::string& style, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Label, style.c_str());
            if (c) c->setText(text.c_str());
            return c;
        }
        Widget* addParagraph(const std::string& text, const std::string& style, Widget* w)
        {
            Widget* c = addLabel(text, style, w);
            if (c) c->wrap = true;
            return c;
        }
        void giveImage(const TextureRef* t, Widget* w)
        {
            if (t)
            {
                t->owner->addRef(t->asset);
            }
            w->setImage(t ? t->asset : nullptr, *services().assets);
        }
        Widget* addImage(const TextureRef* t, float width, float height, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Image);
            if (c)
            {
                giveImage(t, c);
                c->width = width;
                c->height = height;
            }
            return c;
        }
        Widget* addButton(const std::string& text, asIScriptFunction* clicked, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Button);
            if (c) c->setText(text.c_str());
            return with(c, UiEvent::Click, clicked);
        }
        Widget* addToggle(const std::string& text, bool on, asIScriptFunction* changed, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Toggle);
            if (c)
            {
                c->setText(text.c_str());
                c->checked = on;
            }
            return with(c, UiEvent::Change, changed);
        }
        Widget* addSlider(float value, float low, float high, asIScriptFunction* changed, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Slider);
            if (c)
            {
                c->value = value;
                c->low = low;
                c->high = high;
                c->step = (high - low) / 20;
            }
            return with(c, UiEvent::Change, changed);
        }
        Widget* addProgress(float value, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Progress);
            if (c) c->value = value;
            return c;
        }
        void fill(const CScriptArray& from, std::vector<std::string>& out)
        {
            out.clear();
            for (asUINT i = 0; i < from.GetSize(); i++)
            {
                out.push_back(*static_cast<const std::string*>(from.At(i)));
            }
        }
        Widget* addChoice(const CScriptArray& items, int chosen, asIScriptFunction* changed, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Choice);
            if (c)
            {
                fill(items, c->items);
                c->chosen = chosen;
            }
            return with(c, UiEvent::Change, changed);
        }
        Widget* addField(const std::string& text, const std::string& hint, asIScriptFunction* changed, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Field);
            if (c)
            {
                c->setText(text.c_str());
                c->hint = hint;
            }
            return with(c, UiEvent::Change, changed);
        }
        Widget* addList(u32 count, float rowHeight, asIScriptFunction* row, Widget* w)
        {
            Widget* c = added(w, WidgetKind::List);
            if (c)
            {
                c->count = count;
                c->rowHeight = rowHeight > 4 ? rowHeight : 4;
                c->gap = 8;
                c->grow = 1;
                c->columns = 1;
            }
            return with(c, UiEvent::Row, row);
        }
        Widget* addTabsTo(Widget* w)
        {
            Widget* c = w->addTabs();
            if (c == nullptr)
            {
                scriptError("this widget's screen has closed");
            }
            return c;
        }
        Widget* addTabTo(const std::string& title, Widget* w)
        {
            Widget* c = w->addTab(title.c_str());
            if (c == nullptr)
            {
                scriptError("a tab can be added only to tabs");
            }
            return c;
        }
        Widget* addSpace(float size, Widget* w)
        {
            Widget* c = added(w, WidgetKind::Space);
            if (c)
            {
                c->width = c->height = size;
                c->grow = size > 0 ? 0.0f : 1.0f;
            }
            return c;
        }
        Widget* addLine(Widget* w) { return added(w, WidgetKind::Line); }
        Widget* addCustom(asIScriptFunction* drawn, Widget* w) { return with(added(w, WidgetKind::Custom), UiEvent::Draw, drawn); }

        Widget* onClick(asIScriptFunction* f, Widget* w) { return held(with(w, UiEvent::Click, f)); }
        Widget* onChange(asIScriptFunction* f, Widget* w) { return held(with(w, UiEvent::Change, f)); }
        Widget* onFocus(asIScriptFunction* f, Widget* w) { return held(with(w, UiEvent::Focus, f)); }
        Widget* onDone(asIScriptFunction* f, Widget* w) { return held(with(w, UiEvent::Done, f)); }

        std::string getText(const Widget* w) { return w->text; }
        void setText(const std::string& v, Widget* w) { w->setText(v.c_str()); }
        std::string getId(const Widget* w) { return w->id; }
        void setId(const std::string& v, Widget* w) { w->id = v; }
        std::string getHint(const Widget* w) { return w->hint; }
        void setHint(const std::string& v, Widget* w) { w->hint = v; }
        std::string getStyle(const Widget* w) { return w->styleName; }
        void setStyle(const std::string& v, Widget* w)
        {
            snprintf(w->styleName, sizeof(w->styleName), "%s", v.c_str());
            if (w->ui) w->ui->restyle(*w);
        }
        bool getVisible(const Widget* w) { return w->visible; }
        void setVisible(bool v, Widget* w) { if (v != w->visible) { w->visible = v; w->changed(); } }
        bool getEnabled(const Widget* w) { return w->enabled; }
        void setEnabled(bool v, Widget* w) { w->enabled = v; }
        bool getChecked(const Widget* w) { return w->checked; }
        void setChecked(bool v, Widget* w) { w->checked = v; }
        bool getSecret(const Widget* w) { return w->secret; }
        void setSecret(bool v, Widget* w) { w->secret = v; }
        bool getNumeric(const Widget* w) { return w->numeric; }
        void setNumeric(bool v, Widget* w) { w->numeric = v; }
        bool getWrap(const Widget* w) { return w->wrap; }
        void setWrap(bool v, Widget* w) { w->wrap = v; w->changed(); }
        float getValue(const Widget* w) { return w->value; }
        void setValue(float v, Widget* w) { w->value = v; }
        float getLow(const Widget* w) { return w->low; }
        void setLow(float v, Widget* w) { w->low = v; }
        float getHigh(const Widget* w) { return w->high; }
        void setHigh(float v, Widget* w) { w->high = v; }
        float getStep(const Widget* w) { return w->step; }
        void setStep(float v, Widget* w) { w->step = v; }
        int getChosen(const Widget* w) { return w->chosen; }
        void setChosen(int v, Widget* w)
        {
            if (w->ui && w->kind == WidgetKind::Tabs)
            {
                w->ui->showTab(*w, v);
            }
            else if (w->ui && w->kind == WidgetKind::List)
            {
                w->ui->choose(*w, v, false);
            }
            else
            {
                w->chosen = v;
            }
        }
        std::string getChosenText(const Widget* w) { return w->chosen >= 0 && static_cast<size_t>(w->chosen) < w->items.size() ? w->items[static_cast<size_t>(w->chosen)] : ""; }
        void setItems(const CScriptArray& items, Widget* w)
        {
            fill(items, w->items);
            w->chosen = static_cast<size_t>(w->chosen) < w->items.size() ? w->chosen : 0;
            w->changed();
        }
        u32 getCount(const Widget* w) { return w->count; }
        void refresh(Widget* w)
        {
            for (Widget* child : w->children)
            {
                if (w->kind == WidgetKind::List)
                {
                    child->index = -1;
                    child->visible = false;
                }
            }
            w->changed();
        }
        void setCount(u32 v, Widget* w)
        {
            w->count = v;
            w->chosen = w->chosen < static_cast<int>(v) ? w->chosen : v ? static_cast<int>(v) - 1 : 0;
            refresh(w);
        }
        u32 getColumns(const Widget* w) { return w->columns; }
        void setColumns(u32 v, Widget* w) { w->columns = v; w->changed(); }
        u32 getLimit(const Widget* w) { return w->limit; }
        void setLimit(u32 v, Widget* w) { w->limit = v; }
        int getTag(const Widget* w) { return w->tag; }
        void setTag(int v, Widget* w) { w->tag = v; }
        int getIndex(const Widget* w) { return w->index; }
        float getWidth(const Widget* w) { return w->width; }
        void setWidth(float v, Widget* w) { w->width = v; w->changed(); }
        float getHeight(const Widget* w) { return w->height; }
        void setHeight(float v, Widget* w) { w->height = v; w->changed(); }
        float getMinWidth(const Widget* w) { return w->minWidth; }
        void setMinWidth(float v, Widget* w) { w->minWidth = v; w->changed(); }
        float getMaxWidth(const Widget* w) { return w->maxWidth; }
        void setMaxWidth(float v, Widget* w) { w->maxWidth = v; w->changed(); }
        float getColumnWidth(const Widget* w) { return w->columnWidth; }
        void setColumnWidth(float v, Widget* w) { w->columnWidth = v; w->columns = 0; w->changed(); }
        float getRowHeight(const Widget* w) { return w->rowHeight; }
        void setRowHeight(float v, Widget* w) { w->rowHeight = v > 4 ? v : 4; w->changed(); }
        float getGrow(const Widget* w) { return w->grow; }
        void setGrow(float v, Widget* w) { w->grow = v; w->changed(); }
        float getGap(const Widget* w) { return w->space(); }
        void setGap(float v, Widget* w) { w->gap = v; w->changed(); }
        float getPadding(const Widget* w) { return w->pad(0); }
        void setPadding(float v, Widget* w)
        {
            for (float& p : w->padding) p = v;
            w->changed();
        }
        void setPaddings(float left, float top, float right, float bottom, Widget* w)
        {
            w->padding[0] = left;
            w->padding[1] = top;
            w->padding[2] = right;
            w->padding[3] = bottom;
            w->changed();
        }
        // Whole ints, as scripts' enums are.
        int getAlign(const Widget* w) { return static_cast<int>(w->align); }
        void setAlign(UiPlace v, Widget* w) { if (v <= UiPlace::Inherit) { w->align = v; w->changed(); } }
        int getJustify(const Widget* w) { return static_cast<int>(w->justify); }
        void setJustify(UiPlace v, Widget* w) { if (v <= UiPlace::Inherit) { w->justify = v; w->changed(); } }
        int getSelf(const Widget* w) { return static_cast<int>(w->self); }
        void setSelf(UiPlace v, Widget* w) { if (v <= UiPlace::Inherit) { w->self = v; w->changed(); } }
        Color getColor(const Widget* w) { return w->color; }
        void setColor(const Color& v, Widget* w) { w->color = v; }
        float getScroll(const Widget* w) { return w->scroll; }
        void setScroll(float v, Widget* w) { w->scrollTo = v; w->changed(); }
        Vec2 getPlace(const Widget* w)
        {
            Vec2 out;
            if (w->ui) w->ui->place(*w, out.x, out.y);
            return out;
        }
        Vec2 getSize(const Widget* w) { return { w->w, w->h }; }
        bool getFocused(const Widget* w) { return w->ui && w->ui->focused() == w; }
        bool getAlive(const Widget* w) { return w->ui != nullptr; }
        Widget* getParent(Widget* w) { return held(w->parent); }
        u32 childCount(const Widget* w) { return w->children.count(); }
        Widget* childAt(u32 i, Widget* w) { return i < w->children.count() ? held(w->children[i]) : nullptr; }
        void widgetFocus(Widget* w) { if (w->ui) w->ui->focus(w); }
        void widgetPress(Widget* w) { if (w->ui) w->ui->act(*w); }
        void widgetRemove(Widget* w) { w->remove(); }
        void widgetClear(Widget* w) { w->removeAll(); }
        void widgetClose(Widget* w) { if (w->ui) w->ui->close(*w); }

        // --- Pages ---

        Widget* uiRoot() { return held(&ui().root()); }
        void uiDraw(Canvas* c)
        {
            if (c)
            {
                ui().draw(*c, 0, 0, c->width(), c->height());
            }
        }
        void uiDrawIn(Canvas* c, float x, float y, float w, float h)
        {
            if (c)
            {
                ui().draw(*c, x, y, w, h);
            }
        }
        bool report(bool ok)
        {
            if (!ok)
            {
                scriptError(ui().error());
            }
            return ok;
        }
        void uiTheme(const std::string& name) { report(ui().loadTheme(name.c_str())); }
        void uiWords(const std::string& name) { report(ui().words(name.c_str())); }
        std::string uiWord(const std::string& key) { return ui().word(key.c_str()); }
        void uiLoad(const std::string& name, Widget* into)
        {
            Widget& w = into ? *into : ui().root();
            report(ui().load(name.c_str(), w, services().screens->object()));
        }
        Widget* uiFind(const std::string& id) { return held(ui().find(id.c_str())); }
        void uiClear() { ui().root().removeAll(); }
        Widget* uiPopup() { return ui().popup(); }
        void uiDialog(const std::string& title, const std::string& text, const CScriptArray& buttons, asIScriptFunction* answer)
        {
            std::vector<std::string> names;
            fill(buttons, names);
            ui().dialog(title.c_str(), text.c_str(), names, answer);
        }
        void uiToast(const std::string& text, float seconds) { ui().toast(text.c_str(), seconds); }
        Widget* uiFocused() { return held(ui().focused()); }
        bool uiBusy() { return ui().busy(); }
        bool uiTyping() { return ui().typing(); }
        float getSafe() { return ui().safeArea; }
        void setSafe(float v)
        {
            ui().safeArea = v < 0 ? 0 : v > 0.2f ? 0.2f : v;
            ui().touch();
        }
    }

    void bindUi(Binder& b)
    {
        b.enumType("Place", "Where things go in a container, across it or along it.");
        b.enumValue("PlaceStart", static_cast<int>(UiPlace::Start), "At its left, or top.");
        b.enumValue("PlaceCenter", static_cast<int>(UiPlace::Center), "In its middle.");
        b.enumValue("PlaceEnd", static_cast<int>(UiPlace::End), "At its right, or bottom.");
        b.enumValue("PlaceStretch", static_cast<int>(UiPlace::Stretch), "From one side to the other.");
        b.enumValue("PlaceSpread", static_cast<int>(UiPlace::Spread), "With what is left over between them.");
        b.enumValue("PlaceInherit", static_cast<int>(UiPlace::Inherit), "As is usual there.");

        b.refType("Widget", 0, "Something on a page: a button, some text, or a container of others. It belongs to its screen, and once that has closed it does nothing. Each call that adds one gives it back.");
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Widget, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Widget, release));
        b.funcdef("void WidgetEvent(Widget@)", "What to do when something happens to a widget, such as `WidgetEvent(this.onPlay)`.");
        b.funcdef("void WidgetRow(Widget@, int)", "Fills a row of a list: it is given the row, which is empty, and which it is.");
        b.funcdef("void WidgetDraw(Canvas@, Widget@)", "Draws a widget of your own, at its place and size.");
        b.funcdef("void WidgetAnswer(int)", "Is told which of a dialog's buttons was pressed, or -1.");
        b.reopen("Widget");
        b.method("Widget@ column(float gap = 12)", NX_OBJ_LAST(addColumn), "Adds a container of things one under another.");
        b.method("Widget@ row(float gap = 12)", NX_OBJ_LAST(addRow), "One beside another.");
        b.method("Widget@ grid(uint columns, float gap = 12)", NX_OBJ_LAST(addGrid), "In rows of so many, each as wide as the rest.");
        b.method("Widget@ stack()", NX_OBJ_LAST(addStack), "One over another.");
        b.method("Widget@ scroll(float gap = 12)", NX_OBJ_LAST(addScroll), "A column that scrolls when there is more than fits.");
        b.method("Widget@ panel()", NX_OBJ_LAST(addPanel), "A column with something behind it.");
        b.method("Widget@ label(const string &in text, const string &in style = \"plain\")", NX_OBJ_LAST(addLabel), "Text. Anywhere on a page, `@name` is looked up in the words that have been read.");
        b.method("Widget@ paragraph(const string &in text, const string &in style = \"plain\")", NX_OBJ_LAST(addParagraph), "Text that goes on to other lines to fit.");
        b.method("Widget@ image(Texture@+ texture, float width = 0, float height = 0)", NX_OBJ_LAST(addImage), "A picture; 0 is its own size.");
        b.method("Widget@ button(const string &in text, WidgetEvent@ clicked = null)", NX_OBJ_LAST(addButton), "A button. Other widgets may be put into it.");
        b.method("Widget@ toggle(const string &in text, bool on = false, WidgetEvent@ changed = null)", NX_OBJ_LAST(addToggle), "A switch.");
        b.method("Widget@ slider(float value, float low = 0, float high = 1, WidgetEvent@ changed = null)", NX_OBJ_LAST(addSlider), "A value to slide.");
        b.method("Widget@ progress(float value)", NX_OBJ_LAST(addProgress), "How far something has got, 0 to 1.");
        b.method("Widget@ choice(const array<string> &in items, int chosen = 0, WidgetEvent@ changed = null)", NX_OBJ_LAST(addChoice), "One of several: left and right change it, and Accept shows them all.");
        b.method("Widget@ field(const string &in text = \"\", const string &in hint = \"\", WidgetEvent@ changed = null)", NX_OBJ_LAST(addField), "Text to type, with a keyboard or with one on the screen.");
        b.method("Widget@ list(uint count, float rowHeight, WidgetRow@ row)", NX_OBJ_LAST(addList), "Any number of rows: only those that can be seen are made.");
        b.method("Widget@ tabs()", NX_OBJ_LAST(addTabsTo), "Pages, one of which shows. Add them with `tab`.");
        b.method("Widget@ tab(const string &in title)", NX_OBJ_LAST(addTabTo), "Adds a page to tabs: a column.");
        b.method("Widget@ space(float size = 0)", NX_OBJ_LAST(addSpace), "Nothing; 0 takes whatever is left over.");
        b.method("Widget@ line()", NX_OBJ_LAST(addLine), "A line across.");
        b.method("Widget@ custom(WidgetDraw@ drawn)", NX_OBJ_LAST(addCustom), "Whatever you draw: a scene, or a widget of your own.");
        b.method("Widget@ onClick(WidgetEvent@ event)", NX_OBJ_LAST(onClick), "When it is pressed, or a row of a list is. Gives itself back.");
        b.method("Widget@ onChange(WidgetEvent@ event)", NX_OBJ_LAST(onChange), "When its value, choice or text has been changed by someone.");
        b.method("Widget@ onFocus(WidgetEvent@ event)", NX_OBJ_LAST(onFocus), "When it is come to.");
        b.method("Widget@ onDone(WidgetEvent@ event)", NX_OBJ_LAST(onDone), "When typing in a field is over.");
        b.method("string get_text() const property", NX_OBJ_LAST(getText), "What it says, or what has been typed.");
        b.method("void set_text(const string &in) property", NX_OBJ_LAST(setText), "Changes it.");
        b.method("string get_id() const property", NX_OBJ_LAST(getId), "What Ui::find finds it by.");
        b.method("void set_id(const string &in) property", NX_OBJ_LAST(setId), "Names it.");
        b.method("string get_hint() const property", NX_OBJ_LAST(getHint), "What an empty field says.");
        b.method("void set_hint(const string &in) property", NX_OBJ_LAST(setHint), "Changes it.");
        b.method("string get_style() const property", NX_OBJ_LAST(getStyle), "Which of the theme's styles it has.");
        b.method("void set_style(const string &in) property", NX_OBJ_LAST(setStyle), "Gives it another.");
        b.method("bool get_visible() const property", NX_OBJ_LAST(getVisible), "Whether it is there at all.");
        b.method("void set_visible(bool) property", NX_OBJ_LAST(setVisible), "Shows or hides it, and others close up.");
        b.method("bool get_enabled() const property", NX_OBJ_LAST(getEnabled), "Whether it, and what is in it, can be used.");
        b.method("void set_enabled(bool) property", NX_OBJ_LAST(setEnabled), "Lets it be, or not.");
        b.method("bool get_checked() const property", NX_OBJ_LAST(getChecked), "Whether a toggle is on.");
        b.method("void set_checked(bool) property", NX_OBJ_LAST(setChecked), "Turns it on or off.");
        b.method("bool get_secret() const property", NX_OBJ_LAST(getSecret), "Whether what is typed is hidden.");
        b.method("void set_secret(bool) property", NX_OBJ_LAST(setSecret), "Hides it.");
        b.method("bool get_numeric() const property", NX_OBJ_LAST(getNumeric), "Whether only a number can be typed.");
        b.method("void set_numeric(bool) property", NX_OBJ_LAST(setNumeric), "Allows only a number.");
        b.method("bool get_wrap() const property", NX_OBJ_LAST(getWrap), "Whether text goes on to other lines.");
        b.method("void set_wrap(bool) property", NX_OBJ_LAST(setWrap), "Lets it.");
        b.method("float get_value() const property", NX_OBJ_LAST(getValue), "Of a slider, or progress.");
        b.method("void set_value(float) property", NX_OBJ_LAST(setValue), "Moves it.");
        b.method("float get_low() const property", NX_OBJ_LAST(getLow), "The least it can be.");
        b.method("void set_low(float) property", NX_OBJ_LAST(setLow), "Changes it.");
        b.method("float get_high() const property", NX_OBJ_LAST(getHigh), "The most.");
        b.method("void set_high(float) property", NX_OBJ_LAST(setHigh), "Changes it.");
        b.method("float get_step() const property", NX_OBJ_LAST(getStep), "How far a press moves a slider, and what its value is a multiple of. 0: anything.");
        b.method("void set_step(float) property", NX_OBJ_LAST(setStep), "Changes it.");
        b.method("int get_chosen() const property", NX_OBJ_LAST(getChosen), "Which of a choice, tabs or a list.");
        b.method("void set_chosen(int) property", NX_OBJ_LAST(setChosen), "Chooses another.");
        b.method("string get_chosenText() const property", NX_OBJ_LAST(getChosenText), "What a choice's says.");
        b.method("void items(const array<string> &in items)", NX_OBJ_LAST(setItems), "What a choice is of.");
        b.method("uint get_count() const property", NX_OBJ_LAST(getCount), "How many rows a list has.");
        b.method("void set_count(uint) property", NX_OBJ_LAST(setCount), "Changes it, and makes them again.");
        b.method("void refresh()", NX_OBJ_LAST(refresh), "Makes a list's rows again, as what they show has changed.");
        b.method("uint get_columns() const property", NX_OBJ_LAST(getColumns), "Of a grid or a list. 0: as many as fit.");
        b.method("void set_columns(uint) property", NX_OBJ_LAST(setColumns), "Changes it.");
        b.method("float get_columnWidth() const property", NX_OBJ_LAST(getColumnWidth), "How wide they would like to be, when it is as many as fit.");
        b.method("void set_columnWidth(float) property", NX_OBJ_LAST(setColumnWidth), "Makes it as many as fit.");
        b.method("float get_rowHeight() const property", NX_OBJ_LAST(getRowHeight), "Of a list's rows.");
        b.method("void set_rowHeight(float) property", NX_OBJ_LAST(setRowHeight), "Changes it.");
        b.method("uint get_limit() const property", NX_OBJ_LAST(getLimit), "The most characters a field takes.");
        b.method("void set_limit(uint) property", NX_OBJ_LAST(setLimit), "Changes it.");
        b.method("int get_tag() const property", NX_OBJ_LAST(getTag), "A number for your own use.");
        b.method("void set_tag(int) property", NX_OBJ_LAST(setTag), "Changes it.");
        b.method("int get_index() const property", NX_OBJ_LAST(getIndex), "Which row of a list this is, or -1.");
        b.method("float get_width() const property", NX_OBJ_LAST(getWidth), "How wide it must be. 0: as wide as it is given, or needs.");
        b.method("void set_width(float) property", NX_OBJ_LAST(setWidth), "Changes it.");
        b.method("float get_height() const property", NX_OBJ_LAST(getHeight), "How tall it must be, the same way.");
        b.method("void set_height(float) property", NX_OBJ_LAST(setHeight), "Changes it.");
        b.method("float get_minWidth() const property", NX_OBJ_LAST(getMinWidth), "At least this wide.");
        b.method("void set_minWidth(float) property", NX_OBJ_LAST(setMinWidth), "Changes it.");
        b.method("float get_maxWidth() const property", NX_OBJ_LAST(getMaxWidth), "At most this wide. 0: any.");
        b.method("void set_maxWidth(float) property", NX_OBJ_LAST(setMaxWidth), "Changes it.");
        b.method("float get_grow() const property", NX_OBJ_LAST(getGrow), "Its share of what is left over in a row or a column.");
        b.method("void set_grow(float) property", NX_OBJ_LAST(setGrow), "Changes it.");
        b.method("float get_gap() const property", NX_OBJ_LAST(getGap), "Between what is in it.");
        b.method("void set_gap(float) property", NX_OBJ_LAST(setGap), "Changes it.");
        b.method("float get_padding() const property", NX_OBJ_LAST(getPadding), "Between its edges and what is in it.");
        b.method("void set_padding(float) property", NX_OBJ_LAST(setPadding), "Changes it, on every side.");
        b.method("void pad(float left, float top, float right, float bottom)", NX_OBJ_LAST(setPaddings), "Changes it, on each side.");
        b.method("Place get_align() const property", NX_OBJ_LAST(getAlign), "Where what is in it goes, across.");
        b.method("void set_align(Place) property", NX_OBJ_LAST(setAlign), "Changes it.");
        b.method("Place get_justify() const property", NX_OBJ_LAST(getJustify), "And along.");
        b.method("void set_justify(Place) property", NX_OBJ_LAST(setJustify), "Changes it.");
        b.method("Place get_self() const property", NX_OBJ_LAST(getSelf), "Where it goes itself, across what it is in.");
        b.method("void set_self(Place) property", NX_OBJ_LAST(setSelf), "Changes it.");
        b.method("Color get_color() const property", NX_OBJ_LAST(getColor), "Multiplies its picture, or its text.");
        b.method("void set_color(const Color &in) property", NX_OBJ_LAST(setColor), "Changes it.");
        b.method("void set_picture(Texture@+) property", NX_OBJ_LAST(giveImage), "The picture of an image, or in a button.");
        b.method("float get_scrolled() const property", NX_OBJ_LAST(getScroll), "How far down it has scrolled.");
        b.method("void set_scrolled(float) property", NX_OBJ_LAST(setScroll), "Scrolls it, smoothly.");
        b.method("Vec2 get_place() const property", NX_OBJ_LAST(getPlace), "Its top left corner, in design units.");
        b.method("Vec2 get_size() const property", NX_OBJ_LAST(getSize), "How large it has turned out.");
        b.method("bool get_focused() const property", NX_OBJ_LAST(getFocused), "Whether it is the one that Accept would press.");
        b.method("bool get_alive() const property", NX_OBJ_LAST(getAlive), "Whether its screen is still open.");
        b.method("Widget@ get_parent() property", NX_OBJ_LAST(getParent), "What it is in, or null.");
        b.method("uint get_children() const property", NX_OBJ_LAST(childCount), "How many are in it.");
        b.method("Widget@ child(uint index)", NX_OBJ_LAST(childAt), "One of them, or null.");
        b.method("void focus()", NX_OBJ_LAST(widgetFocus), "Goes to it, or to the first in it that can be gone to.");
        b.method("void press()", NX_OBJ_LAST(widgetPress), "As if someone had.");
        b.method("void remove()", NX_OBJ_LAST(widgetRemove), "Takes it off the page.");
        b.method("void clear()", NX_OBJ_LAST(widgetClear), "Takes everything out of it.");
        b.method("void close()", NX_OBJ_LAST(widgetClose), "Closes a popup.");

        b.section("Ui", "Pages of widgets, for gamepad, keyboard and mouse alike. Add to `Ui::root`, and draw it.");
        b.names("Ui");
        b.function("Widget@ get_root() property", NX_FN(uiRoot), "The whole page: a column.");
        b.function("void draw(Canvas@ canvas)", NX_FN(uiDraw), "Draws it over all of the design, with whatever is in front of it.");
        b.function("void draw(Canvas@ canvas, float x, float y, float width, float height)", NX_FN(uiDrawIn), "Over part of it.");
        b.function("void theme(const string &in name)", NX_FN(uiTheme), "Reads how everything looks from a file, over the engine's own. \"\" for only that.");
        b.function("void load(const string &in name, Widget@+ into = null)", NX_FN(uiLoad), "Adds widgets from a file, to the page or to one of it. See the guide.");
        b.function("void words(const string &in name)", NX_FN(uiWords), "Reads what `@name` says, for another language. Anything not in it says its own name.");
        b.function("string word(const string &in name)", NX_FN(uiWord), "One of them.");
        b.function("Widget@ find(const string &in id)", NX_FN(uiFind), "The widget with that id, or null.");
        b.function("void clear()", NX_FN(uiClear), "Takes everything off the page.");
        b.function("Widget@ popup()", NX_FN(uiPopup), "Opens something in front, which must be dealt with first: an empty column to fill. Cancel closes it, or its `close`.");
        b.function("void dialog(const string &in title, const string &in text, const array<string> &in buttons, WidgetAnswer@ answer = null)", NX_FN(uiDialog), "Asks something.");
        b.function("void toast(const string &in text, float seconds = 2.5)", NX_FN(uiToast), "Says something, for a moment.");
        b.function("Widget@ get_focused() property", NX_FN(uiFocused), "What Accept would press, or null.");
        b.function("bool get_busy() property", NX_FN(uiBusy), "Whether something is in front.");
        b.function("bool get_typing() property", NX_FN(uiTyping), "Whether someone is typing. Keys aren't actions then.");
        b.function("float get_safeArea() property", NX_FN(getSafe), "What part of the screen's height is kept clear at its edges, as some televisions hide them.");
        b.function("void set_safeArea(float) property", NX_FN(setSafe), "Changes it.");
        b.names("");
    }
}
