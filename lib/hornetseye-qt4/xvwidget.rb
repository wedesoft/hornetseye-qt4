# hornetseye-qt4 - Graphical output of images for Qt4
# Copyright (C) 2010 Jan Wedekind
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Namespace of Hornetseye computer vision library
module Hornetseye

  class XvWidget < Qt::Widget

    public

    alias_method :orig_write,:write

    def initialize( parent = nil, grab_port_now = true )
      super parent
      XvWidget.register self
      grabPort if grab_port_now
    end

    def closeEvent( e )
      releasePort
      XvWidget.unregister self
    end

    def inspect
      'XvWidget'
    end
  end

end

