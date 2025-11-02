# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -pthread

# Directories
OBJ_DIR = build/obj
BIN_DIR = build/bin

all: $(OBJ_DIR) $(BIN_DIR) $(BIN_DIR)/worker $(BIN_DIR)/distributor
worker: $(BIN_DIR)/worker
distributor: $(BIN_DIR)/distributor

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Binaries
$(BIN_DIR)/worker: $(OBJ_DIR)/main_worker.o $(OBJ_DIR)/common.o $(OBJ_DIR)/connection.o $(OBJ_DIR)/worker.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/main_worker.o $(OBJ_DIR)/common.o $(OBJ_DIR)/connection.o $(OBJ_DIR)/worker.o -o $(BIN_DIR)/worker $(LDFLAGS)

$(BIN_DIR)/distributor: $(OBJ_DIR)/main_distributor.o $(OBJ_DIR)/common.o $(OBJ_DIR)/connection.o $(OBJ_DIR)/distributor.o | $(BIN_DIR)
	$(CXX) $(OBJ_DIR)/main_distributor.o $(OBJ_DIR)/common.o $(OBJ_DIR)/connection.o $(OBJ_DIR)/distributor.o -o $(BIN_DIR)/distributor $(LDFLAGS)

# Object files
$(OBJ_DIR)/main_worker.o: main_worker.cpp src/worker/worker.h src/common/common.h src/utils/connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c main_worker.cpp -o $(OBJ_DIR)/main_worker.o

$(OBJ_DIR)/main_distributor.o: main_distributor.cpp src/distributor/distributor.h src/common/common.h src/utils/connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c main_distributor.cpp -o $(OBJ_DIR)/main_distributor.o

$(OBJ_DIR)/common.o: src/common/common.cpp src/common/common.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c src/common/common.cpp -o $(OBJ_DIR)/common.o

$(OBJ_DIR)/connection.o: src/utils/connection.cpp src/utils/connection.h src/common/common.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c src/utils/connection.cpp -o $(OBJ_DIR)/connection.o

$(OBJ_DIR)/distributor.o: src/distributor/distributor.cpp src/common/common.h src/utils/connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c src/distributor/distributor.cpp -o $(OBJ_DIR)/distributor.o

$(OBJ_DIR)/worker.o: src/worker/worker.cpp src/common/common.h src/utils/connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c src/worker/worker.cpp -o $(OBJ_DIR)/worker.o

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean worker distributor