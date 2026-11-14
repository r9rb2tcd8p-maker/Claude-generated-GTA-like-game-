// Palm Heat - native macOS launcher. Cocoa window + OpenGL 3.2 core context, no third-party libraries.
#define GL_SILENCE_DEPRECATION 1
#import <Cocoa/Cocoa.h>
#include <OpenGL/gl3.h>
#include <mach/mach_time.h>
#include <cstring>
#include "game.h"
#include "renderer.h"
#include "world.h"

static World gWorld;
static Game gGame;
static Renderer gRenderer;
static Input gIn;

static int keyForCode(unsigned short kc) {
    switch (kc) {
        case 13: return K_W;   case 0: return K_A;    case 1: return K_S;    case 2: return K_D;
        case 126: return K_UP; case 125: return K_DOWN; case 123: return K_LEFT; case 124: return K_RIGHT;
        case 49: return K_SPACE; case 8: return K_C;  case 14: return K_E;   case 3: return K_F;
        case 15: return K_R;   case 9: return K_V;    case 18: return K_1;   case 19: return K_2;
        case 20: return K_3;   case 6: return K_Z;    case 7: return K_X;    case 46: return K_M;
        case 96: return K_F5;  case 101: return K_F9; case 53: return K_ESC; case 12: return K_Q;
        default: return -1;
    }
}

@interface GameView : NSOpenGLView {
    NSTimer* _timer;
    uint64_t _last;
    BOOL _captured;
    BOOL _ready;
    int _bw, _bh;
}
@end

@implementation GameView

- (instancetype)initWithFrame:(NSRect)frame {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFADoubleBuffer, NSOpenGLPFAColorSize, 24, NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAAccelerated, 0};
    NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if (!pf) { NSLog(@"Could not create an OpenGL 3.2 core pixel format"); return nil; }
    self = [super initWithFrame:frame pixelFormat:pf];
    if (self) [self setWantsBestResolutionOpenGLSurface:YES];
    return self;
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)e { return YES; }

- (void)startup {
    if (_ready) return;
    [[self openGLContext] makeCurrentContext];
    GLint swap = 1;
    [[self openGLContext] setValues:&swap forParameter:NSOpenGLContextParameterSwapInterval];
    NSRect b = [self convertRectToBacking:[self bounds]];
    _bw = (int)b.size.width; _bh = (int)b.size.height;
    gWorld.build();
    if (!gRenderer.init(_bw, _bh)) NSLog(@"Renderer init failed");
    gRenderer.uploadStatic(gWorld.statics.v);
    gGame.init(&gWorld);
    _last = mach_absolute_time();
    _ready = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(lostFocus:)
                                                 name:NSWindowDidResignKeyNotification object:[self window]];
    _timer = [NSTimer timerWithTimeInterval:1.0 / 240.0 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
}

- (void)lostFocus:(NSNotification*)n {
    gGame.paused = true;
    std::memset(gIn.down, 0, sizeof gIn.down);
    gIn.lmb = gIn.rmb = false;
}

- (void)setCapture:(BOOL)want {
    if (want == _captured) return;
    _captured = want;
    if (want) { CGAssociateMouseAndMouseCursorPosition(false); [NSCursor hide]; }
    else { CGAssociateMouseAndMouseCursorPosition(true); [NSCursor unhide]; }
}

- (void)tick:(NSTimer*)t {
    if (!_ready) return;
    [[self openGLContext] makeCurrentContext];
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t now = mach_absolute_time();
    double dt = (double)(now - _last) * tb.numer / tb.denom / 1e9;
    _last = now;

    NSRect b = [self convertRectToBacking:[self bounds]];
    if ((int)b.size.width != _bw || (int)b.size.height != _bh) {
        _bw = (int)b.size.width; _bh = (int)b.size.height;
        if (_bw > 0 && _bh > 0) gRenderer.resize(_bw, _bh);
    }
    if (_bw <= 0 || _bh <= 0) return;

    gGame.update((float)dt, gIn);
    std::memset(gIn.pressed, 0, sizeof gIn.pressed);
    gIn.mdx = gIn.mdy = 0; gIn.lmbPressed = false;
    if (gGame.quitRequested) { [self setCapture:NO]; [NSApp terminate:nil]; return; }
    [self setCapture:(!gGame.paused && [[self window] isKeyWindow])];

    gRenderer.beginFrame();
    gGame.render(gRenderer);
    [[self openGLContext] flushBuffer];
}

- (void)keyDown:(NSEvent*)e {
    int k = keyForCode(e.keyCode);
    if (k < 0) return;
    if (!e.isARepeat) gIn.pressed[k] = true;
    gIn.down[k] = true;
}
- (void)keyUp:(NSEvent*)e {
    int k = keyForCode(e.keyCode);
    if (k >= 0) gIn.down[k] = false;
}
- (void)flagsChanged:(NSEvent*)e { gIn.down[K_SHIFT] = (e.modifierFlags & NSEventModifierFlagShift) != 0; }

- (void)mouseMoved:(NSEvent*)e { if (_captured) { gIn.mdx += (float)e.deltaX; gIn.mdy += (float)e.deltaY; } }
- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)rightMouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)otherMouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)mouseDown:(NSEvent*)e { if (_captured) { gIn.lmb = true; gIn.lmbPressed = true; } }
- (void)mouseUp:(NSEvent*)e { gIn.lmb = false; }
- (void)rightMouseDown:(NSEvent*)e { if (_captured) gIn.rmb = true; }
- (void)rightMouseUp:(NSEvent*)e { gIn.rmb = false; }

- (void)dealloc { [self setCapture:NO]; }
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end
@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)a { return YES; }
- (void)applicationWillTerminate:(NSNotification*)n {
    CGAssociateMouseAndMouseCursorPosition(true);
    [NSCursor unhide];
}
@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate* del = [[AppDelegate alloc] init];
        [app setDelegate:del];

        NSMenu* bar = [[NSMenu alloc] init];
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        [bar addItem:appItem];
        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit Palm Heat" action:@selector(terminate:) keyEquivalent:@"q"];
        [appItem setSubmenu:appMenu];
        NSMenuItem* viewItem = [[NSMenuItem alloc] init];
        [bar addItem:viewItem];
        NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
        NSMenuItem* fs = [viewMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
        [fs setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];
        [viewItem setSubmenu:viewMenu];
        [app setMainMenu:bar];

        NSRect frame = NSMakeRect(0, 0, 1280, 720);
        NSWindow* win = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                      backing:NSBackingStoreBuffered defer:NO];
        [win setTitle:@"Palm Heat"];
        [win setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [win setAcceptsMouseMovedEvents:YES];
        [win setReleasedWhenClosed:NO];
        GameView* view = [[GameView alloc] initWithFrame:frame];
        if (!view) return 1;
        [win setContentView:view];
        [win makeFirstResponder:view];
        [win center];
        [win makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        [view startup];
        [app run];
    }
    return 0;
}
