######################################################################
# Version $Id$
#
CONFIG += qt debug

TEMPLATE = app
TARGET = bat
DEPENDPATH += .
INCLUDEPATH += . ./console ./restore ./select
INCLUDEPATH += ..
LIBS        += -L../lib
LIBS        += -lbac
LIBS        += -lssl -lcrypto
RESOURCES = main.qrc
MOC_DIR = moc
OBJECTS_DIR = obj
UI_DIR = ui

# Main window
FORMS += main.ui
FORMS += label/label.ui
FORMS += console/console.ui
FORMS += restore/restore.ui restore/prerestore.ui restore/brestore.ui
FORMS += run/run.ui run/runcmd.ui
FORMS += select/select.ui
FORMS += medialist/medialist.ui mediaedit/mediaedit.ui joblist/joblist.ui
FORMS += clients/clients.ui

HEADERS += mainwin.h bat.h bat_conf.h qstd.h
SOURCES += main.cpp bat_conf.cpp mainwin.cpp qstd.cpp

# Console
HEADERS += console/console.h
SOURCES += console/authenticate.cpp console/console.cpp

# Restore
HEADERS += restore/restore.h
SOURCES += restore/prerestore.cpp restore/restore.cpp restore/brestore.cpp

# Label dialog
HEADERS += label/label.h
SOURCES += label/label.cpp

# Run dialog
HEADERS += run/run.h
SOURCES += run/run.cpp run/runcmd.cpp

# Select dialog
HEADERS += select/select.h
SOURCES += select/select.cpp

## Pages
HEADERS += pages.h
SOURCES += pages.cpp

## MediaList
HEADERS += medialist/medialist.h
SOURCES += medialist/medialist.cpp

## MediaEdit
HEADERS += mediaedit/mediaedit.h
SOURCES += mediaedit/mediaedit.cpp

## JobList
HEADERS += joblist/joblist.h
SOURCES += joblist/joblist.cpp

## Clients
HEADERS += clients/clients.h
SOURCES += clients/clients.cpp
