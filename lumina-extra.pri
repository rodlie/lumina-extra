#
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
#

isEmpty(PREFIX) {
    PREFIX = /usr/local
    isEmpty(XDGDIR) {
        XDGDIR = $${PREFIX}/etc/xdg
    }
}
isEmpty(DOCDIR) {
    DOCDIR = $$PREFIX/share/doc
}
isEmpty(XDGDIR) {
    XDGDIR = /etc/xdg
}
isEmpty(PIXEL_PLUGINS) {
    PIXEL_PLUGINS = $${PREFIX}/lib$${LIBSUFFIX}/lumina/pixel
}

CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPUT

CONFIG -= install_lib
CONFIG += staticlib

VERSION = 20180601
