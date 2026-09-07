QT       += core gui printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    can.cpp \
    canmodel.cpp \
    canthread.cpp \
    common.cpp \
    main.cpp \
    mmwaveradar.cpp \
    mycustomplot.cpp \
    qcustomplot.cpp

HEADERS += \
    ControlCAN.h \
    can.h \
    canmodel.h \
    canthread.h \
    common.h \
    include/ECanVci.h \
    include/HexInputFilter.h \
    mmwaveradar.h \
    mycustomplot.h \
    qcustomplot.h

FORMS += \
    mmwaveradar.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

# 添加头文件目录
INCLUDEPATH += $$PWD/include


# 假设你的 DLL 文件在项目的 thirdparty 目录下
DLL_PATH = $$PWD/libs/ECanVci64.dll
DLL_PATH = $$PWD/libs/ControlCAN.dll

# 指定构建目录
DESTDIR = $$OUT_PWD/output

# 使用 QMAKE_POST_LINK 复制 DLL 文件
#QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$DLL_PATH) $$shell_path($$DESTDIR)

CONFIG(debug, release|debug) {
    LIBS += -L$$PWD/libs/debug -lQXlsx
} else { # 即为release版本
    LIBS += -L$$PWD/libs/release -lQXlsx
}

LIBS += -L$$PWD/libs/ -lControlCAN
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

# 在 Debug 和 Release 构建后复制文件夹
CONFIG(debug, debug|release) {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$DLL_PATH) $$shell_path($$DESTDIR) $$escape_expand(\\n)
    QMAKE_POST_LINK += robocopy "$$PWD/config" "$$OUT_PWD/output/config" /E /NFL /NDL /NJH /NJS /NC /NS /NP > nul & exit 0
}
CONFIG(release, debug|release) {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$DLL_PATH) $$shell_path($$DESTDIR) $$escape_expand(\\n)
    QMAKE_POST_LINK += robocopy "$$PWD/config" "$$OUT_PWD/output/config" /E /NFL /NDL /NJH /NJS /NC /NS /NP > nul & exit 0
}
RC_FILE = logo.rc
