CC = gcc
CFLAGS = -Wall -Wextra -I./src/stage0
INC = -I provided
STAGE0_DIR = compiled/stage0
STAGEA_DIR = compiled/stageA
STAGEAPLUS_DIR = compiled/stageA_plus
STAGEB_DIR = compiled/stageB
STAGEC_DIR = compiled/stageC
STAGECRIVAL_DIR = compiled/stageC_rival

#rival
RIVAL_SRC = src/stageCrival/rival.c src/stageCrival/v0malloc.c src/stageCrival/vfastestmalloc.c src/stageCrival/trades.c src/stageA_plus/parser.c provided/trade_io.c
V0_SRC = src/stageCrival/v0malloc.c src/stageCrival/rival.c src/stageCrival/trades.c src/stageA_plus/parser.c provided/trade_io.c
FAST_SRC = src/stageCrival/vfastestmalloc.c src/stageCrival/rival.c src/stageCrival/trades.c src/stageA_plus/parser.c
RACE_SPEED = 10000
RACE_LED = #for the rpi
.PHONY: rival rival-test rival-stress run-rival run-rival-test run-rival-stress plot-rival demov0 demofast run-demov0 run-demofast

SUDO =
# OS specfic checks to create the relevant folders
ifeq ($(OS),Windows_NT)
    SUDO =
    fixpath = $(subst /,\,$(1))
    MKDIR = if not exist $(call fixpath,$(1)) mkdir $(call fixpath,$(1))
else
    SUDO = sudo
    fixpath = $(1)
    MKDIR = mkdir -p $(1)
endif

# ============================
# Stage 0 build commands
# ============================
# Builds all stage 0 files
stage0: loop loop-o1 loop-o2 loop-o3 stage0_benchmark stage0_benchmark-o2
stage0_benchmark: src/stage0/stage0_helper.c src/stage0/sum_recursive.c
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} src/stage0/stage0_helper.c src/stage0/sum_recursive.c -o $(STAGE0_DIR)/sum_recursive

# Optimisation tiers for Stage 0 benchmarking
stage0_benchmark-o2: 
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} -O2 src/stage0/stage0_helper.c src/stage0/sum_recursive.c -o $(STAGE0_DIR)/sum_recursive_O2

loop: src/stage0/stage0_helper.c src/stage0/sum_loop.c
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} src/stage0/stage0_helper.c src/stage0/sum_loop.c -o $(STAGE0_DIR)/sum_loop

# Optimisation tier for iterative Loop
loop-o1: 
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} -O1 src/stage0/stage0_helper.c src/stage0/sum_loop.c -o $(STAGE0_DIR)/sum_loop_O1

loop-o2:
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} -O2 src/stage0/stage0_helper.c src/stage0/sum_loop.c -o $(STAGE0_DIR)/sum_loop_O2

loop-o3:
	$(call MKDIR,$(STAGE0_DIR))
	${CC} ${CFLAGS} -O3 src/stage0/stage0_helper.c src/stage0/sum_loop.c -o $(STAGE0_DIR)/sum_loop_O3

# ===================================
# Stage A build and execute commands
# ===================================
stageA:
	$(call MKDIR,$(STAGEA_DIR))
	$(CC) -O2 $(INC) src/stageA/stageA.c provided/trade_io.c -o $(STAGEA_DIR)/stageA

stageA-test:
	$(call MKDIR,$(STAGEA_DIR))
	$(CC) -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageA/stageA.c provided/trade_io.c -o $(STAGEA_DIR)/stageA_test

run-stageA: stageA
	$(call MKDIR,$(STAGEA_DIR))
	$(call fixpath,./$(STAGEA_DIR)/stageA) > $(call fixpath,$(STAGEA_DIR)/stageA_dev_results.txt)

run-stageA-test: stageA-test
	$(call MKDIR,$(STAGEA_DIR))
	$(call fixpath,./$(STAGEA_DIR)/stageA_test) > $(call fixpath,$(STAGEA_DIR)/stageA_test_results.txt)

# =====================================
# Stage A+ build and execute commands
# =====================================
stageAplus: 
	$(call MKDIR,$(STAGEAPLUS_DIR))
	$(CC) -O2 $(INC) src/stage0/stage0_helper.c src/stageA_plus/stageAplus.c src/stageA_plus/parser.c -o $(STAGEAPLUS_DIR)/stageAplus

run-stageAplus: stageAplus
	./$(STAGEAPLUS_DIR)/stageAplus

# =====================================
# Stage B build and execute commands
# =====================================
stageB: 
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB

stageB-test:
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB_test

stageB-stress:
	$(CC) -O2 -Dstress_file $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB_stress

run-stageB:
	./$(STAGEB_DIR)/stageB

ladder-test:
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c -o $(STAGEB_DIR)/ladder_test

ladder-stress:
	$(CC) -O2 -Dstress_file $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c -o $(STAGEB_DIR)/ladder_stress

ladder-dev:
	$(CC) -O2 $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c -o $(STAGEB_DIR)/ladder_dev

# each run- target rebuilds first, so it can never run an old compiled copy
run-ladder-test: ladder-test
	./$(STAGEB_DIR)/ladder_test

run-ladder-stress: ladder-stress
	./$(STAGEB_DIR)/ladder_stress

run-ladder-dev: ladder-dev
	./$(STAGEB_DIR)/ladder_dev

# the full ladder: check on test20 first, then the stress_full.csv run
run-ladder: ladder-test ladder-stress
	@echo "--- test20 check: expect BUY at 7 and SELL at 10 for every version ---"
	./$(STAGEB_DIR)/ladder_test

	@echo "--- stress_full.csv ladder (takes a few minutes) ---"
	./$(STAGEB_DIR)/ladder_stress

plot-ladder:
	gnuplot plots/plot_ladder
	@echo "Plotted ladder to ladder.png"

run-ladder-full: 
	run-ladder plot-ladder

# =====================================
# Stage C build and execute commands
# =====================================
stageC: 
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 $(INC) src/stageC/stageC.c src/stageA_plus/parser.c -o $(STAGEC_DIR)/stageC

stageC-test:
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageC/stageC.c src/stageA_plus/parser.c -o $(STAGEC_DIR)/stageC_test

stageC-stress:
	$(CC) -O2 -Dstress_file $(INC) src/stageC/stageC.c src/stageA_plus/parser.c -o $(STAGEC_DIR)/stageC_stress

run-stageC:
	./$(STAGEC_DIR)/stageC

run-stageC-test: stageC-test
	./$(STAGEC_DIR)/stageC_test

run-stageC-stress: stageC-stress
	./$(STAGEC_DIR)/stageC_stress

# ===============================================
# Stage C Rival Bot build and execute commands
# ===============================================
rival-test:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) $(RIVAL_SRC) -o $(STAGECRIVAL_DIR)/rival_test

rival-stress:
	$(CC) -O2 -Dstress_file $(INC) $(RIVAL_SRC) -o $(STAGECRIVAL_DIR)/rival_stress

rival: rival-test rival-stress

run-rival-test: rival-test
	./$(STAGECRIVAL_DIR)/rival_test

run-rival-stress: rival-stress
	./$(STAGECRIVAL_DIR)/rival_stress

run-rival: rival-test rival-stress
	@echo "--- test20 check: both bots must show BUY at 7 and SELL at 10 ---"
	./$(STAGECRIVAL_DIR)/rival_test
	@echo "--- stress_full.csv rival sweep ---"
	./$(STAGECRIVAL_DIR)/rival_stress
	@echo "--- plotting rival sweep ---"
	gnuplot plots/plot_rival

plot-rival:
	gnuplot plots/plot_rival

#demo
demov0:
	$(CC) -O2 -Dstress_file -Ddemo_v0 -DSPEED=$(RACE_SPEED) $(INC) $(V0_SRC) -o $(STAGECRIVAL_DIR)/demov0

demofast:
	$(CC) -O2 -Dstress_file -Ddemo_fast -DSPEED=$(RACE_SPEED) $(RACE_LED) $(INC) $(FAST_SRC) -o $(STAGECRIVAL_DIR)/demofast

run-demov0: demov0
	./$(STAGECRIVAL_DIR)/demov0

run-demofast: demofast
	$(SUDO) ./$(STAGECRIVAL_DIR)/demofast



#EDIT THIS PARTS


.PHONY: run-pyA run-pyA-test run-npA-test run-pyA-stress run-npA-stress


# test20 with windows 3 and 5: expect BUY at 7 and SELL at 10
run-pyA-test:
	$(PY) stageA.py data/Group05/test20.csv 3 5
run-npA-test:
	$(PY) stageA_numpy.py data/Group05/test20.csv 3 5

# stress_full.csv with the group windows; the results go to a file (a console is slow), the time still shows
run-pyA-stress:
	$(PY) src/stretch/stageA.py data/stress_full.csv 30 60 > src/stretch/pyA_stress.txt
run-npA-stress:
	$(PY) src/stretch/stageAnumpy.py data/stress_full.csv 30 60 > src/stretch/npA_stress.txt


# everything: the test20 check first, then the stress run
run-pyA: run-pyA-test run-npA-test run-pyA-stress run-npA-stress

# HAND-WRITTEN ASSEMBLY STRETCH GOAL (AArch64: Raspberry Pi only)
.PHONY: asm-bench run-asm asm-listing
ifeq ($(OS),Windows_NT)
asm-bench run-asm asm-listing:
	@echo "The assembly stretch goal is AArch64 code: build and run it on the Raspberry Pi."
else
asm-bench:
	$(CC) -O3 $(INC) -I src src/stretch/vwap.c src/stretch/vwap.S src/parser.c -o compiled/asm_bench
run-asm: asm-bench
	./compiled/asm_bench
asm-listing:
	$(CC) -O3 -S $(INC) -I src src/stretch/vwap.c -o compiled/vwap_O3.s
endif

# STRETCH GOALS: struct-of-arrays and percentile latency
.PHONY: soa-test soa percentile-test percentile run-soa run-percentile
soa-test:
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) -I src src/stretch/soa.c src/parser.c -o compiled/soa_test
soa:
	$(CC) -O2 -Dstress_file $(INC) -I src src/stretch/soa.c src/parser.c -o compiled/soa
percentile-test:
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) -I src src/stretch/percentile.c src/parser.c -o compiled/percentile_test
percentile:
	$(CC) -O2 -Dstress_file $(INC) -I src src/stretch/percentile.c src/parser.c -o compiled/percentile

run-soa: soa-test soa
	./compiled/soa_test
	./compiled/soa
run-percentile: percentile-test percentile
	./compiled/percentile_test
	./compiled/percentile