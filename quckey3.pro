
INCLUDEPATH += /usr/lib/avr/include

HEADERS += \
	avr.h \
	board.h \
    usb.h \
    ps2.h \
    ps2if.h \
    queue16.h \
    typedef.h \
    waitloop.h
SOURCES += \
	board.c \
    usb.c \
    main.c \
    main.cpp \
    ps2dec.c \
    ps2dec001.c \
    ps2dec002.c \
    ps2if.c \
    quckey.c \
    queue16.c \
    waitloop.c
