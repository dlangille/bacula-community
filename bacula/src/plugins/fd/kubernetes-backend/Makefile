#
# Master Makefile
#
# Copyright (C) 2000-2020 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#

include ../Makefile.inc

CWD=$(shell pwd)
PIP_PROG=$(shell ./get_python PIP)
PYTHON_PROG=$(shell ./get_python PYTHON)
PYTHONPATH=$(shell ./get_python PYTHONPATH)
PYTHON_PREFIX=$(shell ./get_python PYTHON_PREFIX)

all: pbuild

clean:
	@find . -name '__pycache__' -print | xargs -r rm -r
	@$(PYTHON_PROG) setup.py clean

dist-clean: clean
	@rm -rf build dist compile.py *.spec install-deps

build: pbuild

pbuild:
	@$(PYTHON_PROG) setup.py build

install: build
	$(MAKE) -C ../ install-kubernetes
	@$(PYTHON_PROG) setup.py install

install-deps: requirements.txt
	PYTHONPATH=$(PYTHONPATH) $(PIP_PROG) install --break-system-packages -r requirements.txt
	touch install-deps

binary: install-deps
	find baculak8s -name '*.py' | ./mkExt.pl > compile.py
	PYTHONPATH=$(PYTHONPATH) $(PYTHON_PROG) compile.py build
	$(CWD)/make_bin

install-bin: install-kubernetes

install-kubernetes: binary
	$(INSTALL_PROGRAM) dist/k8s_backend $(DESTDIR)$(sbindir)
