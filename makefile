
#CFLAGS = -O3 -Wall -Wmissing-prototypes -pthread
# Debug flags
CFLAGS = -g -O0 -Wall -Wmissing-prototypes -pthread `pkg-config gtest --cflags`
CXXFLAGS = -std=c++17 -Itests/gtest/googletests/include

LDFLAGS = -lpthread 
TESTFLAGS = `pkg-config gtest --libs`

OBJECTS = list.o scheduler.o
TESTOBJECTS = $(wildcard tests/*.cpp)

TARGET = main
TESTTARGET = testmain

all: $(TARGET) $(TESTTARGET)

$(TARGET): $(OBJECTS)

$(TESTTARGET): $(TESTOBJECTS) list.o
	$(CXX) $(LDFLAGS) $(TESTFLAGS) $(TESTOBJECTS) list.o -o $(TESTTARGET)

clean:
	$(RM) *.o $(TARGET) $(TESTTARGET)
