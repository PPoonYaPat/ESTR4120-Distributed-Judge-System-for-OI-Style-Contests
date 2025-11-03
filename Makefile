# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -pthread

# Directories
OBJ_DIR = build/obj
BIN_DIR = build/bin
COMMON_DIR = src/common
UTILS_DIR = src/utils
WORKER_DIR = src/worker
DIST_DIR = src/distributor

# Targets
all: $(OBJ_DIR) $(BIN_DIR) $(BIN_DIR)/worker $(BIN_DIR)/distributor
worker: $(BIN_DIR)/worker
distributor: $(BIN_DIR)/distributor

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ============================================================================
# Binaries
# ============================================================================

$(BIN_DIR)/worker: $(OBJ_DIR)/main_worker.o $(OBJ_DIR)/worker.o  $(OBJ_DIR)/base_connection.o $(OBJ_DIR)/connection.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/distributor: $(OBJ_DIR)/main_distributor.o $(OBJ_DIR)/distributor.o  $(OBJ_DIR)/base_connection.o $(OBJ_DIR)/connection.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

# ============================================================================
# Main files (entry points)
# ============================================================================

$(OBJ_DIR)/main_worker.o: main_worker.cpp \
                          $(WORKER_DIR)/worker.h \
                          $(COMMON_DIR)/common.h \
                          $(UTILS_DIR)/connection.h \
                          $(UTILS_DIR)/base_connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/main_distributor.o: main_distributor.cpp \
                               $(DIST_DIR)/distributor.h \
                               $(COMMON_DIR)/common.h \
                               $(UTILS_DIR)/connection.h \
                               $(UTILS_DIR)/base_connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Utils module (base_connection + connection)
# ============================================================================

$(OBJ_DIR)/base_connection.o: $(UTILS_DIR)/base_connection.cpp $(UTILS_DIR)/base_connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/connection.o: $(UTILS_DIR)/connection.cpp \
                         $(UTILS_DIR)/connection.h \
                         $(UTILS_DIR)/base_connection.h \
                         $(COMMON_DIR)/common.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Distributor module
# ============================================================================

$(OBJ_DIR)/distributor.o: $(DIST_DIR)/distributor.cpp \
                          $(DIST_DIR)/distributor.h \
                          $(COMMON_DIR)/common.h \
                          $(UTILS_DIR)/connection.h \
                          $(UTILS_DIR)/base_connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Worker module
# ============================================================================

$(OBJ_DIR)/worker.o: $(WORKER_DIR)/worker.cpp \
                     $(WORKER_DIR)/worker.h \
                     $(COMMON_DIR)/common.h \
                     $(UTILS_DIR)/connection.h \
                     $(UTILS_DIR)/base_connection.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# ============================================================================
# Phony targets
# ============================================================================

.PHONY: all clean worker distributor