/***************************************

	OpenGL manager class

	MacOSX only

	Copyright 1995-2014 by Rebecca Ann Heineman becky@burgerbecky.com

	It is released under an MIT Open Source license. Please see LICENSE
	for license details. Yes, you can use it in a
	commercial title without paying anything, just give me a credit.
	Please? It's not like I'm asking you for money!

***************************************/

#include "brdisplayopengl.h"
#if defined(BURGER_MACOSX) || defined(DOXYGEN)
#include "brdebug.h"
#include "brmacosxapp.h"

#if !defined(DOXYGEN)
#define GL_GLEXT_PROTOTYPES
#endif

#if defined(MAC_OS_X_VERSION_10_6)
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#endif
#include <OpenGL/glext.h>
#include <OpenGL/OpenGL.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSOpenGLView.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSWindowController.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSResponder.h>
#import <AppKit/NSEvent.h>

#if !defined(DOXYGEN)

//
// Special window for full screen rendering
//

@interface BurgerFullScreenWindow : NSWindow
@end

@implementation BurgerFullScreenWindow

//
// Initialize the window with the size of the screen
//

- (id) init
{
	// Use the size of the display
	NSRect ScreenRect = [[NSScreen mainScreen] frame];
	
	// Create a borderless window to cover the whole screen
	// (Use deferred rendering to get rid of screen tears)
	
	self = [super initWithContentRect:ScreenRect styleMask:NSBorderlessWindowMask
		backing:NSBackingStoreBuffered defer:YES];
	// Elevate the level to make sure everything else is hidden
	[self setLevel:NSMainMenuWindowLevel+1];
	
	// Turn off translucency
	[self setOpaque:YES];
	
	// If the app switches, hide this window
	[self setHidesOnDeactivate:YES];
	return self;
}

//
// Allow input on a borderless window
//

- (BOOL) canBecomeKeyWindow
{
	return YES;
}

@end


//
// OpenGLView which handles the callbacks
//

@interface BurgerGLView : NSOpenGLView {
	Burger::DisplayOpenGL *m_pDisplay;
}
- (id) initWithDisplay : (Burger::DisplayOpenGL *)pDisplay;
- (void) drawView;
@end

@implementation BurgerGLView

- (id) initWithDisplay:(Burger::DisplayOpenGL *)pDisplay
{
	self = [super init];
	if (self) {
		m_pDisplay = pDisplay;
	}
	return self;
}

//
// Resize was called, alert the application of the size change
//

- (void) reshape
{
	[super reshape];
	// Was there a resize function installed?
	
	if (m_pDisplay->GetResizeCallback()) {
		// Lock OpenGL
		CGLLockContext(m_pDisplay->GetOpenGLContext());
		
#if defined(MAC_OS_X_VERSION_10_6)
		// Get the view size in Points
		NSRect viewRectPoints = [self bounds];
		// Convert to pixels
		NSRect viewRectPixels = [self convertRectToBacking:viewRectPoints];
		// Set the new dimensions in our renderer
		m_pDisplay->GetResizeCallback()(m_pDisplay->GetResizeCallbackData(),static_cast<int>(viewRectPixels.size.width),static_cast<int>(viewRectPixels.size.height));
#endif
		// Release OpenGL
		CGLUnlockContext(m_pDisplay->GetOpenGLContext());
	}
}

//
// Called whenever graphics state updated (such as window resize)
//

- (void)renewGState
{
	// OpenGL rendering is not synchronous with other rendering on the OSX.
	// Therefore, call disableScreenUpdatesUntilFlush so the window server
	// doesn't render non-OpenGL content in the window asynchronously from
	// OpenGL content, which could cause flickering.  (non-OpenGL content
	// includes the title bar and drawing done by the app with other APIs)
	[[self window] disableScreenUpdatesUntilFlush];
	[super renewGState];
}

//
// Draw the window
//

- (void) drawRect: (NSRect) theRect
{
#pragma unused(theRect)
	// Called during resize operations
	// Avoid flickering during resize by drawing
	[self drawView];
}

//
// Redraw the game screen if the OS needs it
// (Usually through resizing)
//

- (void) drawView
{
	if (m_pDisplay->GetRenderCallback()) {
		[[self openGLContext] makeCurrentContext];
	
		// We draw on a secondary thread through the display link
		// When resizing the view, -reshape is called automatically on the main
		// thread. Add a mutex around to avoid the threads accessing the context
		// simultaneously when resizing

		// Lock OpenGL
		CGLLockContext(m_pDisplay->GetOpenGLContext());
		m_pDisplay->GetRenderCallback()(m_pDisplay->GetRenderCallbackData());
		// Force update
		CGLFlushDrawable(m_pDisplay->GetOpenGLContext());
		// Release OpenGL
		CGLUnlockContext(m_pDisplay->GetOpenGLContext());
	}
}

@end


@interface BurgerWindowController : NSWindowController {
	Burger::DisplayOpenGL *m_pDisplay;
}
@end

@implementation BurgerWindowController

- (id)initWithWindow:(NSWindow *)window display:(Burger::DisplayOpenGL *)pDisplay
{
	self = [super initWithWindow:window];
	if (self) {
		m_pDisplay = pDisplay;
    }
	return self;
}

//
// Force full screen mode
//

- (void) goFullscreen
{
	NSWindow *pFullScreenWindow = m_pDisplay->GetFullScreenWindow();
	if (!pFullScreenWindow) {
		// Allocate a new fullscreen window
		pFullScreenWindow = [[BurgerFullScreenWindow alloc] init];
		m_pDisplay->SetFullScreenWindow(pFullScreenWindow);
	}
	// Resize the view to screensize
	NSRect viewRect = [pFullScreenWindow frame];
	
	// Set the view to the size of the fullscreen window
	[m_pDisplay->GetOpenGLView() setFrameSize: viewRect.size];
	
	// Set the view in the fullscreen window
	[pFullScreenWindow setContentView:m_pDisplay->GetOpenGLView()];
	
	// Hide non-fullscreen window so it doesn't show up when switching out
	// of this app (i.e. with CMD-TAB)
	[static_cast<Burger::MacOSXApp *>(m_pDisplay->GetGameApp())->GetWindow() orderOut:self];
	
	// Set controller to the fullscreen window so that all input will go to
	// this controller (self)
	[self setWindow:pFullScreenWindow];
	
	// Show the window and make it the key window for input
	[pFullScreenWindow makeKeyAndOrderFront:self];
}

//
// Force window mode
//

- (void) goWindow
{
	//
	// Already a window?
	//

	NSWindow *pWindow = static_cast<Burger::MacOSXApp *>(m_pDisplay->GetGameApp())->GetWindow();
	// Get the rectangle of the original window
	NSRect viewRect = [pWindow frame];
	
	// Set the view rect to the new size
	[m_pDisplay->GetOpenGLView() setFrame:viewRect];
	
	// Set controller to the standard window so that all input will go to
	// this controller (self)
	[self setWindow:pWindow];
	
	// Set the content of the orginal window to the view
	[pWindow setContentView:m_pDisplay->GetOpenGLView()];
	
	// Show the window and make it the key window for input
	[pWindow makeKeyAndOrderFront:self];
	
	NSWindow *pFullScreenWindow = m_pDisplay->GetFullScreenWindow();
	if (pFullScreenWindow) {
		// Release the fullscreen window
		[pFullScreenWindow release];
		m_pDisplay->SetFullScreenWindow(NULL);
	}
}

//
// Toggle from full screen to full screen
//

- (void) toggleFullscreen:(id)sender
{
#pragma unused(sender)
	if (m_pDisplay->GetFullScreenWindow() == NULL) {
		[self goFullscreen];
	} else {
		[self goWindow];
	}
}

//
// Check for the commmand "Alt-Enter and
// switch from full screen to windowed mode
// if allowed
//

- (void) keyDown:(NSEvent *)event
{
	if (m_pDisplay->GetFlags()&Burger::Display::ALLOWFULLSCREENTOGGLE) {
		unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];
#if defined(MAC_OS_X_VERSION_10_6) || 1
		if ([event modifierFlags] & NSAlternateKeyMask) {
			switch (c) {
				// Have f key toggle fullscreen
			case '\r':
				[self toggleFullscreen:nil];
				return;
			}
		}
#endif
	}
	
	// Allow other character to be handled (or not and beep)
	[super keyDown:event];
}

@end

#endif


/*! ************************************

	\brief Initialize OpenGL

	Base class for instantiating a video display using OpenGL

	\sa Burger::DisplayOpenGL::~DisplayOpenGL()

***************************************/

Burger::DisplayOpenGL::DisplayOpenGL(Burger::GameApp *pGameApp) :
	Display(pGameApp,OPENGL),
	m_pCompressedFormats(NULL),
	m_pView(NULL),
	m_pWindowController(NULL),
	m_pOpenGLView(NULL),
	m_pOpenGLContext(NULL),
	m_pFullScreenWindow(NULL),
	m_fOpenGLVersion(0.0f),
	m_fShadingLanguageVersion(0.0f),
	m_uCompressedFormatCount(0)	
{
	SetRenderer(NULL);
}

/*! ************************************

	\brief Start up the OpenGL context

	Base class for instantiating a video display using OpenGL

	\sa Burger::DisplayOpenGL::PostShutdown()

***************************************/

Word Burger::DisplayOpenGL::InitContext(void)
{
	// Set the new size of the screen
	Word uWidth = m_uWidth;
	Word uHeight = m_uHeight;
	m_uFlags |= FULLPALETTEALLOWED;

	//
	// Create an auto-release pool for memory clean up
	//
	
	NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];

	//
	// Resize the main window
	//
	
	static_cast<MacOSXApp *>(m_pGameApp)->SetWindowSize(uWidth,uHeight);

	//
	// Is the full screen window needed?
	//
	
	if (m_uFlags&FULLSCREEN) {
		if (!m_pFullScreenWindow) {
			m_pFullScreenWindow = [[BurgerFullScreenWindow alloc] init];
		}
	} else {
		// Get rid of the full screen window
		if (m_pFullScreenWindow) {
			[m_pFullScreenWindow release];
			m_pFullScreenWindow = NULL;
		}
	}

	//
	// Initialize (Or reset) the OpenGL view
	//
	
	NSOpenGLView *pView = m_pOpenGLView;
	if (!m_pOpenGLView) {
		pView = [[BurgerGLView alloc] initWithDisplay:this];
		m_pOpenGLView = pView;
	}
	
	//
	// Set OpenGL to the requested screen size
	//
	
	NSRect GameScreenSize = NSMakeRect(0,0,uWidth,uHeight);
	[pView setFrame:GameScreenSize];
	
	//
	// Notify the view about resizing
	//
	
	NSUInteger uSizeMask;
	if (m_uFlags&ALLOWRESIZING) {
		uSizeMask = NSViewHeightSizable | NSViewWidthSizable;
	} else {
		uSizeMask = NSViewNotSizable;
	}
	[pView setAutoresizingMask:uSizeMask];
	
	//
	// Synchronize buffer swaps with vertical refresh rate
	//
	
	GLint swapInt = 1;
	[[pView openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
	
	//
	// Get our pixel format
	//
	
	NSOpenGLPixelFormatAttribute OpenGLAttributes[] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFADepthSize, 24,
		0
	};
	
	NSOpenGLPixelFormat *pPixelFormat = [[[NSOpenGLPixelFormat alloc] initWithAttributes:OpenGLAttributes] autorelease];
	NSOpenGLContext* pNSOpenGLContext = [[[NSOpenGLContext alloc] initWithFormat:pPixelFormat shareContext:nil] autorelease];
	m_pOpenGLContext = static_cast<CGLContextObj>([pNSOpenGLContext CGLContextObj]);
	[pView setPixelFormat:pPixelFormat];
	[pView setOpenGLContext:pNSOpenGLContext];
	
	//
	// Opt-In to Retina resolution (OSX 10.7 or later)
	//
	
	if ([pView respondsToSelector: @selector(setWantsBestResolutionOpenGLSurface:)]) {
		[pView performSelector: @selector(setWantsBestResolutionOpenGLSurface:) withObject: reinterpret_cast<id>(YES)];
	}
	
	//
	// Enable/disable resizing to the main window
	//
	
	NSWindow *pWindow = static_cast<MacOSXApp *>(m_pGameApp)->GetWindow();
	
	//
	// setStyleMask was added in 10.6. Crap, call it manually to work
	// on 10.5 systems
	//
	
	if ([pWindow respondsToSelector: @selector(setStyleMask:)]) {
		NSUInteger uWindowMask = [pWindow styleMask];
		NSUInteger uNewMask;
		if (m_uFlags & ALLOWRESIZING) {
			uNewMask = NSResizableWindowMask;
		} else {
			uNewMask = 0;
		}
		if ((uNewMask^uWindowMask)&NSResizableWindowMask) {
			[pWindow performSelector: @selector(setStyleMask:) withObject: reinterpret_cast<id>((uWindowMask&(~NSResizableWindowMask))|uNewMask)];
		}
	}
	
	//
	// Add in a controller to handle flipping between full screen and window mode
	//
	
	NSWindowController *pWindowController = m_pWindowController;
	if (!pWindowController) {
		pWindowController = [[BurgerWindowController alloc] initWithWindow:pWindow display:this];
		m_pWindowController = pWindowController;
	}
	//
	// Attach everything!
	//
	
	[pWindowController setWindow:pWindow];
	[pWindow setContentView:pView];
	[NSApp setDelegate:static_cast<id>(m_pOpenGLView)];
	
	//
	// Make the window visible
	//
#if 1
	if (m_uFlags&FULLSCREEN) {
		[static_cast<BurgerWindowController *>(pWindowController) goFullscreen];
	} else {
		[static_cast<BurgerWindowController *>(pWindowController) goWindow];
	}
#endif
//	[pWindow makeKeyAndOrderFront:pWindow];
	[pPool release];
	SetupOpenGL();
	return 0;
}

/*! ************************************

	\brief Start up the OpenGL context

	Shut down OpenGL

	\sa Burger::DisplayOpenGL::PostShutdown()

***************************************/

void Burger::DisplayOpenGL::PostShutdown(void)
{
	if (m_pFullScreenWindow) {
		[m_pFullScreenWindow release];
		m_pFullScreenWindow = NULL;
	}
	if (m_pOpenGLView) {
		[m_pOpenGLView release];
		m_pOpenGLView = NULL;
		m_pOpenGLContext = NULL;
	}
	if (m_pWindowController) {
		[m_pWindowController release];
		m_pWindowController = NULL;
	}
	Free(m_pCompressedFormats);
	m_pCompressedFormats = NULL;
	m_uCompressedFormatCount = 0;
}

void Burger::DisplayOpenGL::PreBeginScene(void)
{
	CGLLockContext(m_pOpenGLContext);
}

/*! ************************************

	\brief Update the video display

	Calls SwapBuffers() in OpenGL to draw the rendered scene

	\sa Burger::DisplayOpenGL::PreBeginScene()

***************************************/

void Burger::DisplayOpenGL::PostEndScene(void)
{
	// Consider it done!
	// Force update
	CGLFlushDrawable(m_pOpenGLContext);
	// Release OpenGL
	CGLUnlockContext(m_pOpenGLContext);

}

#endif
