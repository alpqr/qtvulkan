TEMPLATE = app
QT += widgets vulkan
CONFIG += console

SOURCES = hellovulkanwidget.cpp \
          ../hellovulkanwindow/worker.cpp

HEADERS = ../hellovulkanwindow/worker.h

RESOURCES = hellovulkanwidget.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/hellovulkanwidget
INSTALLS += target

INCLUDEPATH += $$VULKAN_INCLUDE_PATH
