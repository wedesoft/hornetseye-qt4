/* HornetsEye - Computer Vision with Ruby
   Copyright (C) 2006, 2007, 2008, 2009 Jan Wedekind
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#ifndef HORNETSEYE_XVWIDGET_HH
#define HORNETSEYE_XVWIDGET_HH

#include <QtGui/QWidget>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <set>
#include <map>
#include "error.hh"
#include "frame.hh"
#include "rubyinc.hh"

class XvManager
{
public:
  XvManager( QWidget *widget );
  void paint(void) throw (Error);
  void grabPort(void) throw (Error);
  void releasePort(void);
  void clear(void);
  void write( FramePtr frame ) throw (Error);
  static VALUE cRubyClass;
  static VALUE rbHornetseye;
  static VALUE registerRubyClass( VALUE module, VALUE cWidget );
  static VALUE wrapRegister( VALUE rbClass, VALUE rbWidget );
  static VALUE wrapUnregister( VALUE rbClass, VALUE rbWidget );
  static VALUE wrapGrabPort( VALUE rbSelf );
  static VALUE wrapReleasePort( VALUE rbSelf );
  static VALUE wrapClear( VALUE rbSelf );
  static VALUE wrapWrite( VALUE rbSelf, VALUE rbFrame );
  static VALUE wrapPaintEvent( VALUE rbSelf, VALUE rbPaintEvent );
protected:
  static std::set< XvPortID > grabbedPorts;
  static int typecodeToUID( std::string typecode );
  static std::string uidToTypecode( int uid );
  Atom findAtom( Display *display, const char *name ) throw (Error);
  FramePtr alignYV12( FramePtr frame );
  int selectFormat( Display *display, const int preferredUID ) throw (Error);
  QWidget *m_widget;
  XvPortID m_port;
  bool m_requireColourKey;
  int m_colourKey;
  XvImage *m_xvImage;
};

typedef boost::shared_ptr< XvManager > XvManagerPtr;

#endif

