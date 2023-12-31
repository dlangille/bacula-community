#
# Makefile for win32 bacula executables
# Using MinGW cross-compiler on GNU/Linux
#  
#
# Author: Howard Thomson
# License: BSD 2-Clause; see file LICENSE-FOSS
#
#  Written for Bacula by Howard Thomson, April 2006
# 

include ../Makefile.inc

INCLUDES = \
	$(INCLUDE_GCC) \
	$(INCLUDE_MINGW) \
	$(INCLUDE_PTHREADS) \
	$(INCLUDE_BACULA) \
	$(INCLUDE_ZLIB) \
	$(INCLUDE_VSS) \
	$(INCLUDE_ICONS) \
	$(INCLUDE_OPENSSL)

DEFINES = \
	-DWIN32 \
	$(HAVES)

######################################################################

# Files files in src/win32/compat

LIB_OBJS = \
	$(OBJDIR)/compat.o \
	$(OBJDIR)/getopt.o \
	$(OBJDIR)/print.o \
	$(OBJDIR)/vss.o \
	$(OBJDIR)/vss_XP.o \
	$(OBJDIR)/vss_W2K3.o \
	$(OBJDIR)/vss_Vista.o

######################################################################

# Targets

.PHONY: all clean

all: $(LIBDIR)/libcompat.a

clean:
	@echo "Cleaning `pwd`"
	$(ECHO_CMD)rm -f $(OBJDIR)/*.[od] $(LIBDIR)/libcompat.a

#
# Rules
# 

$(LIBDIR)/libcompat.a: $(LIB_OBJS)
	@echo "Updating archive $@"
	$(call checkdir,$@)
	$(ECHO_CMD)$(AR) rs $@ $^

include ../Makefile.rules

ifneq ($(MAKECMDGOALS),clean)
include $(patsubst %.o,%.d,$(filter-out %.res,$(LIB_OBJS)))
endif
