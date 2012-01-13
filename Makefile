CXXFLAGS=-g -O0 -Wall -Iglm-0.9.2.7
LDFLAGS=-lGL -lGLEW -lGLU -lglut

all: kiss-skeleton

kiss-skeleton: main.o ArcBall.o uistate.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf *.o kiss-skeleton
