TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += GLEW_STATIC

win32: LIBS += -lglfw3dll -lglew32s -lopengl32

include("game_engine1/ge1.pri")

SOURCES += \
    main.cpp

DISTFILES += \
    shader/position_vertex.glsl

