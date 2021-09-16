#
# Makefile for building FD plugins PluginLibrary for Bacula
#
# Copyright (C) 2000-2020 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
#  Author: Radoslaw Korzeniewski, radoslaw@korzeniewski.net
#

include ../Makefile.inc

PLUGINLIBSSRC = pluginlib.cpp pluginlib.h
PLUGINLIBSOBJ = $(filter %.lo,$(PLUGINLIBSSRC:.cpp=.lo))
ISO8601SRC = iso8601.cpp iso8601.h
ISO8601OBJ = $(filter %.lo,$(ISO8601SRC:.cpp=.lo))
EXECPROGSRC = execprog.cpp execprog.h
EXECPROGOBJ = $(filter %.lo,$(EXECPROGSRC:.cpp=.lo))
PTCOMMSRC = ptcomm.cpp ptcomm.h
PTCOMMOBJ = $(filter %.lo,$(PTCOMMSRC:.cpp=.lo))
PLUGINBASESRC = pluginbase.h   # pluginbase.cpp
PLUGINBASEOBJ = $(filter %.lo,$(PLUGINBASESRC:.cpp=.lo))
PLUGINCLASSSRC = pluginclass.h # pluginclass.cpp
PLUGINCLASSOBJ = $(filter %.lo,$(PLUGINCLASSSRC:.cpp=.lo))
PLUGINCTXSRC = pluginctx.h pluginctx.cpp
PLUGINCTXOBJ = $(filter %.lo,$(PLUGINCTXSRC:.cpp=.lo))
COMMCTXSRC = commctx.h
SMARTALISTSRC = smartalist.h
SMARTPTRSRC = smartptr.h
SMARTMUTEXSRC = smartmutex.h
SMARTLOCKSRC = smartlock.h
METAPLUGINSRC = metaplugin.cpp metaplugin.h metaplugin_attributes.cpp metaplugin_attributes.h metaplugin_accurate.cpp metaplugin_accurate.h metaplugin_metadata.cpp metaplugin_metadata.h $(SMARTMUTEXSRC) $(SMARTLOCKSRC) $(COMMCTXSRC)
METAPLUGINOBJ = $(filter %.lo,$(METAPLUGINSRC:.cpp=.lo))

PLUGINLIBSTEST = pluginlib_test.cpp $(PLUGINLIBSSRC) $(UNITTESTSOBJ) $(INIOBJ)
PLUGINLIBSTESTOBJ = $(filter %.lo,$(PLUGINLIBSTEST:.cpp=.lo))
ISO8601TEST = iso8601_test.cpp $(ISO8601SRC) $(UNITTESTSOBJ)
ISO8601TESTOBJ = $(filter %.lo,$(ISO8601TEST:.cpp=.lo))
COMMCTXTEST = commctx_test.cpp $(COMMCTXSRC) $(SMARTALISTSRC) $(PLUGINLIBSOBJ) $(UNITTESTSOBJ) $(INIOBJ)
COMMCTXTESTOBJ = $(filter %.lo,$(COMMCTXTEST:.cpp=.lo))
SMARTALISTTEST = smartalist_test.cpp $(SMARTALISTSRC) $(PLUGINLIBSOBJ) $(UNITTESTSOBJ) $(INIOBJ)
SMARTALISTTESTOBJ = $(filter %.lo,$(SMARTALISTTEST:.cpp=.lo))
SMARTPTRTEST = smartptr_test.cpp $(SMARTPTRSRC) $(PLUGINLIBSOBJ) $(UNITTESTSOBJ) $(INIOBJ)
SMARTPTRTESTOBJ = $(filter %.lo,$(SMARTPTRTEST:.cpp=.lo))
SMARTLOCKTEST = smartlock_test.cpp $(SMARTLOCKSRC) $(SMARTMUTEXSRC) $(UNITTESTSOBJ)
SMARTLOCKTESTOBJ = $(filter %.lo,$(SMARTLOCKTEST:.cpp=.lo))

METAPLUGINTEST = metaplugin_test.cpp $(METAPLUGINSRC)
METAPLUGINTESTOBJ = $(filter %.lo,$(METAPLUGINTEST:.cpp=.lo)) $(PTCOMMOBJ) $(PLUGINLIBSOBJ) $(METAPLUGINOBJ) $(UNITTESTSOBJ) $(LIBBACOBJ) $(LIBBACCFGOBJ)

TESTMETAPLUGINBACKENDSRC = test_metaplugin_backend.c
TESTMETAPLUGINBACKENDOBJ = $(TESTMETAPLUGINBACKENDSRC:.c=.lo)

# COMMONPLUGINOBJ = $(PLUGINLIBSOBJ) $(ISO8601OBJ) $(EXECPROGOBJ)
COMMONPLUGINOBJ = $(PLUGINLIBSOBJ) $(PTCOMMOBJ) $(METAPLUGINOBJ) $(PLUGINCLASSOBJ) $(PLUGINBASEOBJ)
# COMMONPLUGINTESTS = iso8601_test
COMMONPLUGINTESTS = pluginlib_test smartalist_test smartptr_test commctx_test

.c.lo:
	@echo "Compiling $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I$(SRCDIR) -I$(FDDIR) -I$(FDPLUGDIR) -I$(LIBDIR) -I$(FINDLIBDIR) -I. -c $<

.cpp.lo:
	@echo "Compiling c++ $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I$(SRCDIR) -I$(FDDIR) -I$(FDPLUGDIR) -I$(LIBDIR) -I$(FINDLIBDIR) -I. -c $<

%.lo: %.cpp %.h
	@echo "Pattern compiling c++ $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I$(SRCDIR) -I$(FDDIR) -I$(FDPLUGDIR) -I$(LIBDIR) -I$(FINDLIBDIR) -I. -c $<

all: $(COMMONPLUGINOBJ) # $(TESTMETAPLUGINBACKENDOBJ)

tests: $(COMMONPLUGINTESTS)

test_metaplugin_backend.lo: $(TESTMETAPLUGINBACKENDSRC)
	@echo "Compiling backend $< ..."
	$(NO_ECHO)$(LIBTOOL_COMPILE) $(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) $(CFLAGS) -I${SRCDIR} -I${FDDIR} -DLOGDIR=\"$(DESTDIR)$(working_dir)\" -c $<

test_metaplugin_backend_fd: $(TESTMETAPLUGINBACKENDOBJ) test_metaplugin_backend_fd.lo
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) test_metaplugin_backend_fd.lo $(TESTMETAPLUGINBACKENDOBJ) -o $@

test_metaplugin-fd.la: $(PTCOMMOBJ) $(PLUGINLIBSOBJ) $(METAPLUGINOBJ) test_metaplugin-fd.lo
	@echo "Linking $(@:.la=.so) ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -shared $^ -o $@ -rpath $(plugindir) -module -export-dynamic -avoid-version

pluginlib_test: $(PLUGINLIBSTESTOBJ) $(PLUGINLIBSTEST) $(UNITTESTSOBJ) $(LIBBACOBJ)
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) -lbac $(PLUGINLIBSTESTOBJ) -o $@

smartalist_test: $(SMARTALISTTESTOBJ) $(SMARTALISTTEST) $(UNITTESTSOBJ) $(LIBBACOBJ)
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) -lbac $(SMARTALISTTESTOBJ) -o $@

smartptr_test: $(SMARTPTRTESTOBJ) $(SMARTPTRTEST) $(UNITTESTSOBJ) $(LIBBACOBJ)
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) -lbac $(SMARTPTRTESTOBJ) -o $@

commctx_test: $(COMMCTXTESTOBJ) $(COMMCTXTEST) $(UNITTESTSOBJ) $(LIBBACOBJ)
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) -lbac $(COMMCTXTESTOBJ) -o $@

iso8601_test: $(ISO8601TESTOBJ) $(ISO8601SRC) $(UNITTESTSOBJ) $(LIBBACOBJ)
	@echo "Building $@ ..."
	$(NO_ECHO)$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) -lbac $(ISO8601TESTOBJ) -o $@

metaplugin_test: $(METAPLUGINTESTOBJ)
	@echo "Building $@ ..."
	$(LIBTOOL_LINK) --silent $(CXX) $(LDFLAGS) -L$(LIBDIR) $(LIBBACCFGOBJ) $(LIBBACOBJ) $(METAPLUGINTESTOBJ) -o $@

#$(NO_ECHO)

install: all
	$(MKDIR) $(DESTDIR)$(plugindir)

libtool-clean:
	@find . -name '*.lo' -print | xargs $(LIBTOOL_CLEAN) $(RMF)
	@find . -name '*.la' -print | xargs $(LIBTOOL_CLEAN) $(RMF)
	@$(RMF) -r .libs _libs

clean: libtool-clean
	@rm -f main *.so *.o
	@rm -f $(COMMONPLUGINTESTS) test_metaplugin_backend_fd

distclean: clean

libtool-uninstall:

depend:

uninstall: $(LIBTOOL_UNINSTALL_TARGET)
