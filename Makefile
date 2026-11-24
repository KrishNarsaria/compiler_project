# ============================================================
# Makefile for the RISC-V compiler (Flex + Bison + C++)
#   make          build ./rvcc
#   make clean    remove generated files
# ============================================================

CXX       = g++
BISON     = bison
FLEX      = flex
SRC_DIR   = src
RT_DIR    = runtime
BUILD_DIR = build
TARGET    = rvcc
CXXFLAGS  = -std=c++14 -O2 -Wall -Wno-unused-function -I$(BUILD_DIR) -I$(SRC_DIR)

CORE_OBJS = $(BUILD_DIR)/parser.tab.o $(BUILD_DIR)/lex.yy.o \
            $(BUILD_DIR)/ast.o $(BUILD_DIR)/codegen.o $(BUILD_DIR)/backend.o

all: $(TARGET)

# ---- stage 0: compiler without runtime, used to translate the runtime library
$(BUILD_DIR)/rvcc0: $(CORE_OBJS) $(BUILD_DIR)/runtime_empty.o
	$(CXX) -o $@ $^

$(BUILD_DIR)/runtime_empty.cpp: | $(BUILD_DIR)
	echo 'extern const char *g_runtime_asm; const char *g_runtime_asm = "";' > $@

# ---- runtime library: C source -> assembly -> embedded string
$(BUILD_DIR)/runtime.s: $(RT_DIR)/runtime.c $(BUILD_DIR)/rvcc0
	$(BUILD_DIR)/rvcc0 --runtime $< $@

$(BUILD_DIR)/runtime_asm.cpp: $(BUILD_DIR)/runtime.s
	( echo 'extern const char *g_runtime_asm;'; \
	  echo 'const char *g_runtime_asm = R"RTASM('; \
	  cat $<; \
	  echo ')RTASM";' ) > $@

# ---- final compiler
$(TARGET): $(CORE_OBJS) $(BUILD_DIR)/runtime_asm.o
	$(CXX) -o $@ $^

$(BUILD_DIR)/parser.tab.cpp: $(SRC_DIR)/parser.y | $(BUILD_DIR)
	$(BISON) -d -o $@ $<

$(BUILD_DIR)/parser.tab.hpp: $(BUILD_DIR)/parser.tab.cpp
	@:

$(BUILD_DIR)/lex.yy.cpp: $(SRC_DIR)/lexer.l $(BUILD_DIR)/parser.tab.hpp | $(BUILD_DIR)
	$(FLEX) -o $@ $<

$(BUILD_DIR)/parser.tab.o: $(BUILD_DIR)/parser.tab.cpp $(SRC_DIR)/ast.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/lex.yy.o: $(BUILD_DIR)/lex.yy.cpp $(SRC_DIR)/ast.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/ast.o: $(SRC_DIR)/ast.cpp $(SRC_DIR)/ast.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/codegen.o: $(SRC_DIR)/codegen.cpp $(SRC_DIR)/ast.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/backend.o: $(SRC_DIR)/backend.cpp $(SRC_DIR)/backend.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/runtime_empty.o: $(BUILD_DIR)/runtime_empty.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/runtime_asm.o: $(BUILD_DIR)/runtime_asm.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) output

.PHONY: all clean
