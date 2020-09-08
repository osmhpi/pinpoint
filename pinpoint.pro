QT -= gui
CONFIG += c++11
CONFIG -= app_bundle

SOURCES += \
	pinpoint.cpp \
	tegra_device_info.c \
	mcp_com.c

HEADERS += \
    PowerDataSource.h \
    tegra_device_info.h \
	TegraDeviceInfo.h \
	mcp_com.h \
	MCP_EasyPower.h
