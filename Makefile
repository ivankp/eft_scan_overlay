ROOT_CFLAGS := -Wno-deprecated-declarations -pthread -m64
ROOT_CFLAGS += -I$(shell root-config --incdir)
ROOT_LIBS   := $(shell root-config --libs)

.PHONY: all clean

NAME := eft_scan_hists

all: $(NAME)

$(NAME): %: src/%.cc src/catstr.hh
	$(CXX) -std=c++14 -Wall -O3 -flto -Isrc \
	  $(ROOT_CFLAGS) \
	  $(filter %.cc,$^) -o $@ \
	  $(ROOT_LIBS) -lboost_system -lboost_filesystem

clean:
	@rm -fv $(NAME)

