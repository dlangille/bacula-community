#
# Copyright (C) 2000-2023 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
# This is the makefile template for the platform directory
# which contains general platform installation.
#
#  17 August 2009 -- Lorenz Schori
#  29 Jun    2022 -- Eric Bollengier
# 
#   for Bacula 
#


# bacula version and download site
BACULA_VERSION ?= $(shell ../../scripts/getver ../../src/version.h)

SWIFT_APP_GIT ?= git@bsweb:macosx
SWIFT_APP_BRANCH ?= master

OPENSSL_VERSION_BASE ?= 1.1.1
OPENSSL_VERSION=$(shell curl https://www.openssl.org/source/ 2>/dev/null| perl -ne '/openssl-(${OPENSSL_VERSION_BASE}[a-z]+).tar.gz/ && print $$1')
OPENSSL_DL_URL:=https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz

LZO_VERSION=2.10
LZO_DL_URL=https://www.oberhumer.com/opensource/lzo/download/lzo-${LZO_VERSION}.tar.gz

# Build universal binary. Comment out when building versions of bacula < 3.0.0
ARCHFLAGS:=
MACOSX_SDK_SYSROOT:=
MACOSX_VERSION_FLAGS:=

# Tools
PB:=/usr/bin/pkgbuild
MAKE:=/usr/bin/make
CURL:=/usr/bin/curl
TAR:=/usr/bin/tar

########### you should not have to edit anything beyond this line ###########

# Build paths
DL_DIR:=dl
BUILD_DIR:=build
PRODUCTS_DIR:=products

# Can source ~/config to have all it set
# To unlock the security, use security unlock

# "Developer ID Application: Bacula YYYY (XXX)"
BIN_KEY ?= "Must specify BIN_KEY='Developer ID Application: Bacula YYYY (XXX)'"

# "Developer ID Installer: Bacula YYYY (XXXX)"
PRODUCT_KEY ?= "Must specify PRODUCT_KEY='Developer ID Installer: Bacula YYYY (XXXX)'"

APPLEID_USER ?= "Must specify APPLEID_USER='test@bacula.org'"
APPLEID_APP_PASS ?= "Must specify APPLEID_APP_PASS='xxx'"

WORKING_DIR:=${BUILD_DIR}
BACULA_TAR:=${DL_DIR}/bacula-${BACULA_VERSION}.tar.gz
BACULA_SOURCE:=${WORKING_DIR}/bacula-${BACULA_VERSION}
BACULA_DESTDIR:=${WORKING_DIR}/destdir
BACULA_PREFIX:=/Applications/Bacula.app/Contents/Resources/BaculaBin
BACULA_FD_CONF:=/Library/Preferences/bacula/bacula-fd.conf
BACULA_WORKING_DIR:=/private/var/bacula/working
BACULA_PMDOC:=${WORKING_DIR}/installer.pmdoc

# Detect whether we sit inside the Bacula source tree. In this case we won't
# download the tar from sourceforge but instead work with what is there
# already
CURSUB:=$(CURDIR:%/platforms/osx=%)
ifneq ($(CURDIR),$(CURSUB))
	BACULA_TAR:=
	BACULA_SOURCE:=../../
endif

NOW=$(shell date +%d%h%y-%H%M)

PACKAGE_TITLE:=bacula-client-${BACULA_VERSION}-${NOW}
PACKAGE_BASE:=org.bacula
PACKAGE_ID:=${PACKAGE_BASE}.bacula-fd.pkg
PACKAGE_DIR:=${PRODUCTS_DIR}/${PACKAGE_TITLE}
PACKAGE_BUNDLE:=${PACKAGE_DIR}/${PACKAGE_TITLE}.pkg
PACKAGE_DMG:=${PRODUCTS_DIR}/${PACKAGE_TITLE}.dmg
PACKAGE_RESOURCES:=ReadMe.html postflight preupgrade
PACKAGE_XRESOURCES:=postflight preupgrade

# Flags for the toolchain
CONFIGFLAGS:= \
  --enable-client-only \
  --prefix=${BACULA_PREFIX} \
  --with-dir-password=@DIR_PW@ \
  --with-fd-password=@FD_PW@ \
  --with-sd-password=@SD_PW@ \
  --with-mon-dir-password=@MON_DIR_PW@ \
  --with-mon-fd-password=@MON_FD_PW@ \
  --with-mon-sd-password=@MON_SD_PW@ \
  --with-basename=@BASENAME@ \
  --with-hostname=@HOSTNAME@ \
  --with-working-dir=@CONTAINER_PATH@ \
  --with-pid-dir=@CONTAINER_PATH@ \
  --with-openssl=${PWD}/openssl \
  --with-lzo=${PWD}/lzo
CPPFLAGS:=
CFLAGS:=-O -g
CXXFLAGS:=${CFLAGS}
LDFLAGS:=-framework CoreFoundation

# required on snow leopard: compiling for 10.4 requires usage of gcc 4.0
# system defaults to version 4.2
CC:=gcc
CPP:=cpp
CXX:=g++
CXXPP:=cpp

# Placeholders for *.in files
INFILE_SUBST=\
  -e "s,@PACKAGE_BASE@,${PACKAGE_BASE},g" \
  -e "s,@PREFIX@,${BACULA_PREFIX},g" \
  -e "s,@BACULA_VERSION@,${BACULA_VERSION},g" \
  -e "s,@FD_CONF@,${BACULA_FD_CONF},g" \
  -e "s,@BACULA_DESTDIR@,${BACULA_DESTDIR},g" \
  -e "s,@PACKAGE_ID@,${PACKAGE_ID},g"

all: sign-fd ${PACKAGE_TITLE}.pkg notarize-pkg

clean-fd:
	rm -rf ${CURDIR}/${BACULA_DESTDIR}

install-fd: clean-fd build-fd
	${MAKE} -C ${BACULA_SOURCE} install DESTDIR="${CURDIR}/${BACULA_DESTDIR}"
	${MAKE} -C ${BACULA_SOURCE}/src/tools install-bsnapshot DESTDIR="${CURDIR}/${BACULA_DESTDIR}"
	cp lzo/lib/*.dylib ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/lib
	cp lzo/share/doc/lzo/COPYING ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/share/doc/COPYING.lzo
	cp openssl/lib/*.dylib ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/lib
	cp -r openssl/lib/engines* ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/lib
	cp -r openssl/ssl ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/
	cp -r openssl/bin/* ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/sbin/
	cp -r openssl/bin/* ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/sbin/
	chmod 755 ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/sbin/*
	chmod 644 ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/lib/*.dylib
	chmod 755 ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/etc
	rm -rf "${BACULA_DESTDIR}/tmp"
	rm -rf "${BACULA_DESTDIR}/opt"
	rm -rf "${BACULA_DESTDIR}/private"
	rm -rf "${BACULA_DESTDIR}/usr"

	for conffile in ${BACULA_DESTDIR}${BACULA_PREFIX}/etc/*.conf; do \
		mv $$conffile $$conffile.example; \
	done

	chmod 644 ${CURDIR}/${BACULA_DESTDIR}/${BACULA_PREFIX}/etc/*.example

	mkdir -p "${BACULA_DESTDIR}${BACULA_PREFIX}/Library/LaunchDaemons"
	sed ${INFILE_SUBST} files/bacula-fd.plist.in \
		> "${BACULA_DESTDIR}${BACULA_PREFIX}/Library/LaunchDaemons/${PACKAGE_BASE}.bacula-fd.plist"

	mkdir -p "${BACULA_PMDOC}"
	for f in index.xml 01destdir.xml; do \
		sed ${INFILE_SUBST} \
			files/installer.pmdoc.in/$$f > "${BACULA_PMDOC}/$$f"; \
	done

	mkdir -p "${WORKING_DIR}/resources"

	for res in ${PACKAGE_RESOURCES}; do \
		sed ${INFILE_SUBST} \
			resources/$$res.in > "${WORKING_DIR}/resources/$$res"; \
	done

	for xres in ${PACKAGE_XRESOURCES}; do \
		chmod +x "${WORKING_DIR}/resources/$$xres"; \
	done

	cp "${BACULA_SOURCE}/LICENSE" "${WORKING_DIR}/resources/License.txt"


dmg: pkg
	rm -rf "${PACKAGE_DIR}"
	mkdir -p "${PACKAGE_DIR}"
	cp ${PACKAGE_TITLE}.pkg "${PACKAGE_DIR}"
	cp ${WORKING_DIR}/resources/ReadMe.html "${PACKAGE_DIR}/ReadMe.html"

	sed ${INFILE_SUBST} \
		files/uninstall.command.in > "${PACKAGE_DIR}/uninstall.command";
	chmod 0775 "${PACKAGE_DIR}/uninstall.command"

	hdiutil create -srcfolder "${PACKAGE_DIR}" "${PACKAGE_DMG}"

pkg: setup-pkg sign-pkg notarize-pkg

notarize-pkg: ${PACKAGE_TITLE}.pkg
	@echo "Sending the notarize request to Apple"
	xcrun altool --notarize-app --file $< --username $(APPLEID_USER) --password $(APPLEID_APP_PASS) --primary-bundle-id "${PACKAGE_BASE}.pkg" > 1
	@cat 1
	./wait-notarize 1

${PACKAGE_TITLE}.unsigned.pkg:
	${PB} --identifier "${PACKAGE_ID}" --version ${BACULA_VERSION} --install-location /Applications --root "${BACULA_DESTDIR}/Applications" ${PACKAGE_TITLE}.unsigned.pkg

sign-pkg: ${PACKAGE_TITLE}.pkg

${PACKAGE_TITLE}.pkg: ${PACKAGE_TITLE}.unsigned.pkg
	productsign --sign  "$(PRODUCT_KEY)" ${PACKAGE_TITLE}.unsigned.pkg ${PACKAGE_TITLE}.pkg
	rm ${PACKAGE_TITLE}.unsigned.pkg
	echo ${PACKAGE_TITLE}.pkg

sign-fd:  install-fd
	for binfile in ${BACULA_DESTDIR}${BACULA_PREFIX}/sbin/* ${BACULA_DESTDIR}${BACULA_PREFIX}/lib/*dylib ${BACULA_DESTDIR}${BACULA_PREFIX}/lib/*/*dylib ${BACULA_DESTDIR}${BACULA_PREFIX}/lib/*.so ; do \
		./update-libs ${BACULA_PREFIX} $$binfile ;\
		codesign  -v --force --timestamp --options=runtime --sign "$(BIN_KEY)" $$binfile; \
	done

build-fd: configure-fd
	make -C ${BACULA_SOURCE}

configure-fd: openssl lzo ${BACULA_SOURCE}/configure
	(cd ${BACULA_SOURCE} && ./configure ${CONFIGFLAGS} CPPFLAGS="${CPPFLAGS}" CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" CC="${CC}" CPP="${CPP}" CXX="${CXX}" CXXPP="${CXXPP}")
	grep " TLS support:" ${BACULA_SOURCE}/config.out | grep yes
	grep "Encryption support:" ${BACULA_SOURCE}/config.out | grep yes
	touch $@

lzo-${LZO_VERSION}.tar.gz:
	@echo "Download LZO ${LZO_VERSION}"
	${CURL} --output $@ ${LZO_DL_URL}

lzo-${LZO_VERSION}: lzo-${LZO_VERSION}.tar.gz
	tar xfz $< 

lzo: lzo-${LZO_VERSION} lzo-${LZO_VERSION}/Makefile
	rm -rf lzo
	make -j3 -C lzo-${LZO_VERSION}
	make -j3 -C lzo-${LZO_VERSION} install

lzo-${LZO_VERSION}/Makefile:
	(cd lzo-${LZO_VERSION} && ./configure --prefix=${PWD}/lzo --enable-shared)

openssl-${OPENSSL_VERSION}.tar.gz:
	@echo "Download ${OPENSSL_VERSION}"
	${CURL} --output $@ ${OPENSSL_DL_URL}

openssl-${OPENSSL_VERSION}: openssl-${OPENSSL_VERSION}.tar.gz
	tar xfz $< 

openssl: openssl-${OPENSSL_VERSION} openssl-${OPENSSL_VERSION}/Makefile
	rm -rf openssl
	make -j4 -C openssl-${OPENSSL_VERSION}
	make -j4 -C openssl-${OPENSSL_VERSION} install

openssl-${OPENSSL_VERSION}/Makefile:
	(cd openssl-${OPENSSL_VERSION} && ./config --prefix=${PWD}/openssl)

################################################################

unlock:
	security unlock

.PHONY: distclean
distclean: clean
	rm -rf build
	make -C ../../ distclean

.PHONY: clean cleanup
clean:
	rm -rf *.pkg *.dmg 1 products sign-pkg pkg check-pkg setup-swift-app configure-fd pkg Applications

cleanup:
	make -C ../../ clean
