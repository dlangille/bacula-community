#
# Simple Makefile for building plugins for Bacula
#
# Copyright (C) 2000-2023 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
# The script creates a docker image of the baculatar.py
# program. It uses also a static version of tar.
#
# It requires docker and pyinstaller (installed via pip3)
# The reference platform is bionic
#
D=$(shell date +%d%b%y)
all: bacula-backup-$(D).tar.gz

bacula-backup-$(D).tar.gz: baculatar.py tar Dockerfile
	docker build -t bacula-backup .
	docker tag bacula-backup:latest bacula-backup:$(D)
	docker save -o bacula-backup-$(D).tar bacula-backup:$(D)
	gzip -9 bacula-backup-$(D).tar

baculatar: baculatar.py
	pyinstaller baculatar.py
	./copylibs dist/baculatar/libpython*
	rm -rf dist build
	pyinstaller -F baculatar.py
	mv -f dist/baculatar .
	./copylibs baculatar

tar:
	rm -rf archbuild
	git clone https://github.com/ebl/tar-static.git archbuild
	(cd archbuild; ./build.sh)
	cp archbuild/releases/tar .
	rm -rf archbuild

clean:
	rm -rf baculatar.c bacula-backup-*.tar.gz __pycache__ baculatar.spec
	rm -rf dist build baculatar libs

distclean: clean
	rm -f tar

upload: bacula-backup-$(D).tar.gz
	rsync -avz bacula-backup-$(D).tar.gz bacula.org:/home/src/depkgs/
