QT += qml qml-private quick-private core-private

SOURCES += \
    main.cpp \
    qmlstreamwriter.cpp

HEADERS += \
    qmlstreamwriter.h

OTHER_FILES += Info.plist
macx: QMAKE_INFO_PLIST = Info.plist

load(qt_tool)
