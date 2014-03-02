
TARGET = echo
C_SRCS = ae.c anet.c zmalloc.c
CXX_SRCS = echo.cc dispatcher.cc

INCPATH = 
LIBPATH = 

CXX = clang++
CPPFLAGS += -std=c++11 -DUSE_TCMALLOC -DINIT_SETSIZE=1000
CXXFLAGS += -Wall -Werror -Wno-deprecated
LDFLAGS += -ltcmalloc -lprofiler

ifeq ($(DEBUG),no)
	CXXFLAGS += -O2 -DNDEBUG
else
	CXXFLAGS += -g -O0 -fno-inline
endif

all: $(TARGET)

clean:
	rm -f $(TARGET) *.o *~ *.d *.gch *.d.*

include $(C_SRCS:.c=.d)
include $(CXX_SRCS:.cc=.d)

$(TARGET): $(C_SRCS:.c=.o) $(CXX_SRCS:.cc=.o)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $^

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $^

ae.d: ae.c ae_select.c ae_epoll.c ae_kqueue.c ae_evport.c
	echo 'ae.o ae.d: ae.c ae.h config.h zmalloc.h' > $@

anet.d: anet.c
	echo 'anet.o anet.d: anet.c fmacros.h anet.h' > $@

zmalloc.d: zmalloc.c
	echo 'zmalloc.o zmalloc.d: zmalloc.c config.h zmalloc.h' > $@

%.d: %.cc
	set -e; rm -f $@; \
	$(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@: ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: all clean
