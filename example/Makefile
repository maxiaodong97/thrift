THRIFT := ../compiler/cpp/thrift

SRC :=  \
    gen-cpp/example_types.cpp \
    main.cpp

INCLUDES := -I./gen-cpp/ -I../lib/cpp/src/

OBJS := $(SRC:%.cpp=%.o)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@ -g

all: $(OBJS)
	$(CXX) -o example.exe $(OBJS) ../lib/cpp/.libs/libthrift.a

gen-cpp/example_types.cpp: example.thrift
	$(THRIFT) --gen cpp example.thrift

clean:
	echo $(OBJS)
	rm -fr *.o gen-cpp/* *.exe
