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
RIVAL_SRC = src/stageCrival/compareRival.c src/stageCrival/common/rival.c src/stageCrival/v0malloc.c src/stageCrival/vfastestmalloc.c src/stageA_plus/parser.c provided/trade_io.c
V0_SRC = src/stageCrival/v0malloc.c src/stageCrival/common/rival.c src/stageA_plus/parser.c provided/trade_io.c
FAST_SRC = src/stageCrival/vfastestmalloc.c src/stageCrival/common/rival.c src/stageA_plus/parser.c
RACE_SPEED = 10000
RACE_LED = #for the rpi
ifeq ($(OS),Windows_NT)
    RMDIR = if exist compiled rmdir /s /q compiled
else
    RMDIR = rm -rf compiled
endif

.PHONY: clean stage0 loop loop-o1 loop-o2 loop-o3 stage0_benchmark stage0_benchmark-o2 \
        stageA stageA-test run-stageA run-stageA-test \
        stageAplus run-stageAplus \
        stageB stageB-test stageB-stress run-stageB ladder-test ladder-stress ladder-dev \
        run-ladder-test run-ladder-stress run-ladder-dev run-ladder plot-ladder run-ladder-full \
        stageC stageC-test stageC-stress run-stageC run-stageC-test run-stageC-stress \
        rival rival-test rival-stress run-rival run-rival-test run-rival-stress plot-rival \
        demov0 demofast run-demov0 run-demofast


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

all: stage0 stageA stageA-test stageAplus stageB stageB-test stageB-stress ladder-test ladder-dev ladder-stress stageC stageC-test stageC-stress rival rival-test rival-stress

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
	$(CC) -O2 $(INC)  src/stageA/stageA.c provided/trade_io.c -o $(STAGEA_DIR)/stageA

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
	$(call fixpath,./$(STAGEAPLUS_DIR)/stageAplus) > $(call fixpath,$(STAGEAPLUS_DIR)/stageAplus_results.txt)

# =====================================
# Stage B build and execute commands
# =====================================
stageB: 
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB

stageB-test:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB_test

stageB-stress:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -Dstress_file $(INC) src/stageB/stageB.c src/stageA_plus/parser.c -o $(STAGEB_DIR)/stageB_stress

run-stageB: stageB
	$(call fixpath,./$(STAGEB_DIR)/stageB) > $(call fixpath,$(STAGEB_DIR)/stageB_dev_results.txt)

ladder-test:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_test

ladder-stress:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -Dstress_file $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_stress

ladder-dev:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 $(INC) src/stageB/ladder.c src/stageA_plus/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_dev

# each run- target rebuilds first, so it can never run an old compiled copy
run-ladder-test: ladder-test
	$(call fixpath,./$(STAGEB_DIR)/ladder_test) > $(call fixpath,$(STAGEB_DIR)/ladder_test_results.txt)

run-ladder-stress: ladder-stress
	$(call fixpath,./$(STAGEB_DIR)/ladder_stress) > $(call fixpath,$(STAGEB_DIR)/ladder_stress_results.txt)

run-ladder-dev: ladder-dev
	$(call fixpath,./$(STAGEB_DIR)/ladder_dev) > $(call fixpath,$(STAGEB_DIR)/ladder_dev_results.txt)

# the full ladder: check on test20 first, then the stress_full.csv run
run-ladder: ladder-test ladder-stress
	@echo "--- test20 check: expect BUY at 7 and SELL at 10 for every version ---"
	$(call fixpath,./$(STAGEB_DIR)/ladder_test) > $(call fixpath,$(STAGEB_DIR)/ladder_test_results.txt)

	@echo "--- stress_full.csv ladder (takes a few minutes) ---"
	$(call fixpath,./$(STAGEB_DIR)/ladder_stress) > $(call fixpath,$(STAGEB_DIR)/ladder_stress_results.txt)

plot-ladder:
	gnuplot plots/plot_ladder
	@echo "Plotted ladder to ladder.png"

run-ladder-full: run-ladder plot-ladder
# =====================================
# Stage C build and execute commands
# =====================================
stageC: 
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 $(INC) src/stageC/stageC.c src/stageA_plus/parser.c src/stageCrival/common/rival.c -o $(STAGEC_DIR)/stageC

stageC-test:
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageC/stageC.c src/stageA_plus/parser.c src/stageCrival/common/rival.c -o $(STAGEC_DIR)/stageC_test

stageC-stress:
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 -Dstress_file $(INC) src/stageC/stageC.c src/stageA_plus/parser.c src/stageCrival/common/rival.c -o $(STAGEC_DIR)/stageC_stress

run-stageC: stageC
	$(call fixpath,./$(STAGEC_DIR)/stageC) > $(call fixpath,$(STAGEC_DIR)/stageC_results.txt)

run-stageC-test: stageC-test
	$(call fixpath,./$(STAGEC_DIR)/stageC_test) > $(call fixpath,$(STAGEC_DIR)/stageC_test_results.txt)

run-stageC-stress: stageC-stress
	$(call fixpath,./$(STAGEC_DIR)/stageC_stress) > $(call fixpath,$(STAGEC_DIR)/stageC_stress_results.txt)

# ===============================================
# Stage C Rival Bot build and execute commands
# ===============================================
rival-test:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) $(RIVAL_SRC) -lm -o $(STAGECRIVAL_DIR)/rival_test

rival-stress:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -Dstress_file $(INC) $(RIVAL_SRC) -lm -o $(STAGECRIVAL_DIR)/rival_stress

rival: rival-test rival-stress

run-rival-test: rival-test
	$(call fixpath,./$(STAGECRIVAL_DIR)/rival_test) > $(call fixpath,$(STAGECRIVAL_DIR)/rival_test_results.txt)

run-rival-stress: rival-stress
	$(call fixpath,./$(STAGECRIVAL_DIR)/rival_stress) > $(call fixpath,$(STAGECRIVAL_DIR)/rival_stress_results.txt)

run-rival: rival-test rival-stress
	@echo "--- test20 check: both bots must show BUY at 7 and SELL at 10 ---"
	$(call fixpath,./$(STAGECRIVAL_DIR)/rival_test) > $(call fixpath,$(STAGECRIVAL_DIR)/rival_test_results.txt)
	@echo "--- stress_full.csv rival sweep ---"
	$(call fixpath,./$(STAGECRIVAL_DIR)/rival_stress) > $(call fixpath,$(STAGECRIVAL_DIR)/rival_stress_results.txt)
	@echo "--- plotting rival sweep ---"
	gnuplot plots/plot_rival

plot-rival:
	gnuplot plots/plot_rival

#demo
demov0:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -Dstress_file -Ddemo_v0 -DSPEED=$(RACE_SPEED) $(INC) $(V0_SRC) -o $(STAGECRIVAL_DIR)/demov0

demofast:	
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -Dstress_file -Ddemo_fast -DSPEED=$(RACE_SPEED) $(RACE_LED) $(INC) $(FAST_SRC) -o $(STAGECRIVAL_DIR)/demofast

run-demov0: demov0
	$(call fixpath,./$(STAGECRIVAL_DIR)/demov0)

run-demofast: demofast
	$(SUDO) $(call fixpath,./$(STAGECRIVAL_DIR)/demofast)
	
clean:
	$(RMDIR)