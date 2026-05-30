# DSC Complete Testbench Makefile
SRCDIR = ../vesa_cmod/source

CC      = gcc
CFLAGS  = -std=gnu99 -O3 -Wall -D_FILE_OFFSET_BITS=64
INCPATH = -I$(SRCDIR) -I.
LDFLAGS = -lm

# DSC library objects (no codec_main.c dependency)
DSC_OBJS = \
	dsc_codec.o \
	dsc_utils.o \
	utl.o \
	fifo.o \
	multiplex.o \
	logging.o

# Testbench objects
TB_OBJS = \
	dsc_tb_main.o \
	dsc_config.o \
	test_pattern.o \
	perf_log.o

TARGET = dsc_tb

# Traced build: dsc_codec compiled with -DDSC_TRACE
TARGET_TRACE = dsc_tb_trace
DSC_TRACE_OBJS = dsc_codec_trace.o dsc_utils.o utl.o fifo.o multiplex.o logging.o
TB_TRACE_OBJS  = dsc_tb_main_trace.o dsc_config.o test_pattern.o perf_log.o c_tracer.o

.PHONY: all clean run test trace

all: $(TARGET) $(TARGET_TRACE)

$(TARGET_TRACE): $(TB_TRACE_OBJS) $(DSC_TRACE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

dsc_tb_main_trace.o: dsc_tb_main.c dsc_tb.h dsc_config.h test_pattern.h
	$(CC) $(CFLAGS) -DDSC_TRACE $(INCPATH) -c $< -o $@

dsc_codec_trace.o: $(SRCDIR)/dsc_codec.c
	$(CC) $(CFLAGS) -DDSC_TRACE $(INCPATH) -c $< -o $@

c_tracer.o: c_tracer.c c_tracer.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

trace: $(TARGET_TRACE)
	./$(TARGET_TRACE) -p zones -o results/trace_zones.csv
	./$(TARGET_TRACE) -p ramp  -o results/trace_ramp.csv
	./$(TARGET_TRACE) -p bars  -o results/trace_bars.csv

$(TARGET): $(TB_OBJS) $(DSC_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- TB sources ---
dsc_tb_main.o: dsc_tb_main.c dsc_tb.h dsc_config.h test_pattern.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

dsc_config.o: dsc_config.c dsc_config.h dsc_tb.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

test_pattern.o: test_pattern.c test_pattern.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

perf_log.o: perf_log.c dsc_tb.h
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

# --- DSC library objects ---
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

# --- Quick test ---
test: $(TARGET)
	./$(TARGET) -p bars -o /tmp/dsc_bars.csv
	./$(TARGET) -p ramp -o /tmp/dsc_ramp.csv
	./$(TARGET) -p checker -o /tmp/dsc_checker.csv
	./$(TARGET) -p random -o /tmp/dsc_random.csv
	./$(TARGET) -p zones -o /tmp/dsc_zones.csv

clean:
	rm -f *.o $(TARGET)
