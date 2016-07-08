TEMPLATE = lib
QT += core-private gui-private quick
TARGET = QtVulkan

load(qt_module)

DEFINES += QTVULKAN_BUILD_DLL

SOURCES += $$PWD/qvulkanfunctions.cpp \
           $$PWD/qvulkanrenderloop.cpp

HEADERS += $$PWD/qtvulkanglobal.h \
           $$PWD/qvulkan.h \
           $$PWD/qvulkanfunctions.h \
           $$PWD/qvulkanrenderloop.h \
           $$PWD/qvulkanrenderloop_p.h

INCLUDEPATH += $$VULKAN_INCLUDE_PATH
