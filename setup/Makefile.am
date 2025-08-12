# vim:set noet ts=4:
#
# ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
#
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

setup_libpinyin_PYTHON = \
	main2.py \
	config.py \
	dicttreeview.py \
	enginefile.py \
	shortcuteditor.py \
	keyboardshortcut.py \
	ibus-libpinyin-preferences.ui \
	$(NULL)

setup_libpinyindir = $(datadir)/ibus-libpinyin/setup

libexec_SCRIPTS = \
	ibus-setup-libpinyin \
	$(NULL)

CLEANFILES = \
	*.pyc \
	config.py \
	Makefile.in \
	Makefile \
	$(NULL)

EXTRA_DIST = \
	config.py.in \
	ibus-setup-libpinyin.in \
	$(NULL)


test-libpinyin:
	$(ENV) DBUS_DEBUG=true \
		   IBUS_LOCALEDIR=@localedir@ \
		   IBUS_PKGDATADIR=$(pkgdatadir) \
		   PYTHONPATH=$(abs_top_srcdir):$(pyexecdir) \
		   $(PYTHON) $(srcdir)/main2.py libpinyin

test-libbopomofo:
	$(ENV) DBUS_DEBUG=true \
		   IBUS_LOCALEDIR=@localedir@ \
		   IBUS_PKGDATADIR=$(pkgdatadir) \
		   PYTHONPATH=$(abs_top_srcdir):$(pyexecdir) \
		   $(PYTHON) $(srcdir)/main2.py libbopomofo
