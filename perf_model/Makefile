# DSC SystemC Performance Model — Fully Self-Contained
# Algorithm code is embedded directly in SC_MODULE .cpp files via common/dsc_algo.h.
# Only external dependency: SystemC 2.3.4

SYSTEMC_HOME = $(HOME)/opt/systemc-2.3.4
SYSTEMC_INC  = $(SYSTEMC_HOME)/include
SYSTEMC_LIB  = $(SYSTEMC_HOME)/lib

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall
INCPATH  = -I$(SYSTEMC_INC) -I./common -I./modules
LDFLAGS  = -L$(SYSTEMC_LIB) -lsystemc -lm

# ---- SystemC pipeline modules ----
SC_SRCS  = modules/fetch.cpp modules/predict.cpp modules/encode.cpp
SC_SRCS += modules/mux.cpp modules/rate_ctl.cpp modules/line_buffer.cpp
SC_SRCS += modules/slice_pipeline.cpp

SC_OBJS  = $(SC_SRCS:.cpp=.o)

# ---- Standalone pipeline (no C model dependency) ----
TARGET = dsc_perf

.PHONY: all clean verify

all: $(TARGET)

$(TARGET): sc_main.o $(SC_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

sc_main.o: sc_main.cpp common/perf_counters.h common/dsc_config.h common/dsc_state.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

modules/%.o: modules/%.cpp
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

# ---- Optional: verification binary (links C model from tb/) ----
# Requires tb/ built first: cd ../tb && make
verify: sc_main_verify.o $(SC_OBJS) ../tb/dsc_codec.o ../tb/dsc_utils.o ../tb/utl.o ../tb/fifo.o ../tb/multiplex.o ../tb/logging.o ../tb/test_pattern.o ../tb/tb_config.o ../tb/c_tracer.o ../tb/perf_log.o
	$(CXX) -o dsc_verify $^ $(LDFLAGS)

sc_main_verify.o: sc_main.cpp
	$(CXX) $(CXXFLAGS) -DDSC_VERIFY $(INCPATH) -I../tb -c $< -o $@

../tb/%.o: ../tb/%.c
	gcc -std=gnu99 -O2 -Wall -D_FILE_OFFSET_BITS=64 -DDSC_TRACE $(INCPATH) -I../tb -c $< -o $@

# Special: dsc_codec.o needs main() renamed
../tb/dsc_codec.o: ../tb/../vesa_cmod/source/dsc_codec.c
	gcc -std=gnu99 -O2 -Wall -D_FILE_OFFSET_BITS=64 -DDSC_TRACE -Dmain=dsc_disabled $(INCPATH) -I../tb -c $< -o $@

clean:
	rm -f *.o modules/*.o $(TARGET) dsc_verify traces/*.csv
