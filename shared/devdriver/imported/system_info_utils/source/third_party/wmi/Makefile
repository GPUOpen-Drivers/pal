##
 #######################################################################################################################
 #
 #  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

CXX=g++
CPPFLAGS=-c -O3 -std=c++11 -Wall -Wextra -Weffc++ -I../kernel_module/include -I./include -I./utils -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -Wdouble-promotion -Wuninitialized -fipa-pure-const -Wtrampolines -Wfloat-equal  -Wcast-qual -Wcast-align -Wconversion -Wlogical-op -Wredundant-decls -Wshadow -Wint-to-pointer-cast
CPP_SOURCES= \
	src/wmi.cpp \
	src/wmiresult.cpp

TEST_SOURCE= src/test.cpp

LDFLAGS=-lole32 -loleaut32 -lwbemuuid -L. -ldiaa_sami_comsupp

OBJECTS=$(CPP_SOURCES:src/%.cpp=bin/%.o)
TEST_OBJECT=$(TEST_SOURCE:src/%.cpp=bin/%.o)
DEPS=$(OBJECTS:bin/%.o=bin/%.d) $(TEST_OBJECT:bin/%.o=bin/%.d)

all: bin wmi.dll test.exe

-include $(DEPS)

wmi.dll: $(OBJECTS) diaa_sami_comsupp.dll
	@echo "Linking" $@
	@$(CXX) -shared -o $@ $(OBJECTS) $(LDFLAGS)

test.exe: wmi.dll $(TEST_OBJECT)
	@echo "Linking" $@
	@$(CXX) -o $@ $(TEST_OBJECT) -L. -lwmi

bin/%.o: src/%.cpp
	@echo "Compiling" $<
	@$(CXX) -MD $(CFLAGS) $(CPPFLAGS) -o $@ $<

bin:
	mkdir bin

diaa_sami_comsupp.dll:
	@make -C diaa_sami_comsupp
	@ln -s diaa_sami_comsupp/$@ .

clean:
	@echo "Cleaning"
	@make -C diaa_sami_comsupp clean
	@rm -rf bin/*.o bin/*.d wmi.dll diaa_sami_comsupp.dll test.exe
