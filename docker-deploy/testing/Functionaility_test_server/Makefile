CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11
LDFLAGS = -lboost_system -lboost_thread -lpthread -lpqxx -lpq -ltinyxml2

SOURCES = main.cpp client.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = client

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
