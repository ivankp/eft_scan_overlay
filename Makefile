STD := -std=c++14
DF := $(STD) -Isrc
CF := $(STD) -Wall -Isrc
LF := $(STD)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), rel)))
# development mode
CF += -O2 -g -fmax-errors=3
else
# release mode
CF += -O3 -flto -funroll-loops -march=native
LF += -flto
endif

# ROOT_CFLAGS := $(shell root-config --cflags)
ROOT_CFLAGS := -Wno-deprecated-declarations -pthread -m64
ROOT_CFLAGS += -I$(shell root-config --incdir)
ROOT_LIBS   := $(shell root-config --libs)

C_plots := $(ROOT_CFLAGS)
L_plots := $(ROOT_LIBS) -lboost_system -lboost_filesystem

SRC := src
BIN := bin
BLD := .build

SRCS := $(shell find $(SRC) -type f -name '*.cc')
DEPS := $(patsubst $(SRC)%.cc,$(BLD)%.d,$(SRCS))

GREP_EXES := grep -rl '^ *int \+main *(' $(SRC)
EXES := $(patsubst $(SRC)%.cc,$(BIN)%,$(filter %.cc,$(shell $(GREP_EXES))))

HISTS := $(filter $(BIN)/hist_%,$(EXES))

NODEPS := clean
.PHONY: all rel clean

all: $(EXES)
rel: $(EXES)

#Don't create dependencies when we're cleaning, for instance
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
-include $(DEPS)
endif

$(DEPS): $(BLD)/%.d: $(SRC)/%.cc | $(BLD)
	$(CXX) $(DF) -MM -MT '$(@:.d=.o)' $< -MF $@

$(BLD)/%.o: | $(BLD)
	$(CXX) $(CF) $(C_$*) -DPREFIX="$(PREFIX)" -c $(filter %.cc,$^) -o $@

$(EXES): $(BIN)/%: $(BLD)/%.o | $(BIN)
	$(CXX) $(LF) $(filter %.o,$^) -o $@ $(L_$*)

$(BLD) $(BIN):
	mkdir $@

clean:
	@rm -rfv $(BLD) $(BIN)
