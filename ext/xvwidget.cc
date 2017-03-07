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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
extern "C" {
#ifndef HAVE_LIBSWSCALE_INCDIR
  #include <ffmpeg/swscale.h>
#else
  #include <libswscale/swscale.h>
#endif
}
#ifndef NDEBUG
#include <iostream>
#include <iomanip>
#endif

using namespace std;

#include <QtCore/QCoreApplication>
#include <QtGui/QX11Info>
#include "rubytools.hh"
#include "xvwidget.hh"

// also see xvideoimagepainter.cc

std::map< VALUE, XvManagerPtr > xvManager;

VALUE XvManager::cRubyClass = Qnil;
VALUE XvManager::rbHornetseye = Qnil;

std::set< XvPortID > XvManager::grabbedPorts;

XvManager::XvManager( QWidget *widget ):
  m_widget(widget), m_port(0), m_requireColourKey(false), m_colourKey(0),
  m_xvImage(NULL)
{
  m_widget->setAttribute( Qt::WA_NoSystemBackground );
  m_widget->setAttribute( Qt::WA_OpaquePaintEvent );
  m_widget->setAttribute( Qt::WA_PaintOnScreen );
  m_widget->setAutoFillBackground( false );
}

void XvManager::paint(void) throw (Error)
{
  Display *display = m_widget->x11Info().display();
  ERRORMACRO( display != NULL, Error, ,
              "Connection to X server does not exist" );
  int id = m_widget->winId();
  XGCValues xgcv;
  GC gc = XCreateGC( display, id, 0L, &xgcv );
  if ( m_xvImage != NULL ) {
    if ( m_requireColourKey ) {
      XSetForeground( display, gc, m_colourKey );
      XFillRectangle( display, id, gc, 0, 0,
                      m_widget->width(), m_widget->height() );
    };
  } else {
    XSetForeground( display, gc, 0 );
    XFillRectangle( display, id, gc, 0, 0,
                    m_widget->width(), m_widget->height() );
  };
  XFreeGC( display, gc );
}

void XvManager::grabPort(void) throw (Error)
{
  if ( m_port == 0 ) {
    // check whether m_port is zero !!!
    Display *display = m_widget->x11Info().display();
    ERRORMACRO( display != NULL, Error, ,
                "Connection to X server does not exist" );
    unsigned int ver, rel, req, ev, err;
    ERRORMACRO( XvQueryExtension( display, &ver, &rel, &req, &ev,
                                  &err ) == Success, Error, ,
                "Failure requesting X video extension" );
    unsigned int numAdaptors;
    XvAdaptorInfo *adaptorInfo = NULL;
    ERRORMACRO( XvQueryAdaptors( display,
                                 DefaultRootWindow( display ),// correct? !!!
                                 &numAdaptors, &adaptorInfo ) == Success,
                Error, , "Error requesting information about X video "
                "adaptors." );
    for ( int i=0; i<(signed)numAdaptors; i++ ) {
      if ( ( adaptorInfo[i].type & ( XvInputMask | XvImageMask ) ) ==
           ( XvInputMask | XvImageMask ) ) {
        for ( int p=adaptorInfo[i].base_id;
              p<(signed)(adaptorInfo[i].base_id+adaptorInfo[i].num_ports);
              p++ )
          // API does not seem to protect against grabbing a port
          // twice (from within the same process).
          if ( grabbedPorts.find( p ) == grabbedPorts.end() ) {
            if ( XvGrabPort( display, p, CurrentTime ) == Success ) {
#ifndef NDEBUG
              cerr << "Grabbed port " << p << endl;
#endif
              grabbedPorts.insert( p );
              m_port = p;
              break;
            };
          };
        if ( m_port != 0 )
          break;
      };
    };
    XvFreeAdaptorInfo( adaptorInfo );
    ERRORMACRO( m_port != 0, Error, ,
                "Could not grab a free port for X video output." );
    try {
      Atom xvColourKey = findAtom( display, "XV_COLORKEY" );
      if ( xvColourKey != None ) {
#ifndef NDEBUG
        cerr << "Require drawing of colourkey." << endl;
#endif
        m_requireColourKey = true;
        ERRORMACRO( XvGetPortAttribute( display, m_port, xvColourKey,
                                        &m_colourKey ) == Success,
                    Error, , "Error reading value of colour-key." );
#ifndef NDEBUG
        cerr << "Colour-key is 0x" << setw( 6 ) << setbase( 16 )
             << m_colourKey << setbase( 10 ) << setw( 0 ) << endl;
#endif
        Atom xvAutoPaint = findAtom( display, "XV_AUTOPAINT_COLORKEY" );
        if ( xvAutoPaint != None ) {
#ifndef NDEBUG
          cerr << "Disabling autopainting." << endl;
#endif
          XvSetPortAttribute( display, m_port, xvAutoPaint, 0 );
        } else {
#ifndef NDEBUG
          cerr << "Graphic card does not provide autopainting." << endl;
#endif
        };
      } else {
        m_requireColourKey = false;
#ifndef NDEBUG
        cerr << "No drawing of colourkey required." << endl;
#endif
      };
      
    } catch ( Error &e ) {
      
      XvUngrabPort( display, m_port, CurrentTime );
      grabbedPorts.erase( m_port );
      throw e;
      
    };
  };
}

void XvManager::releasePort(void)
{
  clear();
  if ( m_port != 0 ) {
    Display *display = m_widget->x11Info().display();
    if ( display != NULL )
      XvUngrabPort( display, m_port, CurrentTime );
    grabbedPorts.erase( m_port );
  };
}

void XvManager::clear(void)
{
  if ( m_xvImage != NULL ) {
    XFree( m_xvImage );
    m_xvImage = NULL;
    m_widget->update();
  };
}

void XvManager::write( FramePtr frame ) throw (Error)
{
  Display *display = m_widget->x11Info().display();
  ERRORMACRO( display != NULL, Error, ,
              "Connection to X server does not exist" );
  ERRORMACRO( m_port != 0, Error, ,
              "Need to grab XVideo port before image can be displayed." );
  if ( frame ) {
#ifndef NDEBUG
    cerr << "XVideo-output for \"" << frame->typecode() << "\"." << endl;
#endif
    int uid = selectFormat( display, typecodeToUID( frame->typecode() ) );
    ERRORMACRO( uid != 0, Error, , "XVideo-extension does not "
                "support YV12, I420, or RGB24" );
    string typecode = uidToTypecode( uid );
    if ( frame->typecode() != typecode ) {
      VALUE
        rbTypecode = rb_funcall( rbHornetseye, rb_intern( "const_get" ), 1,
                                 rb_str_new( typecode.c_str(),
                                             typecode.length() ) ),
        rbFrame = rb_funcall( frame->rubyObject(), rb_intern( "to_type" ), 1,
                              rbTypecode );
      frame = FramePtr( new Frame( rbFrame ) );
    };
    if ( m_xvImage != NULL ) {
      if ( m_xvImage->id != uid ||
           m_xvImage->width != frame->width() ||
           m_xvImage->height != frame->height() ) {
        XFree( m_xvImage );
        m_xvImage = NULL;
      };
    };
    if ( m_xvImage == NULL ) {
      m_xvImage =
        XvCreateImage( display, m_port, uid,
                       (char *)frame->data(),
                       frame->width(), frame->height() );
      m_widget->update();
#ifndef NDEBUG
      cerr << "Created " << m_xvImage->width << 'x'
           << m_xvImage->height << ' ' << frame->typecode()
           << "-image for X video." << endl;
      cerr << "# planes = " << m_xvImage->num_planes << endl;
      for ( int i=0; i<m_xvImage->num_planes; i++ )
        cerr << "offsets[" << i << "]=" << m_xvImage->offsets[i]
             << endl
             << "pitches[" << i << "]=" << m_xvImage->pitches[i]
             << endl;
#endif
    } else
      m_xvImage->data = (char *)frame->data();
    if ( frame->typecode() == "YV12" ) {
      // YV12 requires alignment for X video output.
      frame = alignYV12( frame );
      m_xvImage->data = (char *)frame->data();
    };
    XGCValues xgcv;
    int id = m_widget->winId();
    GC gc = XCreateGC( display, id, 0L, &xgcv );
    XvPutImage( display, m_port, id,
                gc, m_xvImage, 0, 0,
                m_xvImage->width, m_xvImage->height, 0, 0,
                m_widget->width(), m_widget->height() );
    XFreeGC( display, gc );
  };
}

int XvManager::selectFormat( Display *display, const int preferredUID )
  throw (Error)
{
  // Integrated GPL-licensed code from MPlayer (http://www.mplayerhq.hu/).
  assert( m_port != 0 );
  XvImageFormatValues *formats;
  int numFormats;
  formats = XvListImageFormats( display, m_port, &numFormats );
  ERRORMACRO( formats != NULL, Error, ,
              "Error requesting list of image formats." );
#ifndef NDEBUG
  for ( int i=0; i<numFormats; i++ ) {
    int id = formats[i].id;
    cerr << "format 0x" << setbase( 16 ) << id << setbase( 10 ) << ": '";
    cerr << (char)( id & 0xFF )
         << (char)( ( id >> 8 ) & 0xFF )
         << (char)( ( id >> 16 ) & 0xFF )
         << (char)( ( id >> 24 ) & 0xFF ) << "'";
    if ( formats[i].format == XvPacked )
      cerr << "(packed)" << endl;
    else
      cerr << "(planar)" << endl;
  };
#endif
  int retVal = 0;
  for ( int i=0; i<numFormats; i++ )
    if ( formats[i].id == preferredUID && preferredUID != 0 ) {
#ifndef NDEBUG
      cerr << "Found XVideo support for preferred colourspace ";
      cerr << "0x" << setbase( 16 ) << preferredUID << setbase( 10 ) << endl;
#endif
      retVal = preferredUID;
      break;
    } else if ( retVal == 0 && uidToTypecode( formats[i].id ) != "" ) {
#ifndef NDEBUG
      cerr << "Selecting \"" << uidToTypecode( formats[i].id )
           << "\" as colourspace for fallback" << endl;
#endif
      retVal = formats[i].id;
    }
  return retVal;
}

Atom XvManager::findAtom( Display *display, const char *name ) throw (Error)
{
  // Integrated GPL-licensed code from MPlayer (http://www.mplayerhq.hu/).
  assert( m_port != 0 );
  XvAttribute *attributes;
  int numAttributes;
  Atom retVal = None;
  attributes = XvQueryPortAttributes( display, m_port,
                                      &numAttributes );
  if (attributes != NULL) {
    for ( int i=0; i<numAttributes; i++ )
      if ( strcmp( attributes[i].name, name ) == 0 ) {
        retVal = XInternAtom( display, name, False );
        break;
      }
    XFree( attributes );
  }
  return retVal;
}

FramePtr XvManager::alignYV12( FramePtr frame )
{
  // YV12-images aligned by Xine sometimes do not fit the required
  // output.
  assert( m_xvImage != NULL );
  int
    width   = frame->width(),
    height  = frame->height(),
    width2  = ( width  + 1 ) / 2,
    height2 = ( height + 1 ) / 2,
    widtha  = ( width  + 7 ) & ~0x7,
    width2a = ( width2 + 7 ) & ~0x7;
  assert( m_xvImage->offsets[0] == 0 );
  FramePtr retVal;
  if ( m_xvImage->offsets[1] != widtha * height ||
       m_xvImage->offsets[2] != widtha * height + width2a * height2 ||
       m_xvImage->pitches[0] != widtha ||
       m_xvImage->pitches[1] != width2a ||
       m_xvImage->pitches[2] != width2a ) {
#ifndef NDEBUG
    cerr << "YV12-Image needs alignment." << endl;
#endif
    FramePtr dest( new Frame( "YV12",
                              m_xvImage->width, m_xvImage->height ) );
    uint8_t *sourceData[4];
    int sourceLineSize[4];
    sourceData[0] = (uint8_t *)frame->data();
    sourceData[2] = (uint8_t *)frame->data() + widtha * height;
    sourceData[1] = (uint8_t *)sourceData[2] + width2a * height2;
    sourceLineSize[0] = widtha;
    sourceLineSize[1] = width2a;
    sourceLineSize[2] = width2a;
    uint8_t *destData[4];
    int destLineSize[4];
    destData[0] = (uint8_t *)dest->data();
    destData[2] = (uint8_t *)dest->data() + m_xvImage->offsets[1];
    destData[1] = (uint8_t *)dest->data() + m_xvImage->offsets[2];
    destLineSize[0] = m_xvImage->pitches[0];
    destLineSize[1] = m_xvImage->pitches[2];
    destLineSize[2] = m_xvImage->pitches[1];
    SwsContext *swsContext = sws_getContext( width, height, AV_PIX_FMT_YUV420P,
                                             width, height, AV_PIX_FMT_YUV420P,
                                             SWS_FAST_BILINEAR, 0, 0, 0 );
    sws_scale( swsContext, sourceData, sourceLineSize, 0,
               height, destData, destLineSize );
    sws_freeContext( swsContext );
    retVal = dest;
  } else
    retVal = frame;
  return retVal;
}

int XvManager::typecodeToUID( string typecode )
{
  map< string, int > uid;
  uid[ "UBYTERGB" ] = 0x20424752;
  uid[ "YV12"     ] = 0x32315659;
  uid[ "I420"     ] = 0x30323449;
  map< string, int >::iterator i = uid.find( typecode );
  int retVal = 0;
  if ( i != uid.end() ) retVal = i->second;
#ifndef NDEBUG
  if ( retVal != 0 )
    cerr << "uid for \"" << typecode << "\" is 0x"
         << setbase( 16 ) << retVal << setbase( 10 ) << endl;
  else
    cerr << "uid for \"" << typecode << "\" was not found";
#endif
  return retVal;
}

string XvManager::uidToTypecode( int uid )
{
  map< int, string > typecode;
  typecode[ 0x20424752 ] = "UBYTERGB";
  typecode[ 0x32315659 ] = "YV12";
  typecode[ 0x30323449 ] = "I420";
  map< int, string >::iterator i = typecode.find( uid );
  string retVal = "";
  if ( i != typecode.end() ) retVal = i->second;
#ifndef NDEBUG
  if ( retVal != "" )
    cerr << "0x" << setbase( 16 ) << uid << setbase( 10 )
         << " designates colourspace \"" << retVal << "\"" << endl;
  else
    cerr << "0x" << setbase( 16 ) << uid << setbase( 10 )
         << " does not designate a known colourspace" << endl;
#endif
  return retVal;
}

VALUE XvManager::registerRubyClass( VALUE module, VALUE cWidget )
{
  cRubyClass = rb_define_class_under( module, "XvWidget", cWidget );
  rbHornetseye = module;
  rb_define_singleton_method( cRubyClass, "register",
                              RUBY_METHOD_FUNC( XvManager::wrapRegister ),
                              1 );
  rb_define_singleton_method( cRubyClass, "unregister",
                              RUBY_METHOD_FUNC( XvManager::wrapUnregister ),
                              1 );
  rb_define_method( cRubyClass, "paintEvent",
                    RUBY_METHOD_FUNC( XvManager::wrapPaintEvent ), 1 );
  rb_define_method( cRubyClass, "grabPort",
                    RUBY_METHOD_FUNC( XvManager::wrapGrabPort ), 0 );
  rb_define_method( cRubyClass, "releasePort",
                    RUBY_METHOD_FUNC( XvManager::wrapReleasePort ), 0 );
  rb_define_method( cRubyClass, "clear",
                    RUBY_METHOD_FUNC( XvManager::wrapClear ), 0 );
  rb_define_method( cRubyClass, "write",
                    RUBY_METHOD_FUNC( XvManager::wrapWrite ), 1 );
  return cRubyClass;
}

VALUE XvManager::wrapRegister( VALUE rbClass, VALUE rbWidget )
{
#ifndef NDEBUG
  cerr << "Registering custom widget ... " << flush;
#endif
  const char *name = "HORNETSEYE-XVWIDGETFISHING";
  QObject object( QCoreApplication::instance() );
  object.setObjectName( name );
  VALUE rbQObject =
    rb_eval_string( "Qt::CoreApplication.instance.findChild"
                    "( Qt::Object, 'HORNETSEYE-XVWIDGETFISHING' )" );
  rb_funcall( rbQObject, rb_intern( "parent=" ), 1, rbWidget );
  xvManager[ rbWidget ] =
    XvManagerPtr( new XvManager( (QWidget *)object.parent() ) );
#ifndef NDEBUG
  cerr << "done" << endl;
#endif
  return rbWidget;
}

VALUE XvManager::wrapUnregister( VALUE rbClass, VALUE rbWidget )
{
  xvManager.erase( rbWidget );
  return rbWidget;
}

VALUE XvManager::wrapGrabPort( VALUE rbSelf )
{
  try {
    assert( xvManager[ rbSelf ] );
    xvManager[ rbSelf ]->grabPort();
  } catch ( std::exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  }
  return rbSelf;
}

VALUE XvManager::wrapReleasePort( VALUE rbSelf )
{
  assert( xvManager[ rbSelf ] );
  xvManager[ rbSelf ]->releasePort();
  return rbSelf;
}

VALUE XvManager::wrapClear( VALUE rbSelf )
{
  assert( xvManager[ rbSelf ] );
  xvManager[ rbSelf ]->clear();
  return rbSelf;
}

VALUE XvManager::wrapWrite( VALUE rbSelf, VALUE rbFrame )
{
  try {
    FramePtr frame( new Frame( rbFrame ) );
    assert( xvManager[ rbSelf ] );
    xvManager[ rbSelf ]->write( frame );
  } catch ( std::exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  }
  return rbFrame;
}

VALUE XvManager::wrapPaintEvent( VALUE rbSelf, VALUE rbPaintEvent )
{
  try {
    assert( xvManager[ rbSelf ] );
    xvManager[ rbSelf ]->paint();
  } catch ( std::exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  }
  return rbSelf;
}

