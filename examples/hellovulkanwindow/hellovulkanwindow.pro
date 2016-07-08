TEMPLATE = app
QT += vulkan
CONFIG += console

SOURCES = main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/hellovulkanwindow
INSTALLS += target

INCLUDEPATH += $$VULKAN_INCLUDE_PATH
