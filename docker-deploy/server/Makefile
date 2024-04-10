CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -g
LDFLAGS = -lboost_system -lboost_thread -lpthread -lpqxx -lpq -ltinyxml2
SOURCES = server.cpp account.cpp createrequest.cpp request.cpp transaction.cpp transactionrequest.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
