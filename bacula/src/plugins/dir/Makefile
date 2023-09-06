#
# Simple Makefile for building test FD plugins for Bacula
#
# Copyright (C) 2000-2023 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#

include Makefile.inc

.c.lo:
	@echo "Compiling $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${DIRDIR} -I${LIBDIR} -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -c $<

%.lo: %.c %.h
	@echo "Pattern compiling $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${DIRDIR} -I${LIBDIR} -DWORKDIR=\"$(DESTDIR)$(working_dir)\" -c $(@:.lo=.c)

all: dirpluglib.lo example-plugin-dir.la test-authentication-api-dir.la

install-totp:
	$(MAKE) -C totp/ install

# example-plugin-dir.lo: example-plugin-dir.c ${DIRDIR}/dir_plugins.h
# 	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CFLAGS) -I../.. -I${DIRDIR} -c example-plugin-dir.c

example-plugin-dir.la: example-plugin-dir.lo
	@echo "Linking $(@:.la=.so) ..."
	$(NO_ECHO)$(LIBTOOL_LINK) $(CXX) $(LDFLAGS) -shared example-plugin-dir.lo -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

# test-authentication-api-dir.lo: test-authentication-api-dir.c ${DIRDIR}/dir_plugins.h
# 	$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CFLAGS) -I../.. -I${DIRDIR} -c test-authentication-api-dir.c

test-authentication-api-dir.la: test-authentication-api-dir.lo dirpluglib.lo
	@echo "Linking $(@:.la=.so) ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -shared $^ -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

install: all $(DIRPLUG_INSTALL_TARGET)
	$(NO_ECHO)$(MKDIR) $(DESTDIR)$(plugindir)

install-bpam-ldap:
	$(MAKE) -C ldap install

$(LIBTOOL_CLEAN_TARGET):
	$(NO_ECHO)find . -name '*.lo' -print | xargs $(LIBTOOL_CLEAN) $(RMF)
	$(NO_ECHO)$(RMF) *.la
	$(NO_ECHO)$(RMF) -r .libs _libs

clean: $(LIBTOOL_CLEAN_TARGET)
	$(NO_ECHO)rm -f main *.so *.o 1 2 3

distclean: clean
	$(NO_ECHO)rm -f Makefile

$(LIBTOOL_UNINSTALL_TARGET):
	$(NO_ECHO)$(LIBTOOL_UNINSTALL) $(RMF) $(DESTDIR)$(plugindir)/example-plugin-dir.la

uninstall: $(LIBTOOL_UNINSTALL_TARGET)

depend:
