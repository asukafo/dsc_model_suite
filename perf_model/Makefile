# DSC Self-Contained Verification Testbench
# Links C reference model objects into SystemC binary for inline comparison.

SYSTEMC_HOME = $(HOME)/opt/systemc-2.3.4
SYSTEMC_INC  = $(SYSTEMC_HOME)/include
SYSTEMC_LIB  = $(SYSTEMC_HOME)/lib

SRCDIR  = ../vesa_cmod/source
TBDIR   = ../tb

CC      = gcc
CXX     = g++
CFLAGS  = -std=gnu99 -O2 -Wall -D_FILE_OFFSET_BITS=64 -DDSC_TRACE
CXXFLAGS = -std=c++17 -O2 -Wall
INCPATH  = -I$(SYSTEMC_INC) -I./common -I./modules -I$(SRCDIR) -I$(TBDIR)
LDFLAGS  = -L$(SYSTEMC_LIB) -lsystemc -lm

# ---- C model objects (compiled as C) ----
C_OBJS  = dsc_codec.o dsc_utils.o utl.o fifo.o multiplex.o logging.o

# ---- TB C objects ----
TB_OBJS = test_pattern.o dsc_config.o perf_log.o c_tracer.o c_model_api.o

# ---- SystemC C++ objects ----
SC_OBJS = fetch.o predict.o encode.o mux.o rate_ctl.o line_buffer.o slice_pipeline.o

# ---- Top-level ----
TARGET = dsc_verify

.PHONY: all clean

all: $(TARGET)

$(TARGET): sc_main.o $(C_OBJS) $(TB_OBJS) $(SC_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# ---- Top-level (C++) ----
sc_main.o: sc_main.cpp common/perf_counters.h common/dsc_config.h common/dsc_state.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

# ---- SystemC modules (C++) ----
fetch.o: modules/fetch.cpp modules/fetch.h modules/line_buffer.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

predict.o: modules/predict.cpp modules/predict.h modules/line_buffer.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

encode.o: modules/encode.cpp modules/encode.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

mux.o: modules/mux.cpp modules/mux.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

rate_ctl.o: modules/rate_ctl.cpp modules/rate_ctl.h common/perf_counters.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

line_buffer.o: modules/line_buffer.cpp modules/line_buffer.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

slice_pipeline.o: modules/slice_pipeline.cpp modules/slice_pipeline.h
	$(CXX) $(CXXFLAGS) $(INCPATH) -c $< -o $@

# ---- C model (compiled as C) ----
dsc_codec.o: $(SRCDIR)/dsc_codec.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

dsc_utils.o: $(SRCDIR)/dsc_utils.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

utl.o: $(SRCDIR)/utl.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

fifo.o: $(SRCDIR)/fifo.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

multiplex.o: $(SRCDIR)/multiplex.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

logging.o: $(SRCDIR)/logging.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

# ---- TB C objects ----
test_pattern.o: $(TBDIR)/test_pattern.c $(TBDIR)/test_pattern.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

dsc_config.o: $(TBDIR)/dsc_config.c $(TBDIR)/dsc_config.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

perf_log.o: $(TBDIR)/perf_log.c $(TBDIR)/dsc_tb.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

c_tracer.o: $(TBDIR)/c_tracer.c $(TBDIR)/c_tracer.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

c_model_api.o: $(TBDIR)/c_model_api.c $(TBDIR)/c_model_api.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

# ---- Cleanup ----
clean:
	rm -f *.o $(TARGET) traces/*.csv
