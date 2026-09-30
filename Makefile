CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Isrc

# Automatic OS Detection
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    EXE := .exe
    LDFLAGS ?= -lws2_32
    RM := del /Q /F
else
    DETECTED_OS := $(shell uname -s 2>/dev/null || echo Linux)
    EXE :=
    LDFLAGS ?= -pthread
    RM := rm -f
endif

CORE_SRCS = src/packet.cpp src/sha256.cpp src/fault_injection.cpp src/udp_socket.cpp src/stats.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

SENDER_SRCS = src/main_sender.cpp src/sender_base.cpp src/sender_saw.cpp src/sender_gbn.cpp src/sender_sr.cpp
SENDER_OBJS = $(SENDER_SRCS:.cpp=.o)

RECEIVER_SRCS = src/main_receiver.cpp src/receiver.cpp
RECEIVER_OBJS = $(RECEIVER_SRCS:.cpp=.o)

TEST_SRCS = tests/test_packet.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

TCP_SENDER_SRCS = scripts/tcp_sender.cpp
TCP_SENDER_OBJS = $(TCP_SENDER_SRCS:.cpp=.o)

TCP_RECEIVER_SRCS = scripts/tcp_receiver.cpp
TCP_RECEIVER_OBJS = $(TCP_RECEIVER_SRCS:.cpp=.o)

TARGETS = sender$(EXE) receiver$(EXE) test_packet$(EXE) tcp_sender$(EXE) tcp_receiver$(EXE)

.PHONY: all clean test info

all: $(TARGETS)

info:
	@echo "Detected OS: $(DETECTED_OS)"
	@echo "Targets: $(TARGETS)"

sender$(EXE): $(SENDER_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

receiver$(EXE): $(RECEIVER_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_packet$(EXE): $(TEST_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

tcp_sender$(EXE): $(TCP_SENDER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

tcp_receiver$(EXE): $(TCP_RECEIVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: test_packet$(EXE)
	./test_packet$(EXE)

clean:
	$(RM) $(TARGETS) $(CORE_OBJS) $(SENDER_OBJS) $(RECEIVER_OBJS) $(TEST_OBJS) $(TCP_SENDER_OBJS) $(TCP_RECEIVER_OBJS)
