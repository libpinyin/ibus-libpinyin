# vim:set noet ts=4:
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

if IBUS_BUILD_LUA_EXTENSION
LUADIR=lua
endif

SUBDIRS = \
	data \
	$(LUADIR) \
	src \
	setup \
	m4 \
	po \
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	CFLAGS='-g3 -O3 -Wall' \
	CXXFLAGS='-g3 -O3 -Wall' \
	$(NULL)

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath  \
	autogen.sh \
	ibus-libpinyin.spec.in \
	$(NULL)

noinst_DIST = \
	$(NULL)


DISTCLEANFILES = \
	po/stamp-it \
	$(NULL)

dist-hook:
	if test -d .git ; then \
		git log --name-status --date=iso > $(distdir)/ChangeLog ; \
	fi

rpm: dist @PACKAGE_NAME@.spec
	rpmbuild -bb \
			--define "_sourcedir `pwd`" \
			--define "_builddir `pwd`/rpm" \
			--define "_srcrpmdir `pwd`/rpm" \
			--define "_rpmdir `pwd`/rpm" \
			--define "_specdir `pwd`" \
			@PACKAGE_NAME@.spec

srpm: dist @PACKAGE_NAME@.spec
	rpmbuild -bs \
			--define "_sourcedir `pwd`" \
			--define "_builddir `pwd`/rpm" \
			--define "_srcrpmdir `pwd`/rpm" \
			--define "_rpmdir `pwd`/rpm" \
			--define "_specdir `pwd`" \
			@PACKAGE_NAME@.spec

.PHONY: debian/changelog
debian/changelog:
	$(AM_V_GEN) \
	( \
		. /etc/lsb-release; \
		date=`date -R`; \
		version=@VERSION@; \
		serie=$(serie); \
		if test -z "$$serie"; then \
		  serie=$$DISTRIB_CODENAME; \
		fi; \
		if test -z "$$release"; then \
		  release=1; \
		fi; \
		s=`cat debian/changelog.in`; \
		eval "echo \"$${s}\""; \
	) > $@

ppa: dist debian/changelog
	$(AM_V_GEN) \
	( \
		mkdir ppa; \
		cp $(distdir).tar.gz ppa/@PACKAGE_NAME@_@PACKAGE_VERSION@.orig.tar.gz ; \
		cd ppa; \
		tar zxvf ../$(distdir).tar.gz ; \
		cd $(distdir); \
		cp -a ../../debian . ; \
		cd debian; \
		debuild -S -sa ; \
	)

dpkg: dist debian/changelog
	$(AM_V_GEN) \
	( \
		mkdir ppa; \
		cd ppa; \
		tar zxvf ../$(distdir).tar.gz ; \
		cd $(distdir); \
		cp -a ../../debian . ; \
		cd debian; \
		debuild -b -uc -us; \
	)

clean-rpm:
	$(RM) -r "`uname -i`"

clean-local: clean-rpm
