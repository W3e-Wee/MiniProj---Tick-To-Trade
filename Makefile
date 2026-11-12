CC = gcc
CFLAGS = -Wall -Wextra -I./src/stage0
INC = -I provided
STAGE0_DIR = compiled/stage0
STAGEA_DIR = compiled/stageA
STAGEAPLUS_DIR = compiled/stageA_plus
STAGEB_DIR = compiled/stageB

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

run-ladder-full: run-ladder plot-ladder


