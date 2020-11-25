
INCLUDEPATH += /usr/lib/avr/include

HEADERS += \
    usb.h \
    ps2.h \
    ps2if.h \
    queue16.h \
    waitloop.h \
    avrgpio.h \
    lcd.h
SOURCES += \
    main.c \
    main.cpp \
    ps2dec001.cpp \
    ps2dec002.cpp \
    ps2if.cpp \
    quckey.cpp \
    queue16.cpp \
    waitloop.cpp \
    ps2dec.cpp \
    lcd.cpp \
    usb.c
