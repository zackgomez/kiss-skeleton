CXXFLAGS=-g -O0 -Wall -Iglm-0.9.2.7
LDFLAGS=-lGL -lGLEW -lGLU -lglut

all: kiss-skeleton

kiss-skeleton: kiss-skeleton.o main.o arcball.o
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

run: kiss-skeleton
	./kiss-skeleton

.PHONY: clean
clean:
	rm -rf *.o kiss-skeleton
