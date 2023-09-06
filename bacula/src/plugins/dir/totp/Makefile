#
# Makefile for building FD plugins PluginLibrary for Bacula
#
# Copyright (C) 2000-2020 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
#  Author: Radoslaw Korzeniewski, radoslaw@korzeniewski.net
#

include ../../fd/Makefile.inc

TOTPSRC = totp-dir.c
TOTPOBJ = $(TOTPSRC:.c=.lo)

.SUFFIXES:    .c .lo

all: totp-dir.la

.c.lo:
	@echo "Compiling $< ..."
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${LIBDIR} -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -c $<

%.lo: %.c %.h
	@echo "Pattern compiling $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${LIBDIR}  -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -c $(@:.lo=.c)

../dirpluglib.lo:
	$(MAKE) -C ../ dirpluglib.lo

totp-dir.la:  $(TOTPOBJ)
	@echo "Linking $(@:.la=.so) ..."
	$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -shared $^ ../dirpluglib.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

btotp: $(TOTPSRC) ../dirpluglib.lo
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${LIBDIR} -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -DBTOTP_PROGRAM=1 -c $< -o btotp.lo
	$(LIBTOOL_LINK) $(CXX) $(DEFS) $(DEBUG) -I${SRCDIR} -I${LIBDIR} -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -o $@ btotp.lo ../dirpluglib.lo -L${LIBDIR} -lbac ${OPENSSL_LIBS}

btotp_test: $(TOTPSRC) ../dirpluglib.lo
	$(MAKE) -C ${LIBDIR} alist_test
	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${LIBDIR} -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -DTEST_PROGRAM=1 -c $< -o btotp_test.lo
	$(LIBTOOL_LINK) $(CXX) -I${SRCDIR} -I${LIBDIR} -I${SRCDIR}/dird -I.. -DSYSCONFDIR=\"$(sysconfdir)\" -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -o $@ btotp_test.lo ../dirpluglib.lo ${LIBDIR}/unittests.o -L${LIBDIR} -lbac ${OPENSSL_LIBS}

install-test: btotp_test
	$(NO_ECHO)$(MKDIR) $(DESTDIR)$(sbindir)
	$(NO_ECHO)$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) btotp_test $(DESTDIR)$(sbindir)

install: install-totp install-btotp

install-btotp: btotp
	@echo "Installing btotp"
	$(NO_ECHO)$(MKDIR) $(DESTDIR)$(sbindir)
	$(NO_ECHO)$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) btotp $(DESTDIR)$(sbindir)

install-totp: totp-dir.la
	@echo "Installing plugin ... $(^:.la=.so)"
	$(NO_ECHO)$(MKDIR) $(DESTDIR)$(plugindir)
	$(NO_ECHO)$(LIBTOOL_INSTALL) $(INSTALL_PROGRAM) totp-dir.la $(DESTDIR)$(plugindir)
	$(NO_ECHO)$(RMF) $(DESTDIR)$(plugindir)/totp-dir.la

$(LIBTOOL_CLEAN_TARGET):
	$(NO_ECHO)find . -name '*.lo' -print | xargs $(LIBTOOL_CLEAN) $(RMF)
	$(NO_ECHO)$(RMF) *.la
	$(NO_ECHO)$(RMF) -r .libs _libs

clean: $(LIBTOOL_CLEAN_TARGET)
	$(NO_ECHO)$(RMF) main btotp btotp_test *.so *.o

distclean: clean
	$(NO_ECHO)rm -f *.la *.lo
	$(NO_ECHO)rm -rf .libs

$(LIBTOOL_UNINSTALL_TARGET):
	$(LIBTOOL_UNINSTALL) $(RMF) $(DESTDIR)$(plugindir)/totp-dir.so

uninstall: $(LIBTOOL_UNINSTALL_TARGET)

depend:
