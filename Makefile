CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = 

TARGET = flic_client
SOURCES = flic_client.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# You'll need to download client_protocol_packets.h from the fliclib-linux-hci repository
# https://github.com/50ButtonsEach/fliclib-linux-hci/blob/master/simpleclient/client_protocol_packets.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install:
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall