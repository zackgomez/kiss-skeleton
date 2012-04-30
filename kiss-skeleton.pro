INCLUDEPATH  += glm-0.9.2.7
CONFIG       += debug
QMAKE_CXXFLAGS += -std=c++0x

HEADERS       = glwidget.h \
                arcball.h \
                skeleton.h \
                zgfx.h \
                mainwindow.h \
                libgsm.h \
                meshops.h \
                glsubdisplay.h \
                timeline.h
SOURCES       = glwidget.cpp \
                qtmain.cpp \
                arcball.cpp \
                skeleton.cpp \
                zgfx.cpp \
                mainwindow.cpp \
                libgsm.cpp \
                meshops.cpp \
                glsubdisplay.cpp \
                timeline.cpp
QT           += opengl
LIBS         += -lGLEW -lzip

# install
target.path = $$[QT_INSTALL_EXAMPLES]/opengl/hellogl
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS hellogl.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/opengl/hellogl
INSTALLS += target sources
