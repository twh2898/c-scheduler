
#CFLAGS = -O3 -Wall -Wmissing-prototypes -pthread
# Debug flags
CFLAGS = -g -O0 -Wall -Wextra -Wmissing-prototypes -pthread
CXXFLAGS = -std=c++17 -Itests/gtest/googletests/include/ -Ltests/gtest/lib/ -lgtest -lpthread

LDFLAGS = -lpthread

OBJECTS = list.o scheduler.o
TESTOBJECTS = $(addprefix tests/,  test.o TestScheduler.o)

TARGET = main
TESTTARGET = testmain

all: $(TARGET) $(TESTTARGET)

$(TARGET): $(OBJECTS)

$(TESTTARGET): $(TESTOBJECTS) $(OBJECTS)
	$(CXX) -o testmain $(TESTOBJECTS) list.o $(CXXFLAGS) 

list.o: list.c list.h
scheduler.o: scheduler.c scheduler.h

clean:
	$(RM) *.o tests/*.o $(TARGET) $(TESTTARGET)
