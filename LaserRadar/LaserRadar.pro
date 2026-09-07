QT       += core gui network opengl openglwidgets svgwidgets webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += opengl
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    GradientEditorWidget.cpp \
    PlaybackController.cpp \
    ReplayWindow.cpp \
    browserdialog.cpp \
    colorbarwidget.cpp \
    common.cpp \
    exporter.cpp \
    exportframedialog.cpp \
    main.cpp \
    laserradar.cpp \
    pointcloudfiltermanager.cpp \
    pointdatapagemodel.cpp \
    simplepointcloudview.cpp \
    udpreceiver.cpp

HEADERS += \
    GradientEditorWidget.h \
    PlaybackController.h \
    ReplayWindow.h \
    browserdialog.h \
    colorbarwidget.h \
    common.h \
    exporter.h \
    exportframedialog.h \
    laserradar.h \
    pointcloudfiltermanager.h \
    pointdatapagemodel.h \
    simplepointcloudview.h \
    udpreceiver.h

FORMS += \
    ReplayWindow.ui \
    exportframedialog.ui \
    laserradar.ui

# === libLAS 设置 ===
LIBLAS_ROOT = D:/libLas/vcpkg-master/installed/x64-windows
INCLUDEPATH += $$LIBLAS_ROOT/include
DEFINES += LIBLAS_DLL  # 告诉它用动态库模式

CONFIG(debug, debug|release) {
    LIBS += -L$$LIBLAS_ROOT/debug/lib -lliblas
} else {
    LIBS += -L$$LIBLAS_ROOT/lib -lliblas
}
LIBS += -lpcl_common -lpcl_io -lpcl_filters -lpcl_segmentation -lpcl_features

LIBS += -lUser32
LIBS += -lopengl32
LIBS += -lglu32

# PCL 路径 (你自己的路径)
PCL_ROOT = D:/PCL/vcpkg/installed/x64-windows

# 头文件路径
INCLUDEPATH += $$PCL_ROOT/include
#INCLUDEPATH += $$PCL_ROOT/include/pcl  # 版本号按实际调整

# 库文件路径
LIBS += -L$$PCL_ROOT/lib

# 链接需要的 PCL 库
LIBS += -lpcl_common
LIBS += -lpcl_io
LIBS += -lpcl_filters
LIBS += -lpcl_kdtree
LIBS += -lpcl_search
LIBS += -lpcl_sample_consensus
LIBS += -lpcl_segmentation
LIBS += -lpcl_surface
LIBS += -llz4
#LIBS += -lpcl_visualization
# 你还可以按需加更多 PCL 组件
# 自动 copy DLL 到 exe 目录
win32 {
    DLLDIR = $$PCL_ROOT/bin

    CONFIG(debug, debug|release) {
        OUTDIR = $$OUT_PWD/debug
    } else {
        OUTDIR = $$OUT_PWD/output

        # Release EXE输出目录
        DESTDIR = $$OUT_PWD/output
    }

    message("DLLDIR = $$DLLDIR")
    message("OUTDIR = $$OUTDIR")

    system(cmd /c copy /Y \"$$DLLDIR\\pcl_*.dll\" \"$$OUTDIR\")
    system(cmd /c copy /Y \"$$DLLDIR\\boost_*.dll\" \"$$OUTDIR\")
    system(cmd /c copy /Y \"$$DLLDIR\\flann*.dll\" \"$$OUTDIR\")
    system(cmd /c copy /Y \"$$DLLDIR\\lz4.dll\" \"$$OUTDIR\")
    system(cmd /c copy /Y \"$$DLLDIR\\qhull_r.dll\" \"$$OUTDIR\")
}
win32 {
    CONFIG(debug, debug|release) {
        OUTDIR = $$OUT_PWD/debug
        DEPLOY_LIBLAS_DLL = $$LIBLAS_ROOT/debug/bin
    } else {
        OUTDIR = $$OUT_PWD/output

        # Release EXE输出目录
        DESTDIR = $$OUT_PWD/output

        DEPLOY_LIBLAS_DLL = $$LIBLAS_ROOT/bin
    }

    system(cmd /c copy /Y \"$$DEPLOY_LIBLAS_DLL\\*.dll\" \"$$OUTDIR\")
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc
QMAKE_CXXFLAGS += /bigobj
RC_FILE = logo.rc
