TEMPLATE = app
QT += vulkan

SOURCES = main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/hellovulkanwindow
INSTALLS += target

INCLUDEPATH += $$VULKAN_INCLUDE_PATH
