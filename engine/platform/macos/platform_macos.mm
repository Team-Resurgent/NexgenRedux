#define GL_SILENCE_DEPRECATION
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#import <Cocoa/Cocoa.h>
#import <GameController/GameController.h>

#include "base/log.h"
#include "gpu/gl/gl_window.h"
#include "platform/posix/platform_posix.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Closing the window, or quitting from the menu, only asks: the engine decides when.
@interface NexgenDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
{
@public
    bool quit;
    bool focused;
    bool resized;
    void (*dropped)(const char* path, void* user);
    void* droppedUser;
}
@end

@implementation NexgenDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    quit = true;
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    (void)sender;
    quit = true;
    return NSTerminateCancel;
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
    (void)notification;
    focused = true;
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    (void)notification;
    focused = false;
}

// The window passes these on to its delegate.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    (void)sender;
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSArray* files = [[sender draggingPasteboard] readObjectsForClasses:@[ [NSURL class] ] options:@{ NSPasteboardURLReadingFileURLsOnlyKey : @YES }];
    NSUInteger count = [files count] < 64 ? [files count] : 64;
    for (NSUInteger i = 0; i < count && dropped; i++)
    {
        dropped([[(NSURL*)[files objectAtIndex:i] path] fileSystemRepresentation], droppedUser);
    }
    return count > 0;
}

- (void)windowDidResize:(NSNotification*)notification
{
    (void)notification;
    resized = true;
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    (void)notification;
    resized = true;
}

@end

namespace nx
{
    namespace
    {
        // By where the key is, as macOS numbers them.
        Key translateKey(unsigned short code)
        {
            static const struct { unsigned short code; Key key; } map[] = {
                { 0, Key::A }, { 11, Key::B }, { 8, Key::C }, { 2, Key::D }, { 14, Key::E }, { 3, Key::F }, { 5, Key::G }, { 4, Key::H }, { 34, Key::I },
                { 38, Key::J }, { 40, Key::K }, { 37, Key::L }, { 46, Key::M }, { 45, Key::N }, { 31, Key::O }, { 35, Key::P }, { 12, Key::Q }, { 15, Key::R },
                { 1, Key::S }, { 17, Key::T }, { 32, Key::U }, { 9, Key::V }, { 13, Key::W }, { 7, Key::X }, { 16, Key::Y }, { 6, Key::Z },
                { 29, Key::Num0 }, { 18, Key::Num1 }, { 19, Key::Num2 }, { 20, Key::Num3 }, { 21, Key::Num4 }, { 23, Key::Num5 }, { 22, Key::Num6 },
                { 26, Key::Num7 }, { 28, Key::Num8 }, { 25, Key::Num9 },
                { 126, Key::Up }, { 125, Key::Down }, { 123, Key::Left }, { 124, Key::Right }, { 49, Key::Space }, { 36, Key::Enter }, { 76, Key::Enter },
                { 53, Key::Escape }, { 48, Key::Tab }, { 51, Key::Backspace },
            };
            for (const auto& m : map)
            {
                if (m.code == code)
                {
                    return m.key;
                }
            }
            return Key::None;
        }

        class PlatformMacOS final : public PlatformPosix
        {
        public:
            ~PlatformMacOS() override
            {
                @autoreleasepool
                {
                    if (m_context)
                    {
                        [NSOpenGLContext clearCurrentContext];
                        [m_context clearDrawable];
                        [m_context release];
                    }
                    if (m_window)
                    {
                        [m_window setDelegate:nil];
                        [m_window close];
                        [m_window release];
                    }
                    if (m_delegate)
                    {
                        [NSApp setDelegate:nil];
                        [m_delegate release];
                    }
                }
                if (m_library)
                {
                    dlclose(m_library);
                }
            }

            bool init()
            {
                char program[kMaxPath] = {};
                char base[kMaxPath] = {};
                u32 size = sizeof(program);
                if (_NSGetExecutablePath(program, &size) != 0 || realpath(program, base) == nullptr)
                {
                    NX_ERROR("cannot find the program's folder");
                    return false;
                }
                if (char* slash = strrchr(base, '/'))
                {
                    *slash = 0;
                }
                // Inside an application bundle the media is with its other resources.
                char media[kMaxPath];
                const char* tail = "/Contents/MacOS";
                const size_t length = strlen(base);
                const bool bundle = length > strlen(tail) && strcmp(base + length - strlen(tail), tail) == 0;
                snprintf(media, sizeof(media), "%s%s", base, bundle ? "/../Resources/media" : "/media");
                char saves[kMaxPath];
                const char* home = getenv("HOME");
                snprintf(saves, sizeof(saves), "%s/Library/Application Support/Nexgen", home ? home : "/tmp");
                return setFolders(media, saves);
            }

            bool openWindow(const WindowDesc& desc) override
            {
                @autoreleasepool
                {
                    [NSApplication sharedApplication];
                    m_delegate = [[NexgenDelegate alloc] init];
                    [NSApp setDelegate:m_delegate];
                    [NSApp setActivationPolicy:desc.hidden ? NSApplicationActivationPolicyProhibited : NSApplicationActivationPolicyRegular];
                    makeMenu();
                    [NSApp finishLaunching];

                    const NSRect frame = NSMakeRect(0, 0, desc.width, desc.height);
                    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
                    m_window = [[NSWindow alloc] initWithContentRect:frame styleMask:style backing:NSBackingStoreBuffered defer:NO];
                    if (m_window == nil)
                    {
                        NX_ERROR("cannot open a window");
                        return false;
                    }
                    [m_window setReleasedWhenClosed:NO];
                    [m_window setTitle:[NSString stringWithUTF8String:desc.title]];
                    [m_window setDelegate:m_delegate];
                    [m_window setAcceptsMouseMovedEvents:YES];
                    m_delegate->dropped = [](const char* path, void* user) { static_cast<PlatformMacOS*>(user)->keepOwn(path); };
                    m_delegate->droppedUser = this;
                    [m_window registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
                    [m_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
                    // Real pixels on a Retina screen, except when hidden, where the size should be exact.
                    [[m_window contentView] setWantsBestResolutionOpenGLSurface:desc.hidden ? NO : YES];
                    [m_window center];
                    if (!desc.hidden)
                    {
                        [m_window makeKeyAndOrderFront:nil];
                        [NSApp activateIgnoringOtherApps:YES];
                    }
                    m_hidden = desc.hidden;
                    return createContext();
                }
            }

            void* nativeWindow() override { return m_context ? &m_gl : nullptr; }

            bool pumpEvents() override
            {
                @autoreleasepool
                {
                    if (m_window)
                    {
                        NSEvent* event = nil;
                        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES]) != nil)
                        {
                            handle(event);
                        }
                        if (!m_delegate->focused)
                        {
                            // Key-ups go elsewhere, so nothing may stay held.
                            memset(m_input.keys, 0, sizeof(m_input.keys));
                            memset(m_input.mouseButtons, 0, sizeof(m_input.mouseButtons));
                        }
                        if (m_delegate->resized)
                        {
                            m_delegate->resized = false;
                            [m_context update];
                        }
                    }
                    readPads();
                }
                return m_delegate == nil || !m_delegate->quit;
            }

            const InputState& input() const override { return m_input; }
            const char* name() const override { return "macOS"; }

        private:
            static void makeMenu()
            {
                NSMenu* bar = [[[NSMenu alloc] init] autorelease];
                NSMenuItem* item = [[[NSMenuItem alloc] init] autorelease];
                [bar addItem:item];
                NSMenu* menu = [[[NSMenu alloc] init] autorelease];
                [menu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
                [item setSubmenu:menu];
                [NSApp setMainMenu:bar];
            }

            bool createContext()
            {
                const NSOpenGLPixelFormatAttribute attributes[] = {
                    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core, NSOpenGLPFADoubleBuffer, NSOpenGLPFAColorSize, 24, NSOpenGLPFAAllowOfflineRenderers, 0
                };
                NSOpenGLPixelFormat* format = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
                m_context = format ? [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil] : nil;
                m_library = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
                if (m_context == nil || m_library == nullptr)
                {
                    NX_ERROR("OpenGL 3.3 is needed");
                    return false;
                }
                // Deprecated with the rest of OpenGL, but under a warning of its own.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                [m_context setView:[m_window contentView]];
#pragma clang diagnostic pop
                [m_context makeCurrentContext];
                const GLint interval = m_hidden ? 0 : 1;
                [m_context setValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
                m_gl.user = this;
                m_gl.load = [](const char* function) { return dlsym(RTLD_DEFAULT, function); };
                m_gl.size = [](void* self, u32& width, u32& height) { static_cast<PlatformMacOS*>(self)->pixelSize(width, height); };
                m_gl.swap = [](void* self) { [static_cast<PlatformMacOS*>(self)->m_context flushBuffer]; };
                return true;
            }

            void pixelSize(u32& width, u32& height)
            {
                NSView* view = [m_window contentView];
                const NSRect bounds = m_hidden ? [view bounds] : [view convertRectToBacking:[view bounds]];
                const bool gone = !m_hidden && [m_window isMiniaturized];
                width = gone ? 0 : static_cast<u32>(bounds.size.width);
                height = gone ? 0 : static_cast<u32>(bounds.size.height);
            }

            void handle(NSEvent* event)
            {
                const NSEventType type = [event type];
                switch (type)
                {
                case NSEventTypeKeyDown:
                case NSEventTypeKeyUp:
                    // A menu shortcut isn't a key press; anything else would beep if it were passed on.
                    if (type == NSEventTypeKeyDown && ([event modifierFlags] & NSEventModifierFlagCommand))
                    {
                        break;
                    }
                    m_input.keys[static_cast<size_t>(translateKey([event keyCode]))] = type == NSEventTypeKeyDown;
                    if (type == NSEventTypeKeyDown)
                    {
                        NSString* text = [event characters];
                        for (NSUInteger i = 0; i < [text length]; i++)
                        {
                            const unichar c = [text characterAtIndex:i];
                            // The arrows and others are in here too, from F700 up; and Delete is 7F.
                            if (c < 0xD800)
                            {
                                m_input.type(c == 0x7F ? 8 : c);
                            }
                        }
                    }
                    m_input.keys[static_cast<size_t>(Key::None)] = false;
                    return;
                case NSEventTypeFlagsChanged:
                    m_input.keys[static_cast<size_t>(Key::Shift)] = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
                    m_input.keys[static_cast<size_t>(Key::Control)] = ([event modifierFlags] & NSEventModifierFlagControl) != 0;
                    break;
                case NSEventTypeMouseMoved:
                case NSEventTypeLeftMouseDragged:
                case NSEventTypeRightMouseDragged:
                case NSEventTypeOtherMouseDragged:
                    moveMouse(event);
                    break;
                case NSEventTypeLeftMouseDown:
                case NSEventTypeLeftMouseUp:
                    press(event, 0, type == NSEventTypeLeftMouseDown);
                    break;
                case NSEventTypeRightMouseDown:
                case NSEventTypeRightMouseUp:
                    press(event, 1, type == NSEventTypeRightMouseDown);
                    break;
                case NSEventTypeOtherMouseDown:
                case NSEventTypeOtherMouseUp:
                    press(event, 2, type == NSEventTypeOtherMouseDown);
                    break;
                case NSEventTypeScrollWheel:
                    m_input.wheel += static_cast<float>([event hasPreciseScrollingDeltas] ? [event scrollingDeltaY] / 10 : [event scrollingDeltaY]);
                    break;
                default:
                    break;
                }
                [NSApp sendEvent:event];
            }

            void moveMouse(NSEvent* event)
            {
                NSView* view = [m_window contentView];
                const NSPoint at = [view convertPointToBacking:[view convertPoint:[event locationInWindow] fromView:nil]];
                u32 width = 0, height = 0;
                pixelSize(width, height);
                m_input.hasMouse = true;
                m_input.mouseX = static_cast<float>(at.x);
                m_input.mouseY = static_cast<float>(height) - static_cast<float>(at.y);
            }

            void press(NSEvent* event, u32 button, bool down)
            {
                // Not on the title bar.
                NSView* view = [m_window contentView];
                const NSPoint at = [view convertPoint:[event locationInWindow] fromView:nil];
                if (!down || NSPointInRect(at, [view bounds]))
                {
                    m_input.mouseButtons[button] = down;
                }
            }

            void readPads()
            {
                for (Gamepad& pad : m_input.pads)
                {
                    pad = {};
                }
                u32 next = 0;
                for (GCController* controller in [GCController controllers])
                {
                    GCExtendedGamepad* source = [controller extendedGamepad];
                    if (source == nil || next == kMaxPads)
                    {
                        continue;
                    }
                    Gamepad& pad = m_input.pads[next++];
                    pad.connected = true;
                    // A pad works only while the window is in front.
                    if (m_delegate == nil || !m_delegate->focused)
                    {
                        continue;
                    }
                    const auto set = [&](GCControllerButtonInput* button, Button b) { pad.buttons |= button != nil && [button isPressed] ? 1u << static_cast<u32>(b) : 0; };
                    set([[source dpad] up], Button::Up);
                    set([[source dpad] down], Button::Down);
                    set([[source dpad] left], Button::Left);
                    set([[source dpad] right], Button::Right);
                    set([source buttonMenu], Button::Start);
                    set([source buttonOptions], Button::Back);
                    set([source leftThumbstickButton], Button::LeftStick);
                    set([source rightThumbstickButton], Button::RightStick);
                    set([source buttonA], Button::A);
                    set([source buttonB], Button::B);
                    set([source buttonX], Button::X);
                    set([source buttonY], Button::Y);
                    set([source leftShoulder], Button::LeftShoulder);
                    set([source rightShoulder], Button::RightShoulder);
                    pad.leftX = removeDeadZone([[[source leftThumbstick] xAxis] value]);
                    pad.leftY = removeDeadZone([[[source leftThumbstick] yAxis] value]);
                    pad.rightX = removeDeadZone([[[source rightThumbstick] xAxis] value]);
                    pad.rightY = removeDeadZone([[[source rightThumbstick] yAxis] value]);
                    pad.leftTrigger = [[source leftTrigger] value];
                    pad.rightTrigger = [[source rightTrigger] value];
                }
            }

            NexgenDelegate* m_delegate = nil;
            NSWindow* m_window = nil;
            NSOpenGLContext* m_context = nil;
            void* m_library = nullptr;
            GlWindow m_gl;
            InputState m_input;
            bool m_hidden = false;
        };
    }

    Platform* createPlatform()
    {
        PlatformMacOS* platform = create<PlatformMacOS>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}
