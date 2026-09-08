QT       += core gui charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

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
    mycustomplot.cpp \
    qcustomplot.cpp \
    ultrasonicradar.cpp

HEADERS += \
    ControlCAN.h \
    can.h \
    canmodel.h \
    canthread.h \
    common.h \
    include/ECanVci.h \
    include/HexInputFilter.h \
    include/xlsxabstractooxmlfile.h \
    include/xlsxabstractooxmlfile_p.h \
    include/xlsxabstractsheet.h \
    include/xlsxabstractsheet_p.h \
    include/xlsxcell.h \
    include/xlsxcell_p.h \
    include/xlsxcellformula.h \
    include/xlsxcellformula_p.h \
    include/xlsxcelllocation.h \
    include/xlsxcellrange.h \
    include/xlsxcellreference.h \
    include/xlsxchart.h \
    include/xlsxchart_p.h \
    include/xlsxchartsheet.h \
    include/xlsxchartsheet_p.h \
    include/xlsxcolor_p.h \
    include/xlsxconditionalformatting.h \
    include/xlsxconditionalformatting_p.h \
    include/xlsxcontenttypes_p.h \
    include/xlsxdatavalidation.h \
    include/xlsxdatavalidation_p.h \
    include/xlsxdatetype.h \
    include/xlsxdocpropsapp_p.h \
    include/xlsxdocpropscore_p.h \
    include/xlsxdocument.h \
    include/xlsxdocument_p.h \
    include/xlsxdrawing_p.h \
    include/xlsxdrawinganchor_p.h \
    include/xlsxformat.h \
    include/xlsxformat_p.h \
    include/xlsxglobal.h \
    include/xlsxmediafile_p.h \
    include/xlsxnumformatparser_p.h \
    include/xlsxrelationships_p.h \
    include/xlsxrichstring.h \
    include/xlsxrichstring_p.h \
    include/xlsxsharedstrings_p.h \
    include/xlsxsimpleooxmlfile_p.h \
    include/xlsxstyles_p.h \
    include/xlsxtheme_p.h \
    include/xlsxutility_p.h \
    include/xlsxworkbook.h \
    include/xlsxworkbook_p.h \
    include/xlsxworksheet.h \
    include/xlsxworksheet_p.h \
    include/xlsxzipreader_p.h \
    include/xlsxzipwriter_p.h \
    mycustomplot.h \
    qcustomplot.h \
    ultrasonicradar.h

FORMS += \
    ultrasonicradar.ui
INCLUDEPATH += "D:\Qt\Qt6\6.7.3\msvc2022_64\include"
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    UltrasonicRadar.pro.user

RESOURCES += \
    resource.qrc

# # 添加库目录
# LIBS += -L$$PWD/libs -lECanVci64

# 添加头文件目录
INCLUDEPATH += $$PWD/include


# 假设你的 DLL 文件在项目的 thirdparty 目录下
DLL_PATH = $$PWD/libs/ECanVci64.dll
DLL_PATH = $$PWD/libs/ControlCAN.dll

# 指定构建目录
DESTDIR = $$OUT_PWD/output

# 使用 QMAKE_POST_LINK 复制 DLL 文件
#QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$DLL_PATH) $$shell_path($$DESTDIR)

LIBS += -L$$PWD/libs/ -lControlCAN
CONFIG(debug, release|debug) {
    LIBS += -L$$PWD/libs/debug -lQXlsx
} else { # 即为release版本
    LIBS += -L$$PWD/libs/release -lQXlsx
}

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
