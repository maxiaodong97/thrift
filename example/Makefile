THRIFT := ../thrift

SRC :=  \
    gen-cpp/example_types.cpp \
    main.cpp

OBJS := $(SRC:%.cpp=%.o)

%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

all: $(OBJS)
	$(CXX) $(LDFLAGS) -o example.exe $(OBJS) -lthrift

gen-cpp/example_types.cpp: example.thrift
	$(THRIFT) --gen cpp example.thrift

clean:
	echo $(OBJS)
	rm -fr *.o gen-cpp/*
