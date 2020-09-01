# vim:set et ts=4:
#
# ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# if not 1, append datestamp to the version number.
m4_define([ibus_released], [1])
m4_define([ibus_major_version], [1])
m4_define([ibus_minor_version], [11])
m4_define([ibus_micro_version], [94])
m4_define(ibus_maybe_datestamp,
    m4_esyscmd([if test x]ibus_released[ != x1; then date +.%Y%m%d | tr -d '\n\r'; fi]))

m4_define([ibus_version],
    ibus_major_version.ibus_minor_version.ibus_micro_version[]ibus_maybe_datestamp)

AC_INIT([ibus-libpinyin], [ibus_version], [https://github.com/libpinyin/ibus-libpinyin/issues/new],[ibus-libpinyin])
AM_INIT_AUTOMAKE([1.10])
AC_GNU_SOURCE

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# define PACKAGE_VERSION_* variables
AS_VERSION
AS_NANO
AM_SANITY_CHECK
AM_MAINTAINER_MODE
AM_DISABLE_STATIC
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
AC_PROG_SED

# check ibus
PKG_CHECK_MODULES(IBUS, [
    ibus-1.0 >= 1.5.11
])

# check sqlite
PKG_CHECK_MODULES(SQLITE, [
    sqlite3
])

AC_PATH_PROG(SQLITE3, sqlite3)
if test -z "$SQLITE3"; then
    AC_MSG_ERROR([could not find sqlite3 tool.])
fi

PKG_CHECK_MODULES(LIBPINYIN, [
    libpinyin >= 2.2.1
], [enable_libpinyin=yes])

LIBPINYIN_DATADIR=`$PKG_CONFIG --variable=pkgdatadir libpinyin`

AC_SUBST(LIBPINYIN_DATADIR)

# check if ibus_config_get_values, which is available in ibus-1.3.99+ (git master)
save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
CFLAGS="$CFLAGS $IBUS_CFLAGS"
LIBS="$LIBS $IBUS_LIBS"
AC_CHECK_FUNCS([ibus_config_get_values])
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"


# check env
AC_PATH_PROG(ENV, env)
AC_SUBST(ENV)

# check python
AC_ARG_WITH(python,
    AS_HELP_STRING([--with-python[=PATH]],
        [Select python2 or python3]),
    [PYTHON=$with_python], []
)
AM_PATH_PYTHON([3.2])

# check icon_prop_key in IBus.EngineDesc
AC_MSG_CHECKING([if IBus.EngineDesc has get_icon_prop_key])
ICON_PROP_KEY_TEST="import gi
gi.require_version('IBus', '1.0')
from gi.repository import IBus
exit(not hasattr(IBus.EngineDesc, 'get_icon_prop_key'))
"
$PYTHON -c "$ICON_PROP_KEY_TEST"
if test $? -eq 0; then
    ICON_PROP_KEY_XML="<icon_prop_key>InputMode</icon_prop_key>"
    AC_MSG_RESULT([yes])
else
    ICON_PROP_KEY_XML="<!-- <icon_prop_key>InputMode</icon_prop_key> -->"
    AC_MSG_RESULT([ibus 1.5.11 or later supports icon-prop-key in IBus.EngineDesc])
fi
AC_SUBST(ICON_PROP_KEY_XML)

# --enable-boost
AC_ARG_ENABLE(boost,
    AC_HELP_STRING([--enable-boost],
        [Use boost to replace stdc++0x.]),
        [enable_boost=$enableval],
        [enable_boost=no]
)
if test x"$enable_boost" = x"yes"; then
    # check boost
    BOOST_REQUIRE([1.39])
    BOOST_FIND_HEADER([boost/bind.hpp])
    BOOST_FIND_HEADER([boost/signals2.hpp])
fi
AM_CONDITIONAL(HAVE_BOOST, test x"$enable_boost" = x"yes")

# --enable-opencc
AC_ARG_ENABLE(opencc,
    AC_HELP_STRING([--enable-opencc],
        [Use opencc for simplified and traditional Chinese conversion]),
        [enable_opencc=$enableval],
        [enable_opencc=no]
)
if test x"$enable_opencc" = x"yes"; then
    # check opencc
    PKG_CHECK_MODULES(OPENCC, [opencc >= 1.0.0], [
        AC_DEFINE(HAVE_OPENCC, 1, [Define if found opencc])
    ])
fi

# define GETTEXT_* variables
GETTEXT_PACKAGE=ibus-libpinyin
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])

AM_GNU_GETTEXT_VERSION([0.19.7])
AM_GNU_GETTEXT([external])

# Checks for GSettings.
PKG_CHECK_MODULES(GIO2, [gio-2.0 >= 2.26.0])

GLIB_GSETTINGS

# --disable-lua-extension
AC_ARG_ENABLE(lua-extension,
    AS_HELP_STRING([--disable-lua-extension],
         [do not build lua extension]),
    [enable_lua_extension=$enableval],
    [enable_lua_extension=yes]
)

# check lua
PKG_CHECK_MODULES(LUA, [
    lua >= 5.1
], [],
[has_lua_extension=no]
)

if test x"$has_lua_extension" = x"no";
then
    PKG_CHECK_MODULES(LUA, [
        lua5.1
    ], [],
    [enable_lua_extension=no]
    );
fi

AM_CONDITIONAL(IBUS_BUILD_LUA_EXTENSION, [test x"$enable_lua_extension" = x"yes"])

# --enable-cloud-input-mode
AC_ARG_ENABLE(cloud-input-mode,
    AC_HELP_STRING([--enable-cloud-input-mode],
        [add cloud candidates]),
        [enable_cloud_input_mode=$enableval],
        [enable_cloud_input_mode=no]
)
if test x"$enable_cloud_input_mode" = x"yes"; then
    # check soup
    PKG_CHECK_MODULES(LIBSOUP, [libsoup-2.4 >= 2.26])
    AC_SUBST(LIBSOUP_CFLAGS)
    AC_SUBST(LIBSOUP_LIBS)

    # check json-glib
    PKG_CHECK_MODULES(JSONGLIB, [json-glib-1.0 >= 1.0])
    AC_SUBST(JSONGLIB_CFLAGS)
    AC_SUBST(JSONGLIB_LIBS)
fi
AM_CONDITIONAL(ENABLE_CLOUD_INPUT_MODE, test x"$enable_cloud_input_mode" = x"yes")

# --disable-english-input-mode
AC_ARG_ENABLE(english-input-mode,
    AS_HELP_STRING([--disable-english-input-mode],
    [do not build english input mode]),
    [enable_english_input_mode=$enableval],
    [enable_english_input_mode=yes]
)

AM_CONDITIONAL(IBUS_BUILD_ENGLISH_INPUT_MODE, [test x"$enable_english_input_mode" = x"yes"])

# --disable-stroke-input-mode
AC_ARG_ENABLE(stroke-input-mode,
        AS_HELP_STRING([--disable-stroke-input-mode],
        [do not build stroke input mode]),
        [enable_stroke_input_mode=$enableval],
        [enable_stroke_input_mode=yes]
)

AM_CONDITIONAL(IBUS_BUILD_STROKE_INPUT_MODE, [test x"$enable_stroke_input_mode" = x"yes"])

# OUTPUT files
AC_CONFIG_FILES([ po/Makefile.in
Makefile
ibus-libpinyin.spec
lua/Makefile
src/Makefile
src/libpinyin.xml.in
setup/Makefile
setup/ibus-setup-libpinyin
setup/config.py
data/Makefile
data/icons/Makefile
m4/Makefile
])

AC_OUTPUT

AC_MSG_RESULT([
Build options:
    Version                     $VERSION
    Install prefix              $prefix
    Use boost                   $enable_boost
    Use opencc                  $enable_opencc
    Use libpinyin               $enable_libpinyin
    Build lua extension         $enable_lua_extension
    Build cloud input mode      $enable_cloud_input_mode
    Build stroke input mode     $enable_stroke_input_mode
    Build english input mode    $enable_english_input_mode
])

