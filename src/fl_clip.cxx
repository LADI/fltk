//
// "$Id: fl_clip.cxx,v 1.31 2005/01/25 20:11:46 matthiaswm Exp $"
//
// The fltk graphics clipping stack.  These routines are always
// linked into an fltk program.
//
// Copyright 1998-2003 by Bill Spitzak and others.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "fltk-bugs@fltk.org".
//

#include <config.h>
#include <fltk/Window.h>
#include <fltk/draw.h>
#include <fltk/x.h>
#include <stdlib.h>
using namespace fltk;

/*! \defgroup clipping Clipping
    \ingroup drawing

  You can limit all your drawing to a region by calling
  fltk::push_clip(), and put the drawings back by using
  fltk::pop_clip(). Fltk may also set up clipping before draw() is
  called to limit the drawing to the region of the window that is
  damaged.

  When drawing you can also test the current clip region with
  fltk::not_clipped() and fltk::clip_box(). By using these to skip
  over complex drawings that are clipped you can greatly speed up your
  program's redisplay.

  <i>The width and height of the clipping region is measured in
  transformed coordianates.</i>
*/

#if USE_X11
// Region == Region
#elif defined(_WIN32)
# define Region HRGN
#elif defined(__APPLE__)
# define Region RgnHandle
#endif

static Region emptyrstack = 0;
static Region* rstack = &emptyrstack;
static int rstacksize = 0;
static int rstackptr = 0;

static inline void pushregion(Region r) {
  if (rstackptr >= rstacksize) {
    if (rstacksize) {
      rstacksize = 2*rstacksize;
      rstack = (Region*)realloc(rstack, rstacksize*sizeof(Region));
    } else {
      rstacksize = 16;
      rstack = (Region*)malloc(rstacksize*sizeof(Region));
      rstack[0] = 0;
    }
  }
  rstack[++rstackptr] = r;
}

int fl_clip_state_number = 0; // used by code that needs to update clip regions

/*! Return the current region as a system-specific structure. You must
  include <fltk/x.h> to use this. Returns null if there is no clipping.
*/
Region fltk::clip_region() {
  return rstack[rstackptr];
}

#if USE_X11
// Missing X call: (is this the fastest way to init a 1-rectangle region?)
// MSWindows equivalent exists, implemented inline in win32.h
Region XRectangleRegion(int x, int y, int w, int h) {
  XRectangle R;
  R.x = x; R.y = y; R.width = w; R.height = h;
  Region r = XCreateRegion();
  XUnionRectWithRegion(&R, r, r);
  return r;
}
#endif

// Make the system's clip match the top of the clip stack.  This can
// be used after changing the stack, or to undo any clobbering of clip
// done by your program:
void fl_restore_clip() {
  Region r = rstack[rstackptr];
  fl_clip_state_number++;
#if USE_CAIRO
#elif USE_X11
  if (r) XSetRegion(xdisplay, gc, r);
  else XSetClipMask(xdisplay, gc, 0);
#elif defined(_WIN32)
  SelectClipRgn(dc, r); //if r is NULL, clip is automatically cleared
#elif defined(__APPLE__)
  /* //+++
  // We must intersect with the clip region for child windows:
  GrafPtr port; GDHandle GD; GetGWorld(&port, &GD);
  if ( port ) { // appaently this is zero for offscreen drawables
    if (r) {
      RgnHandle portClip = NewRgn();
      CopyRgn(CreatedWindow::find(Window::current())->subRegion, portClip );
      SectRgn( portClip, r, portClip );
      SetPortClipRegion( port, portClip );
      DisposeRgn( portClip );
    } else {
      SetPortClipRegion( port,
			 CreatedWindow::find(Window::current())->subRegion);
    }
  }*/
  if ( quartz_window )
  {
    GrafPtr port = GetWindowPort( quartz_window );
    if ( port ) { 
      RgnHandle portClip = NewRgn();
      CopyRgn( CreatedWindow::find(Window::current())->subRegion, portClip );
      if ( r )
        SectRgn( portClip, r, portClip );
      Rect portRect; GetPortBounds(port, &portRect);
      CreatedWindow::clear_quartz_clipping();
      ClipCGContextToRegion(quartz_gc, &portRect, portClip );
      CreatedWindow::fill_quartz_context();
      DisposeRgn( portClip );
    }
  }
#else
# error
#endif
}

/*! Replace the top of the clip stack. */
void fltk::clip_region(Region r) {
  Region oldr = rstack[rstackptr];
#if USE_X11
  if (oldr) XDestroyRegion(oldr);
#elif defined(_WIN32)
  if (oldr) DeleteObject(oldr);
#elif defined(__APPLE__)
  if (oldr) DisposeRgn(oldr);
#endif
  rstack[rstackptr] = r;
  fl_restore_clip();
}

/*!
  Pushes the \e intersection of the current region and this rectangle
  onto the clip stack. */
void fltk::push_clip(const Rectangle& r1) {
  Rectangle r(r1); transform(r);
  Region region;
  if (r.empty()) {
#if USE_X11
    region = XCreateRegion();
#elif defined(_WIN32)
    region = CreateRectRgn(0,0,0,0);
#elif defined(__APPLE__)
    region = NewRgn(); 
    SetEmptyRgn(region);
#else
# error
#endif
  } else {
    Region current = rstack[rstackptr];
#if USE_X11
    region = XRectangleRegion(r.x(), r.y(), r.w(), r.h());
    if (current) {
      Region temp = XCreateRegion();
      XIntersectRegion(current, region, temp);
      XDestroyRegion(region);
      region = temp;
    }
#elif defined(_WIN32)
    region = CreateRectRgn(r.x(), r.y(), r.r(), r.b());
    if (current) CombineRgn(region, region, current, RGN_AND);
#elif defined(__APPLE__)
    region = NewRgn();
    SetRectRgn(region, r.x(), r.y(), r.r(), r.b());
    if (current) SectRgn(region, current, region);
#else
# error
#endif
  }
  pushregion(region);
  fl_restore_clip();
}

/*!
  Remove the rectangle from the current clip region, thus making it a
  more complex shape. This does not push the stack, it just replaces
  the top of it.

  Some graphics backends (OpenGL and Cairo, at least) do not support
  non-rectangular clip regions. This call does nothing on those.
*/
void fltk::clipout(const Rectangle& r1) {
  Rectangle r(r1); transform(r);
  if (r.empty()) return;
#if USE_X11
  Region current = rstack[rstackptr];
  if (!current) current = XRectangleRegion(0,0,16383,16383);//?
  Region region = XRectangleRegion(r.x(), r.y(), r.w(), r.h());
  Region temp = XCreateRegion();
  XSubtractRegion(current, region, temp);
  XDestroyRegion(region);
  XDestroyRegion(current);
  rstack[rstackptr] = temp;
#elif defined(_WIN32)
  Region current = rstack[rstackptr];
  if (!current) current = CreateRectRgn(0,0,16383,16383);
  Region region = CreateRectRgn(r.x(), r.y(), r.r(), r.b());
  CombineRgn(current, current, region, RGN_DIFF);
  DeleteObject(region);
#elif defined(__APPLE__)
  Region current = rstack[rstackptr];
  if (!current) {current = NewRgn(); SetRectRgn(current, 0,0,16383,16383);}
  Region region = NewRgn();
  SetRectRgn(region, r.x(), r.y(), r.r(), r.b());
  DiffRgn(current, region, current);
#endif
  fl_restore_clip();
}

/*!
  Pushes an empty clip region on the stack so nothing will be
  clipped. This lets you draw outside the current clip region. This should
  only be used to temporarily ignore the clip region to draw into
  an offscreen area.
*/
void fltk::push_no_clip() {
  pushregion(0);
  fl_restore_clip();
}

/*! 
  Restore the previous clip region. You must call fltk::pop_clip()
  exactly once for every time you call fltk::push_clip(). If you return to
  FLTK with the clip stack not empty unpredictable results occur.
*/
void fltk::pop_clip() {
  if (rstackptr > 0) {
    Region oldr = rstack[rstackptr--];
#if USE_X11
    if (oldr) XDestroyRegion(oldr);
#elif defined(_WIN32)
    if (oldr) DeleteObject(oldr);
#elif defined(__APPLE__)
    if (oldr) DisposeRgn(oldr);
#endif
    fl_restore_clip();
  }
}

////////////////////////////////////////////////////////////////
// clipping tests:

/*! Returns true if any or all of the Rectangle is inside the
  clip region.
*/
bool fltk::not_clipped(const Rectangle& r1) {
  Rectangle r(r1); transform(r);
  // first check against the window so we get rid of coordinates
  // outside the 16-bit range the X/Win32 calls take:
  if (r.r() <= 0 || r.b() <= 0 || r.x() >= Window::current()->w()
      || r.y() >= Window::current()->h()) return false;
  Region region = rstack[rstackptr];
  if (!region) return true;
#if USE_X11
  return XRectInRegion(region, r.x(), r.y(), r.w(), r.h());
#elif defined(_WIN32)
  RECT rect;
  rect.left = r.x(); rect.top = r.y(); rect.right = r.r(); rect.bottom = r.b();
  return RectInRegion(region,&rect);
#elif defined(__APPLE__)
  Rect rect;
  rect.left = r.x(); rect.top = r.y(); rect.right = r.r(); rect.bottom = r.b();
  return RectInRgn(&rect, region);
#endif
}

/*!
  Intersect a \e transform()'d rectangle with the current clip
  region and change it to the smaller rectangle that surrounds (and
  probably equals) this intersection area.

  This can be used by device-specific drawing code to limit complex pixel
  operations (like drawing images) to the smallest rectangle needed to
  update the visible area.

  Return values:
  - 0 if it does not intersect, and W and H are set to zero.
  - 1 if if the result is equal to the rectangle (i.e. it is
    entirely inside or equal to the clip region)
  - 2 if it is partially clipped.
*/
int fltk::intersect_with_clip(Rectangle& r) {
  Region region = rstack[rstackptr];
  // If no clip region, claim it is not clipped. This is wrong because the
  // rectangle may be clipped by the window itself, but this test would
  // break the current draw-image-into-buffer code. This needs to be fixed
  // by replacing Window::current() below:
  if (!region) return 1;
  // Test against the window to get 16-bit values:
  int ret = 1;
  if (r.x() < 0) {r.set_x(0); ret = 2;}
  int t = Window::current()->w(); if (r.r() > t) {r.set_r(t); ret = 2;}
  if (r.y() < 0) {r.set_y(0); ret = 2;}
  t = Window::current()->h(); if (r.b() > t) {r.set_b(t); ret = 2;}
  // check for total clip (or for empty rectangle):
  if (r.empty()) return 0;
#if USE_X11
  switch (XRectInRegion(region, r.x(), r.y(), r.w(), r.h())) {
  case 0: // completely outside
    r.set(0,0,0,0);
    return 0;
  case 1: // completely inside:
    return ret;
  default: { // partial:
    Region rr = XRectangleRegion(r.x(), r.y(), r.w(), r.h());
    Region temp = XCreateRegion();
    XIntersectRegion(region, rr, temp);
    XRectangle xr;
    XClipBox(temp, &xr);
    r.set(xr.x, xr.y, xr.width, xr.height);
    XDestroyRegion(temp);
    XDestroyRegion(rr);
    return 2;}
  }
#elif defined(_WIN32)
// The win32 API makes no distinction between partial and complete
// intersection, so we have to check for partial intersection ourselves.
// However, given that the regions may be composite, we have to do
// some voodoo stuff...
  Region rr = CreateRectRgn(r.x(), r.y(), r.r(), r.b());
  Region temp = CreateRectRgn(0,0,0,0);
  if (CombineRgn(temp, rr, region, RGN_AND) == NULLREGION) { // disjoint
    r.set(0,0,0,0);
    ret = 0;
  } else if (EqualRgn(temp, rr)) { // complete
    // ret = ret
  } else {	// parital intersection
    RECT xr;
    GetRgnBox(temp, &xr);
    r.set(xr.left, xr.top, xr.right-xr.left, xr.bottom-xr.top);
    ret = 2;
  }
  DeleteObject(temp);
  DeleteObject(rr);
  return ret;
#elif defined(__APPLE__)
  RgnHandle rr = NewRgn();
  SetRectRgn(rr, r.x(), r.y(), r.r(), r.b());
  SectRgn(region, rr, rr);
  Rect rp; GetRegionBounds(rr, &rp);
  if (rp.bottom <= rp.top) ret = 0;
  else if (rp.right-rp.left < r.w() || rp.bottom-rp.top < r.h()) ret = 2;
  r.set(rp.left, rp.top, rp.right-rp.left, rp.bottom-rp.top);
  DisposeRgn(rr);
  return ret;
#else
# error
#endif
}

//
// End of "$Id: fl_clip.cxx,v 1.31 2005/01/25 20:11:46 matthiaswm Exp $"
//
