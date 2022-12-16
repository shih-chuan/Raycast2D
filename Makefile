TARGET := _raycast2d$(shell python3-config --extension-suffix)
INCLUDES := $(shell python3 -m pybind11 --includes) $(shell python3-config --includes)
LIBRARYS := -mavx -Wpsabi
CXXFLAGS := -O3 -Wall -shared -std=c++11 -fPIC $(LIBRARYS) $(INCLUDES)

$(TARGET): _raycast2d.cpp
	g++ $^ -o $(TARGET) $(CXXFLAGS) 

.PHONY: test
test: $(TARGET)
	python3 -m pytest

.PHONY: clean
clean:
	rm -rf *.so __pycache__ .pytest_cache