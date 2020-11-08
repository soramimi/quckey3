
INCLUDEPATH += /usr/lib/avr/include

HEADERS += \
	avr.h \
	board.h \
    usb.h \
    ps2.h \
    ps2if.h \
    queue16.h \
    waitloop.h
SOURCES += \
    usb.c \
    main.c \
    main.cpp \
    board.cpp \
    ps2dec.cpp \
    ps2dec001.cpp \
    ps2dec002.cpp \
    ps2if.cpp \
    quckey.cpp \
    queue16.cpp \
    waitloop.cpp
