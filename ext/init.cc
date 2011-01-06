/* HornetsEye - Computer Vision with Ruby
   Copyright (C) 2006, 2007, 2008, 2009, 2010   Jan Wedekind

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
#include "rubyinc.hh"
#include "xvwidget.hh"

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#define DLLLOCAL
#else
#define DLLEXPORT __attribute__ ((visibility("default")))
#define DLLLOCAL __attribute__ ((visibility("hidden")))
#endif

extern "C" DLLEXPORT void Init_hornetseye_qt4(void);

extern "C" {

  void Init_hornetseye_qt4(void)
  {
    // XInitThreads();
    rb_require( "hornetseye_frame" );
    rb_require( "Qt4" );
    VALUE rbHornetseye = rb_define_module( "Hornetseye" );
    VALUE rbQt = rb_define_module( "Qt" );
    VALUE cWidget = rb_const_get( rbQt, rb_intern( "Widget" ) );
    XvManager::registerRubyClass( rbHornetseye, cWidget );
    rb_require( "hornetseye_qt4_ext.rb" );
  }

}