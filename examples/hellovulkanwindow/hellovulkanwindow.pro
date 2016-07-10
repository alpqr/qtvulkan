TEMPLATE = app
QT += vulkan
CONFIG += console

SOURCES = main.cpp worker.cpp
HEADERS = worker.h

target.path = $$[QT_INSTALL_EXAMPLES]/hellovulkanwindow
INSTALLS += target

INCLUDEPATH += $$VULKAN_INCLUDE_PATH
