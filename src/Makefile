
# CONFIG REQUIRED: SET THIS VARIABLE TO THE PATH OF THE INDRI SOURCE CODE
ATIRESRC=../external/atire
# CONFIG REQUIRED: SET THIS TO THE SDSL INSTALL PATH
SDSLPREFIX=../build/external/sdsl-lite/
DIVSUF=../build/external/sdsl-lite/external/libdivsufsort-2.0.1/
NEWTSRC=../include
FASTPFOR=../external/fastpfor/headers/

include $(ATIRESRC)/GNUmakefile
SHARED=
INCPATH=-I$(ATIRESRC)/source/ -I$(ATIRESRC)/atire/ -I $(SDSLPREFIX)/include/ -I$(NEWTSRC) -I$(FASTPFOR) -I$(DIVSUF)/include -I..
LIBPATH=-L $(SDSLPREFIX)/lib/ -L$(DIVSUF)/lib
LIBS= $(patsubst %, -l%, $(DEPENDENCIES)) -lsdsl -ldivsufsort ../build/libfastpfor_lib.a

all:
	$(CXX) $(CXXFLAGS) -O3 -msse4.2 -std=c++11 mk_wand_idx.cpp -o mk_wand_idx $(INCPATH) $(LIBPATH) $(LIBS) $(CPPLDFLAGS) $(SOURCES_OBJECTS) $(EXTRA_OBJS) $(LDFLAGS) -DFILENAME_INDEX -DIMPACT_HEADER
	$(CXX) $(CXXFLAGS) -O3 -msse4.2 -std=c++11 kstem_query.cpp -o kstem_query $(INCPATH) $(LIBPATH) $(LIBS) $(CPPLDFLAGS) $(SOURCES_OBJECTS) $(EXTRA_OBJS) $(LDFLAGS) -DFILENAME_INDEX -DIMPACT_HEADER

clean:
	rm -f mk_wand_idx kstem_query *~
