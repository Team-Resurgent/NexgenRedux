#include "base/log.h"
#include "gpu/gl/gl_window.h"
#include "platform/linux/icon.h"
#include "platform/posix/platform_posix.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

// Last, because they define a great many plain words.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <GL/glx.h>
#undef None

namespace nx
{
    namespace
    {
        Key translateKey(KeySym symbol)
        {
            if (symbol >= XK_a && symbol <= XK_z)
            {
                return static_cast<Key>(static_cast<u32>(Key::A) + static_cast<u32>(symbol - XK_a));
            }
            if (symbol >= XK_0 && symbol <= XK_9)
            {
                return static_cast<Key>(static_cast<u32>(Key::Num0) + static_cast<u32>(symbol - XK_0));
            }
            switch (symbol)
            {
            case XK_Up: return Key::Up;
            case XK_Down: return Key::Down;
            case XK_Left: return Key::Left;
            case XK_Right: return Key::Right;
            case XK_space: return Key::Space;
            case XK_Return:
            case XK_KP_Enter: return Key::Enter;
            case XK_Escape: return Key::Escape;
            case XK_Tab: return Key::Tab;
            case XK_BackSpace: return Key::Backspace;
            case XK_Shift_L:
            case XK_Shift_R: return Key::Shift;
            case XK_Control_L:
            case XK_Control_R: return Key::Control;
            default: return Key::None;
            }
        }

        // The kernel's own gamepad interface, which says which button is which.
        struct EvdevPad
        {
            int file = -1;
            char path[300] = {};
            i32 low[ABS_CNT] = {};
            i32 high[ABS_CNT] = {};
            float axes[ABS_CNT] = {};
            u32 buttons = 0;
            bool rumbles = false;   // it has motors, and may be written to
            int effect = -1;        // the rumble given to it, which is changed rather than made again

            bool open(const char* device)
            {
                // Written to as well, for rumble, where that is allowed.
                file = ::open(device, O_RDWR | O_NONBLOCK | O_CLOEXEC);
                const bool writable = file >= 0;
                if (file < 0)
                {
                    file = ::open(device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                }
                if (file < 0)
                {
                    return false;
                }
                u8 forces[FF_CNT / 8 + 1] = {};
                rumbles = writable && ioctl(file, EVIOCGBIT(EV_FF, sizeof(forces)), forces) >= 0 && (forces[FF_RUMBLE / 8] & (1 << (FF_RUMBLE % 8)));
                u8 keys[KEY_CNT / 8 + 1] = {};
                ioctl(file, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
                if (!(keys[BTN_GAMEPAD / 8] & (1 << (BTN_GAMEPAD % 8))))
                {
                    close();
                    return false;
                }
                for (u32 i = 0; i < ABS_CNT; i++)
                {
                    input_absinfo info = {};
                    if (ioctl(file, EVIOCGABS(i), &info) >= 0)
                    {
                        low[i] = info.minimum;
                        high[i] = info.maximum;
                        axes[i] = scale(i, info.value);
                    }
                }
                snprintf(path, sizeof(path), "%s", device);
                return true;
            }

            void close()
            {
                if (file >= 0)
                {
                    ::close(file);
                }
                *this = EvdevPad();
            }

            void rumble(float heavy, float light)
            {
                if (file < 0 || !rumbles)
                {
                    return;
                }
                ff_effect e = {};
                e.type = FF_RUMBLE;
                e.id = static_cast<i16>(effect);
                e.u.rumble.strong_magnitude = static_cast<u16>(heavy * 65535);
                e.u.rumble.weak_magnitude = static_cast<u16>(light * 65535);
                e.replay.length = 0;  // until it is stopped
                if (ioctl(file, EVIOCSFF, &e) < 0)
                {
                    return;
                }
                effect = e.id;
                input_event play = {};
                play.type = EV_FF;
                play.code = static_cast<u16>(effect);
                play.value = heavy > 0 || light > 0 ? 1 : 0;
                if (::write(file, &play, sizeof(play)) < 0)
                {
                    rumbles = false;
                }
            }

            // -1 to 1 across the axis's range.
            float scale(u32 axis, i32 value) const
            {
                const float range = static_cast<float>(high[axis]) - static_cast<float>(low[axis]);
                return range > 0 ? (static_cast<float>(value) - static_cast<float>(low[axis])) / range * 2 - 1 : 0;
            }

            void read()
            {
                static const struct { u16 code; Button button; } map[] = {
                    { BTN_SOUTH, Button::A }, { BTN_EAST, Button::B }, { BTN_NORTH, Button::X }, { BTN_WEST, Button::Y },
                    { BTN_TL, Button::LeftShoulder }, { BTN_TR, Button::RightShoulder }, { BTN_START, Button::Start }, { BTN_SELECT, Button::Back },
                    { BTN_THUMBL, Button::LeftStick }, { BTN_THUMBR, Button::RightStick },
                    { BTN_DPAD_UP, Button::Up }, { BTN_DPAD_DOWN, Button::Down }, { BTN_DPAD_LEFT, Button::Left }, { BTN_DPAD_RIGHT, Button::Right },
                };
                input_event event;
                ssize_t got = 0;
                while ((got = ::read(file, &event, sizeof(event))) == static_cast<ssize_t>(sizeof(event)))
                {
                    if (event.type == EV_ABS && event.code < ABS_CNT)
                    {
                        axes[event.code] = scale(event.code, event.value);
                    }
                    else if (event.type == EV_KEY)
                    {
                        for (const auto& m : map)
                        {
                            if (m.code == event.code)
                            {
                                const u32 bit = 1u << static_cast<u32>(m.button);
                                buttons = event.value ? buttons | bit : buttons & ~bit;
                            }
                        }
                    }
                }
                if (got == 0 || (got < 0 && errno != EAGAIN && errno != EINTR))
                {
                    close();  // unplugged
                }
            }

            Gamepad state() const
            {
                Gamepad pad;
                pad.connected = true;
                pad.buttons = buttons;
                const auto set = [&](bool down, Button b) { pad.buttons |= down ? 1u << static_cast<u32>(b) : 0; };
                set(axes[ABS_HAT0X] < -0.5f, Button::Left);
                set(axes[ABS_HAT0X] > 0.5f, Button::Right);
                set(axes[ABS_HAT0Y] < -0.5f, Button::Up);
                set(axes[ABS_HAT0Y] > 0.5f, Button::Down);
                pad.leftX = removeDeadZone(axes[ABS_X]);
                pad.leftY = removeDeadZone(-axes[ABS_Y]);
                pad.rightX = removeDeadZone(axes[ABS_RX]);
                pad.rightY = removeDeadZone(-axes[ABS_RY]);
                pad.leftTrigger = high[ABS_Z] ? (axes[ABS_Z] + 1) / 2 : 0;
                pad.rightTrigger = high[ABS_RZ] ? (axes[ABS_RZ] + 1) / 2 : 0;
                return pad;
            }
        };

        class PlatformLinux final : public PlatformPosix
        {
        public:
            ~PlatformLinux() override
            {
                for (EvdevPad& pad : m_pads)
                {
                    pad.close();
                }
                if (m_display == nullptr)
                {
                    return;
                }
                if (m_context)
                {
                    glXMakeCurrent(m_display, 0, nullptr);
                    glXDestroyContext(m_display, m_context);
                }
                if (m_window)
                {
                    XDestroyWindow(m_display, m_window);
                }
                if (m_colormap)
                {
                    XFreeColormap(m_display, m_colormap);
                }
                XCloseDisplay(m_display);
            }

            bool init()
            {
                char base[kMaxPath] = {};
                const ssize_t length = readlink("/proc/self/exe", base, sizeof(base) - 8);
                if (length <= 0)
                {
                    NX_ERROR("cannot find the program's folder");
                    return false;
                }
                base[length] = 0;
                if (char* slash = strrchr(base, '/'))
                {
                    snprintf(slash, 8, "/media");
                }
                char saves[kMaxPath];
                const char* data = getenv("XDG_DATA_HOME");
                const char* home = getenv("HOME");
                snprintf(saves, sizeof(saves), "%s%s/nexgen", data && *data ? data : home ? home : "/tmp", data && *data ? "" : "/.local/share");
                return setFolders(base, saves);
            }

            bool openWindow(const WindowDesc& desc) override
            {
                m_display = XOpenDisplay(nullptr);
                if (m_display == nullptr)
                {
                    NX_ERROR("cannot open the X display");
                    return false;
                }
                const int screen = DefaultScreen(m_display);
                static const int attributes[] = {
                    GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                    GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DOUBLEBUFFER, True, 0
                };
                int count = 0;
                GLXFBConfig* configs = glXChooseFBConfig(m_display, screen, attributes, &count);
                XVisualInfo* visual = configs && count ? glXGetVisualFromFBConfig(m_display, configs[0]) : nullptr;
                if (visual == nullptr)
                {
                    NX_ERROR("the X server has no suitable visual");
                    if (configs)
                    {
                        XFree(configs);
                    }
                    return false;
                }
                const GLXFBConfig config = configs[0];
                const Window root = RootWindow(m_display, screen);
                m_colormap = XCreateColormap(m_display, root, visual->visual, AllocNone);
                XSetWindowAttributes set = {};
                set.colormap = m_colormap;
                set.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask;
                m_width = desc.width;
                m_height = desc.height;
                m_unseen = desc.hidden;
                m_window = XCreateWindow(m_display, root, 0, 0, desc.width, desc.height, 0, visual->depth, InputOutput, visual->visual, CWColormap | CWEventMask, &set);
                XFree(visual);
                XFree(configs);
                XStoreName(m_display, m_window, desc.title);
                // Its icon: the size, then the pixels, each in a long as the property has them.
                {
                    static unsigned long icon[2 + kIconSide * kIconSide];
                    icon[0] = icon[1] = kIconSide;
                    for (u32 i = 0; i < kIconSide * kIconSide; i++)
                    {
                        icon[2 + i] = kIconPixels[i];
                    }
                    const Atom property = XInternAtom(m_display, "_NET_WM_ICON", False);
                    XChangeProperty(m_display, m_window, property, XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(icon), 2 + kIconSide * kIconSide);
                }
                m_closeMessage = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
                XSetWMProtocols(m_display, m_window, &m_closeMessage, 1);
                // Files may be dropped on it, as XDND has it.
                for (u32 i = 0; i < kDropAtoms; i++)
                {
                    static const char* const names[kDropAtoms] = { "XdndAware", "XdndPosition", "XdndStatus", "XdndDrop", "XdndFinished", "XdndSelection", "XdndActionCopy", "text/uri-list", "NEXGEN_DROP" };
                    m_drop[i] = XInternAtom(m_display, names[i], False);
                }
                const Atom version = 5;
                XChangeProperty(m_display, m_window, m_drop[DropAware], XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&version), 1);
                if (!desc.hidden)
                {
                    XMapWindow(m_display, m_window);
                }
                return createContext(config);
            }

            void* nativeWindow() override { return m_context ? &m_gl : nullptr; }

            bool pumpEvents() override
            {
                while (m_display && XPending(m_display))
                {
                    XEvent event;
                    XNextEvent(m_display, &event);
                    handle(event);
                }
                readPads();
                return !m_quit;
            }

            const InputState& input() const override { return m_input; }
            const char* name() const override { return "Linux"; }

        private:
            bool createContext(GLXFBConfig config)
            {
                using Create = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
                const Create create = reinterpret_cast<Create>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
                // Core profile, 3.3.
                static const int attributes[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x0001, 0 };
                // An old driver answers with an error, which would otherwise end the program.
                int (*before)(Display*, XErrorEvent*) = XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
                m_context = create ? create(m_display, config, nullptr, True, attributes) : nullptr;
                XSync(m_display, False);
                XSetErrorHandler(before);
                if (m_context == nullptr || !glXMakeCurrent(m_display, m_window, m_context))
                {
                    NX_ERROR("OpenGL 3.3 is needed");
                    return false;
                }
                using Interval = void (*)(Display*, GLXDrawable, int);
                const Interval interval = reinterpret_cast<Interval>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
                const char* extensions = glXQueryExtensionsString(m_display, DefaultScreen(m_display));
                if (interval && extensions && strstr(extensions, "GLX_EXT_swap_control"))
                {
                    interval(m_display, m_window, 1);
                    m_interval = reinterpret_cast<void*>(interval);
                }
                m_gl.wait = [](void* self, bool on)
                {
                    PlatformLinux* p = static_cast<PlatformLinux*>(self);
                    if (p->m_interval)
                    {
                        reinterpret_cast<void (*)(Display*, GLXDrawable, int)>(p->m_interval)(p->m_display, p->m_window, on ? 1 : 0);
                    }
                };
                m_gl.user = this;
                m_gl.load = [](const char* function) { return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(function))); };
                m_gl.size = [](void* self, u32& width, u32& height)
                {
                    width = static_cast<PlatformLinux*>(self)->m_width;
                    height = static_cast<PlatformLinux*>(self)->m_height;
                };
                m_gl.swap = [](void* self)
                {
                    PlatformLinux& p = *static_cast<PlatformLinux*>(self);
                    glXSwapBuffers(p.m_display, p.m_window);
                };
                return true;
            }

            void handle(XEvent& event)
            {
                switch (event.type)
                {
                case SelectionNotify:
                    takeDrop(event.xselection);
                    break;
                case ClientMessage:
                    if (event.xclient.message_type == m_drop[DropPosition])
                    {
                        // Anywhere will do.
                        const long accept[5] = { static_cast<long>(m_window), 1, 0, 0, static_cast<long>(m_drop[DropCopy]) };
                        sendDrop(static_cast<Window>(event.xclient.data.l[0]), DropStatus, accept);
                        break;
                    }
                    if (event.xclient.message_type == m_drop[DropDrop])
                    {
                        m_dropFrom = static_cast<Window>(event.xclient.data.l[0]);
                        XConvertSelection(m_display, m_drop[DropSelection], m_drop[DropList], m_drop[DropHere], m_window, static_cast<Time>(event.xclient.data.l[2]));
                        break;
                    }
                    m_quit = m_quit || static_cast<Atom>(event.xclient.data.l[0]) == m_closeMessage;
                    break;
                case ConfigureNotify:
                    m_width = static_cast<u32>(event.xconfigure.width);
                    m_height = static_cast<u32>(event.xconfigure.height);
                    break;
                case FocusIn:
                    m_focused = true;
                    break;
                case FocusOut:
                    // Key-ups go elsewhere from here on, so nothing may stay held.
                    m_focused = false;
                    memset(m_input.keys, 0, sizeof(m_input.keys));
                    memset(m_input.mouseButtons, 0, sizeof(m_input.mouseButtons));
                    break;
                case KeyRelease:
                    // A held key repeats as a release and a press at the same moment: that isn't a release.
                    if (XPending(m_display))
                    {
                        XEvent next;
                        XPeekEvent(m_display, &next);
                        if (next.type == KeyPress && next.xkey.keycode == event.xkey.keycode && next.xkey.time == event.xkey.time)
                        {
                            break;
                        }
                    }
                    [[fallthrough]];
                case KeyPress:
                    // Unshifted, so a letter is the same key with Shift held.
                    m_input.keys[static_cast<size_t>(translateKey(XLookupKeysym(&event.xkey, 0)))] = event.type == KeyPress;
                    m_input.keys[static_cast<size_t>(Key::None)] = false;
                    if (event.type == KeyPress)
                    {
                        // Latin-1, as it comes without an input method.
                        char text[8];
                        const int count = XLookupString(&event.xkey, text, sizeof(text), nullptr, nullptr);
                        for (int i = 0; i < count; i++)
                        {
                            m_input.type(static_cast<u8>(text[i]));
                        }
                    }
                    break;
                case MotionNotify:
                {
                    // Where it was put back to, which isn't a move.
                    const int cx = static_cast<int>(m_width / 2), cy = static_cast<int>(m_height / 2);
                    if (m_locked && event.xmotion.x == cx && event.xmotion.y == cy)
                    {
                        break;
                    }
                    if (m_seenMouse)
                    {
                        m_input.turnX += static_cast<float>(event.xmotion.x) - m_input.mouseX;
                        m_input.turnY += static_cast<float>(event.xmotion.y) - m_input.mouseY;
                    }
                    m_seenMouse = true;
                    if (m_locked)
                    {
                        XWarpPointer(m_display, 0, m_window, 0, 0, 0, 0, cx, cy);
                        m_input.mouseX = static_cast<float>(cx);
                        m_input.mouseY = static_cast<float>(cy);
                        m_input.hasMouse = true;
                        break;
                    }
                }
                    m_input.hasMouse = true;
                    m_input.mouseX = static_cast<float>(event.xmotion.x);
                    m_input.mouseY = static_cast<float>(event.xmotion.y);
                    break;
                case ButtonPress:
                case ButtonRelease:
                {
                    const bool down = event.type == ButtonPress;
                    switch (event.xbutton.button)
                    {
                    case Button1: m_input.mouseButtons[0] = down; break;
                    case Button3: m_input.mouseButtons[1] = down; break;
                    case Button2: m_input.mouseButtons[2] = down; break;
                    case Button4: m_input.wheel += down ? 1.0f : 0.0f; break;
                    case Button5: m_input.wheel -= down ? 1.0f : 0.0f; break;
                    default: break;
                    }
                    break;
                }
                default:
                    break;
                }
            }

            enum : u32 { DropAware, DropPosition, DropStatus, DropDrop, DropFinished, DropSelection, DropCopy, DropList, DropHere, kDropAtoms };

            void sendDrop(Window to, u32 what, const long (&data)[5])
            {
                XEvent event = {};
                event.xclient.type = ClientMessage;
                event.xclient.display = m_display;
                event.xclient.window = to;
                event.xclient.message_type = m_drop[what];
                event.xclient.format = 32;
                for (u32 i = 0; i < 5; i++)
                {
                    event.xclient.data.l[i] = data[i];
                }
                // Whoever it was may have gone.
                int (*before)(Display*, XErrorEvent*) = XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
                XSendEvent(m_display, to, False, NoEventMask, &event);
                XSync(m_display, False);
                XSetErrorHandler(before);
            }

            // A line for each, as "file:///home/someone/a%20chair.obj".
            void takeDrop(const XSelectionEvent& selection)
            {
                unsigned char* text = nullptr;
                if (selection.property != 0)
                {
                    Atom type = 0;
                    int format = 0;
                    unsigned long count = 0, left = 0;
                    XGetWindowProperty(m_display, m_window, m_drop[DropHere], 0, 16384, True, AnyPropertyType, &type, &format, &count, &left, &text);
                    std::string line;
                    u32 files = 0;
                    for (unsigned long i = 0; text && format == 8 && i <= count && files < 64; i++)
                    {
                        const char c = i < count ? static_cast<char>(text[i]) : '\n';
                        if (c != '\n' && c != '\r')
                        {
                            line += c;
                            continue;
                        }
                        if (line.compare(0, 7, "file://") == 0)
                        {
                            const size_t from = line.find('/', 7);
                            std::string path;
                            for (size_t k = from; from != std::string::npos && k < line.size(); k++)
                            {
                                if (line[k] == '%' && k + 2 < line.size())
                                {
                                    path += static_cast<char>(strtoul(line.substr(k + 1, 2).c_str(), nullptr, 16));
                                    k += 2;
                                }
                                else
                                {
                                    path += line[k];
                                }
                            }
                            if (!path.empty())
                            {
                                keepOwn(path.c_str());
                                files++;
                            }
                        }
                        line.clear();
                    }
                }
                if (text)
                {
                    XFree(text);
                }
                if (m_dropFrom)
                {
                    const long done[5] = { static_cast<long>(m_window), selection.property != 0, static_cast<long>(m_drop[DropCopy]), 0, 0 };
                    sendDrop(m_dropFrom, DropFinished, done);
                    m_dropFrom = 0;
                }
            }

            void rumble(u32 pad, float heavy, float light) override
            {
                if (pad < kMaxPads)
                {
                    m_pads[pad].rumble(heavy, light);
                }
            }

            bool setFullscreen(bool on) override
            {
                if (m_display == nullptr)
                {
                    return false;
                }
                // Asked of the window manager, which every desktop has: _NET_WM_STATE_FULLSCREEN.
                XEvent event = {};
                event.xclient.type = ClientMessage;
                event.xclient.window = m_window;
                event.xclient.message_type = XInternAtom(m_display, "_NET_WM_STATE", False);
                event.xclient.format = 32;
                event.xclient.data.l[0] = on ? 1 : 0;
                event.xclient.data.l[1] = static_cast<long>(XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False));
                event.xclient.data.l[3] = 1;
                XSendEvent(m_display, DefaultRootWindow(m_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
                XFlush(m_display);
                m_fullscreen = on;
                return true;
            }

            bool fullscreen() const override { return m_fullscreen; }

            void setTitle(const char* text) override
            {
                if (m_display)
                {
                    XStoreName(m_display, m_window, text);
                    XChangeProperty(m_display, m_window, XInternAtom(m_display, "_NET_WM_NAME", False), XInternAtom(m_display, "UTF8_STRING", False), 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(text), static_cast<int>(strlen(text)));
                    XFlush(m_display);
                }
            }

            bool setWindowSize(u32 width, u32 height) override
            {
                if (m_display == nullptr || m_fullscreen || width < 64 || height < 64)
                {
                    return false;
                }
                XResizeWindow(m_display, m_window, width, height);
                XFlush(m_display);
                return true;
            }

            void setPointer(PointerShape shape) override
            {
                static const unsigned int kShapes[] = { XC_left_ptr, XC_hand2, XC_xterm, XC_crosshair, XC_watch, XC_fleur, XC_sb_h_double_arrow, XC_sb_v_double_arrow, XC_X_cursor };
                if (m_display == nullptr)
                {
                    return;
                }
                if (m_pointer)
                {
                    XFreeCursor(m_display, m_pointer);
                }
                m_pointer = XCreateFontCursor(m_display, kShapes[static_cast<u32>(shape) < 9 ? static_cast<u32>(shape) : 0]);
                if (!m_locked)
                {
                    XDefineCursor(m_display, m_window, m_pointer);
                    XFlush(m_display);
                }
            }

            bool focused() const override { return m_focused; }

            // With whichever of zenity, kdialog or xmessage there is, waited for.
            bool showMessage(const char* title, const char* text, bool question) override
            {
                if (m_unseen)
                {
                    return false;   // unattended: nobody to answer
                }
                const std::string t = std::string("--title=") + title;
                const char* zenity[] = { "zenity", question ? "--question" : "--info", t.c_str(), "--text", text, nullptr };
                const char* kdialog[] = { "kdialog", "--title", title, question ? "--yesno" : "--msgbox", text, nullptr };
                const char* xmessage[] = { "xmessage", "-center", "-buttons", question ? "Yes:0,No:1" : "OK:0", text, nullptr };
                for (const char* const* command : { zenity, kdialog, xmessage })
                {
                    const int result = runAndWait(command);
                    if (result >= 0 && result != 127)
                    {
                        return result == 0;
                    }
                }
                return false;
            }

            // With wl-paste or wl-copy on Wayland, xclip or xsel on X11. None of them: the engine keeps its own.
            bool clipboard(std::string& text) override
            {
                if (m_unseen)
                {
                    return false;
                }
                const bool wayland = getenv("WAYLAND_DISPLAY") != nullptr;
                for (const char* command : { wayland ? "wl-paste --no-newline 2>/dev/null" : "", "xclip -selection clipboard -o 2>/dev/null", "xsel --clipboard --output 2>/dev/null" })
                {
                    FILE* pipe = command[0] ? popen(command, "r") : nullptr;
                    if (pipe == nullptr)
                    {
                        continue;
                    }
                    std::string got;
                    char chunk[1024];
                    size_t n;
                    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0 && got.size() < (1u << 20))
                    {
                        got.append(chunk, n);
                    }
                    const int status = pclose(pipe);
                    if (status == 0 || !got.empty())
                    {
                        text = got;
                        return true;
                    }
                }
                return false;
            }

            bool setClipboard(const char* text) override
            {
                if (m_unseen)
                {
                    return false;
                }
                const bool wayland = getenv("WAYLAND_DISPLAY") != nullptr;
                for (const char* command : { wayland ? "wl-copy 2>/dev/null" : "", "xclip -selection clipboard -i 2>/dev/null", "xsel --clipboard --input 2>/dev/null" })
                {
                    FILE* pipe = command[0] ? popen(command, "w") : nullptr;
                    if (pipe == nullptr)
                    {
                        continue;
                    }
                    fwrite(text, 1, strlen(text), pipe);
                    if (pclose(pipe) == 0)
                    {
                        return true;
                    }
                }
                return false;
            }

            static int runAndWait(const char* const* command)
            {
                pid_t child = 0;
                if (posix_spawnp(&child, command[0], nullptr, nullptr, const_cast<char* const*>(command), environ) != 0)
                {
                    return -1;
                }
                int status = 0;
                if (waitpid(child, &status, 0) != child || !WIFEXITED(status))
                {
                    return -1;
                }
                return WEXITSTATUS(status);
            }

            void lockMouse(bool on) override
            {
                if (m_display == nullptr || on == m_locked)
                {
                    return;
                }
                m_locked = on;
                if (on)
                {
                    // A cursor of nothing.
                    char none[8] = {};
                    XColor black = {};
                    const Pixmap empty = XCreateBitmapFromData(m_display, m_window, none, 8, 8);
                    m_hidden = XCreatePixmapCursor(m_display, empty, empty, &black, &black, 0, 0);
                    XFreePixmap(m_display, empty);
                    XDefineCursor(m_display, m_window, m_hidden);
                    XGrabPointer(m_display, m_window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, m_window, m_hidden, CurrentTime);
                }
                else
                {
                    XUngrabPointer(m_display, CurrentTime);
                    if (m_pointer)
                    {
                        XDefineCursor(m_display, m_window, m_pointer);
                    }
                    else
                    {
                        XUndefineCursor(m_display, m_window);
                    }
                    if (m_hidden)
                    {
                        XFreeCursor(m_display, m_hidden);
                        m_hidden = 0;
                    }
                }
            }

            void readPads()
            {
                // Looking for new ones means going through the devices, so only now and then.
                const double time = seconds();
                if (time >= m_nextPadSearch)
                {
                    m_nextPadSearch = time + 2.0;
                    findPads();
                }
                for (u32 i = 0; i < kMaxPads; i++)
                {
                    Gamepad pad;
                    if (m_pads[i].file >= 0)
                    {
                        m_pads[i].read();
                    }
                    if (m_pads[i].file >= 0)
                    {
                        // A pad works only while the window is in front.
                        if (m_focused)
                        {
                            pad = m_pads[i].state();
                        }
                        pad.connected = true;
                    }
                    m_input.pads[i] = pad;
                }
            }

            void findPads()
            {
                DIR* folder = opendir("/dev/input");
                if (folder == nullptr)
                {
                    return;
                }
                while (const dirent* entry = readdir(folder))
                {
                    char path[300];
                    snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
                    EvdevPad* empty = nullptr;
                    bool known = strncmp(entry->d_name, "event", 5) != 0;
                    for (EvdevPad& pad : m_pads)
                    {
                        known = known || (pad.file >= 0 && strcmp(pad.path, path) == 0);
                        empty = empty == nullptr && pad.file < 0 ? &pad : empty;
                    }
                    if (!known && empty)
                    {
                        empty->open(path);
                    }
                }
                closedir(folder);
            }

            Display* m_display = nullptr;
            Window m_window = 0;
            Colormap m_colormap = 0;
            GLXContext m_context = nullptr;
            Atom m_closeMessage = 0;
            bool m_locked = false, m_seenMouse = false;
            Cursor m_hidden = 0;
            Cursor m_pointer = 0;
            void* m_interval = nullptr;   // glXSwapIntervalEXT
            bool m_fullscreen = false;
            bool m_unseen = false;   // opened hidden
            Atom m_drop[kDropAtoms] = {};
            Window m_dropFrom = 0;
            GlWindow m_gl;
            u32 m_width = 0;
            u32 m_height = 0;
            InputState m_input;
            EvdevPad m_pads[kMaxPads];
            double m_nextPadSearch = 0;
            bool m_focused = true;
            bool m_quit = false;
        };
    }

    Platform* createPlatform()
    {
        PlatformLinux* platform = create<PlatformLinux>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}
