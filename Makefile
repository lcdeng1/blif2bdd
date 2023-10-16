# Compiler and compiler flags
CXX := g++
CXXFLAGS := -Wall -ggdb -std=c++11

# Source files
CXX_SRCS := $(wildcard ./*.cc)

# Object files
CXX_OBJS := $(addprefix ./,$(notdir $(CXX_SRCS:.cc=.o)))

# Output executable
TARGET := blif2bdd

# Library directories and libraries
REXDD_DIR := /PATH_TO_REDD_LIB/
LIB_DIRS := -L$(REXDD_DIR)/build-release/src
INCLUDE_DIRS := -I$(REXDD_DIR)/src
LIBS := -lRexDD

# Makefile targets
all: $(TARGET)

$(TARGET): $(CXX_OBJS)
	$(CXX) $(INCLUDE_DIRS) $(LIBS) $(LIB_DIRS) $(CXXFLAGS) $^ -o $@

%.o: %.cc
	$(CXX) $(INCLUDE_DIRS) $(LIBS) $(LIB_DIRS) -c $(CXXFLAGS) $< 
# $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
# 	@mkdir -p $(OBJ_DIR)
# 	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(LIBS) -c $< -o $@

clean:
	rm -rf $(TARGET) *.o

.PHONY: all clean
