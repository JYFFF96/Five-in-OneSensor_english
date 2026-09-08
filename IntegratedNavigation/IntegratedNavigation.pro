QT       += core gui charts serialport webenginewidgets webchannel

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    arcwidget.cpp \
    common.cpp \
    compasswidget.cpp \
    custommessagebox.cpp \
    fastcommanddialog.cpp \
    imagedialog.cpp \
    main.cpp \
    mainwindow.cpp \
    mapbridge.cpp \
    polarplot.cpp \
    postioningview.cpp \
    qcustomplot.cpp \
    satellitecounter.cpp \
    satelliteinfo.cpp \
    simulatepolarplot.cpp \
    trajectorysimulator.cpp

HEADERS += \
    arcwidget.h \
    common.h \
    compasswidget.h \
    custommessagebox.h \
    fastcommanddialog.h \
    imagedialog.h \
    include/CoordGeodetic.h \
    include/CoordTopocentric.h \
    include/DateTime.h \
    include/DecayedException.h \
    include/Eci.h \
    include/Globals.h \
    include/Observer.h \
    include/OrbitalElements.h \
    include/SGP4.h \
    include/SatelliteException.h \
    include/SolarPosition.h \
    include/TimeSpan.h \
    include/Tle.h \
    include/TleException.h \
    include/Util.h \
    include/Vector.h \
    mainwindow.h \
    mapbridge.h \
    polarplot.h \
    postioningview.h \
    qcustomplot.h \
    satellitecounter.h \
    satelliteinfo.h \
    simulatepolarplot.h \
    trajectorysimulator.h

FORMS += \
    mainwindow.ui \
    satelliteinfo.ui


INCLUDEPATH += include

CONFIG(debug, release|debug) {
    LIBS += -L$$PWD/libs/debug -lsgp4
} else { # 即为release版本
    LIBS += -L$$PWD/libs/release -lsgp4
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

DISTFILES += \
    IntegratedNavigation.pro.user

# 指定构建目录
DESTDIR = $$OUT_PWD/output

# 定义需要复制的文件夹
RESOURCES_DIR = $$PWD/config

# 确定构建目标目录
win32 {
    DESTDIR_DEBUG = $$OUT_PWD/debug
    DESTDIR_RELEASE = $$OUT_PWD/release
} else {
    DESTDIR_DEBUG = $$OUT_PWD
    DESTDIR_RELEASE = $$OUT_PWD
}


# config文件夹复制到output
    QMAKE_POST_LINK += robocopy "$$PWD/config" "$$OUT_PWD/output/config" /E /NFL /NDL /NJH /NJS /NC /NS /NP > nul & exit 0

RC_FILE = logo.rc
