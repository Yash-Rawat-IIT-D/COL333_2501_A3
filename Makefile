CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

CXXFLAGS_DBG = -g -O0
CXXFLAGS_REL = -O3

SRC_DIR = src
SRC_MAIN1 = $(SRC_DIR)/main1.cpp
SRC_MAIN2 = $(SRC_DIR)/main2.cpp

BIN_DIR = bin
RELEASE_DIR = release
DBG_DIR = debug

DBG_MAIN1 = $(BIN_DIR)/$(DBG_DIR)/main1
DBG_MAIN2 = $(BIN_DIR)/$(DBG_DIR)/main2
TARGET_MAIN1 = $(BIN_DIR)/$(RELEASE_DIR)/main1
TARGET_MAIN2 = $(BIN_DIR)/$(RELEASE_DIR)/main2

all: main1

main1: $(SRC_MAIN1)
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/$(RELEASE_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET_MAIN1) $(SRC_MAIN1)

main2: $(SRC_MAIN2)
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/$(RELEASE_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET_MAIN2) $(SRC_MAIN2)

dbg_main1: $(SRC_MAIN1)
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/$(DBG_DIR)
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_DBG) -o $(DBG_MAIN1) $(SRC_MAIN1)

dbg_main2: $(SRC_MAIN2)
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/$(DBG_DIR)
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_DBG) -o $(DBG_MAIN2) $(SRC_MAIN2)

clean:
	rm -f $(TARGET_MAIN1) $(TARGET_MAIN2)
	rm -f $(DBG_MAIN1) $(DBG_MAIN2)
	rmdir --ignore-fail-on-non-empty $(BIN_DIR)/$(RELEASE_DIR)
	rmdir --ignore-fail-on-non-empty $(BIN_DIR)/$(DBG_DIR)