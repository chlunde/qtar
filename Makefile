CXXFLAGS=-W -Wall -g -O2
LDFLAGS+=-larchive

OBJECTS=qtar.o linux_getdents.o firstblock.o
BIN=qtar

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJECTS)

$(BIN): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@
