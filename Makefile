# Shameless copy paste from tj/luna project
# https://github.com/tj/luna/blob/master/Makefile

LIBROOT=$(BASE_LIBROOT)

SRC = $(wildcard base/*.cpp)
OFILES = $(SRC:.cpp=.o)
OBJ = $(subst base,obj,$(OFILES))

SRC_BACKEND = $(wildcard base/backend/*.cpp)
OFILES_BACKEND = $(SRC_BACKEND:.cpp=.o)
OBJ := $(OBJ) $(subst base/backend,obj,$(OFILES_BACKEND))

SRC_C = $(wildcard base/*.c)
OFILES_C = $(SRC_C:.c=.o)
OBJ_C = $(subst base,obj,$(OFILES_C))

OUT = renderer

LIBS := -lglfw #$(shell pkg-config --libs glfw3)
LIBS += -lGLEW -lGLU -lGL #$(shell pkg-config --libs glew)
LIBS += -lstdc++fs -lvulkan

##
# TODO: provide fallback options for libraries that are usually in /usr/include
# but have been installed locally instead (like glm)
##

CPPFLAGS=-I./base -I${LIBROOT}/json/include -DGLM_ENABLE_EXPERIMENTAL -DBASE_ENABLE_VULKAN
CXXFLAGS=-std=c++20 -Wall -Wpedantic -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-deprecated-enum-enum-conversion
CFLAGS=

ifdef DEBUG
	CPPFLAGS += -DBASE_DEBUG=1 -DBASE_ON_DIE_TRIGGER_SEGFAULT
	CXXFLAGS += -g -ggdb -O0 
	CFLAGS += -g -ggdb -O0
else
	CXXFLAGS += -O2
	CFLAGS += -O2
endif


CC = gcc
# Fallback to gcc if clang not available
CXX = g++
#clang = $(shell which clang 2> /dev/null)
#ifeq (, $(clang))
#	CXX = gcc
#endif

dir:
	mkdir -p linux
	mkdir -p obj

linux: dir $(OUT)

$(OUT): $(OBJ) $(OBJ_C)
	@printf "\e[33mLinking begin!\n"
	$(CXX) $^ $(LIBS) -o $@
	@printf "\e[33mLinking\e[90m %s\e[0m\n" $@
	@printf "\e[34mDone!\e[0m\n"

obj/%.o: base/%.cpp 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
	@printf "\e[36mCompile\e[90m %s\e[0m\n" $@

obj/%.o: base/backend/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
	@printf "\e[36mCompile\e[90m %s\e[0m\n" $@

obj/%.o: base/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@printf "\e[36mCompile\e[90m %s\e[0m\n" $@

clean:
	rm -rf linux obj
	@printf "\e[34mAll clear!\e[0m\n"
