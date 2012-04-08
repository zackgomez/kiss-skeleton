#VPATH += ../shared
INCLUDEPATH += glm-0.9.2.7

HEADERS       = glwidget.h \
                window.h \
                arcball.h
SOURCES       = glwidget.cpp \
                qtmain.cpp \
                window.cpp \
                arcball.cpp
QT           += opengl
LIBS         += -lGLEW

# install
target.path = $$[QT_INSTALL_EXAMPLES]/opengl/hellogl
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS hellogl.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/opengl/hellogl
INSTALLS += target sources
