
CFLAGS = -g -O0 -Wall -Wmissing-prototypes -pthread `pkg-config sdl2 glew gl --cflags`
#CFLAGS = -O3 -Wall -Wmissing-prototypes -pthread `pkg-config sdl2 glew gl --cflags`

LDFLAGS = -pthread `pkg-config sdl2 glew gl --libs`

OBJECTS = list.o

all: main test

main: $(OBJECTS)

test: $(OBJECTS)

clean:
	$(RM) *.o main test
