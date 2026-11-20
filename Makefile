CC = gcc
CFLAGS = -Wall -Wextra
INC = -I provided
STAGE0_DIR = compiled/stage0
STAGEA_DIR = compiled/stageA
STAGEAPLUS_DIR = compiled/stageA_plus
STAGEB_DIR = compiled/stageB
STAGEC_DIR = compiled/stageC
STAGECRIVAL_DIR = compiled/stageC_rival
 
#rival
RIVAL_SRC = src/stageCrival/compareRival.c src/stageCrival/common/rival.c src/stageCrival/v0Bot.c src/shared/parser.c provided/trade_io.c
V0_SRC = src/stageCrival/v0Bot.c src/stageCrival/common/rival.c src/shared/parser.c provided/trade_io.c
FAST_SRC = src/stageCrival/common/rival.c src/shared/parser.c
REPLAY_SRC = src/stageCrival/common/replay.c
RACE_SRC = src/stageCrival/race_demo.c src/stageCrival/common/race.c $(REPLAY_SRC)
STAGEC_SRC = src/stageC/stageC.c src/shared/parser.c src/stageCrival/common/rival.c $(REPLAY_SRC)
RACE_SPEED = 10000
RACE_LED = #for the rpi
 
# which bot the race window runs: make run-demo BOT=v0   or   make run-demo BOT=fast
BOT = fast
ifeq ($(BOT),v0)
    DEMO_SRC = $(V0_SRC)
else ifeq ($(BOT),fast)
    DEMO_SRC = $(FAST_SRC)
else
    $(error BOT must be v0 or fast)
endif
 
.PHONY: clean rival rival-test rival-stress run-rival run-rival-test run-rival-stress plot-rival \
        demo run-demo demov0 demofast run-demov0 run-demofast run-ladder-full
 
SUDO =
# OS specfic checks to create the relevant folders
ifeq ($(OS),Windows_NT)
    SUDO =
    fixpath = $(subst /,\,$(1))
    MKDIR = if not exist $(call fixpath,$(1)) mkdir $(call fixpath,$(1))
    RMDIR = if exist $(call fixpath,$(1)) rmdir /s /q $(call fixpath,$(1))
else
    SUDO = sudo
    fixpath = $(1)
    MKDIR = mkdir -p $(1)
    RMDIR = rm -rf $(1)
endif
 
all: stage0 stageA stageA-test stageAplus stageB stageB-test stageB-stress ladder-test ladder-dev ladder-stress stageC stageC-test stageC-stress rival rival-test rival-stress
 
# delete every build output (binaries, results, plots)
clean:
	$(call RMDIR,compiled)
 
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
	$(CC) -O2 $(INC) src/stage0/stage0_helper.c src/stageA_plus/stageAplus.c src/shared/parser.c -o $(STAGEAPLUS_DIR)/stageAplus
 
run-stageAplus: stageAplus
	./$(STAGEAPLUS_DIR)/stageAplus
 
# =====================================
# Stage B build and execute commands
# =====================================
stageB: 
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 $(INC) src/stageB/stageB.c src/shared/parser.c -o $(STAGEB_DIR)/stageB
 
stageB-test:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/stageB.c src/shared/parser.c -o $(STAGEB_DIR)/stageB_test
 
stageB-stress:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -Dstress_file $(INC) src/stageB/stageB.c src/shared/parser.c -o $(STAGEB_DIR)/stageB_stress
 
run-stageB:
	./$(STAGEB_DIR)/stageB
 
ladder-test:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) src/stageB/ladder.c src/shared/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_test
 
ladder-stress:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 -Dstress_file $(INC) src/stageB/ladder.c src/shared/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_stress
 
ladder-dev:
	$(call MKDIR,$(STAGEB_DIR))
	$(CC) -O2 $(INC) src/stageB/ladder.c src/shared/parser.c provided/trade_io.c src/stage0/stage0_helper.c src/stageB/versions.c -o $(STAGEB_DIR)/ladder_dev
 
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
 
# =====================================
# Stage C build and execute commands
# =====================================
stageC: 
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 $(INC) $(STAGEC_SRC) -o $(STAGEC_DIR)/stageC
 
stageC-test:
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) $(STAGEC_SRC) -o $(STAGEC_DIR)/stageC_test
 
stageC-stress:
	$(call MKDIR,$(STAGEC_DIR))
	$(CC) -O2 -Dstress_file $(INC) $(STAGEC_SRC) -o $(STAGEC_DIR)/stageC_stress
 
run-stageC: stageC
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
	$(CC) -O2 -DSHORT_WIN=3 -DLONG_WIN=5 -Ddata_file $(INC) $(RIVAL_SRC) -lm -o $(STAGECRIVAL_DIR)/rival_test
 
rival-stress:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -Dstress_file $(INC) $(RIVAL_SRC) -lm -o $(STAGECRIVAL_DIR)/rival_stress
 
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
 
# ===============================================
# Race window: the bot is picked with BOT=v0 or BOT=fast
# ===============================================
demo:
	$(call MKDIR,$(STAGECRIVAL_DIR))
	$(CC) -O2 -Dstress_file -Ddemo_$(BOT) -DSPEED=$(RACE_SPEED) $(RACE_LED) $(INC) $(RACE_SRC) $(DEMO_SRC) -o $(STAGECRIVAL_DIR)/demo_$(BOT)
 
run-demo: demo
	$(SUDO) $(call fixpath,./$(STAGECRIVAL_DIR)/demo_$(BOT))
 
# the old names still work
demov0:
	$(MAKE) demo BOT=v0
 
demofast:
	$(MAKE) demo BOT=fast
 
run-demov0:
	$(MAKE) run-demo BOT=v0
 
run-demofast:
	$(MAKE) run-demo BOT=fast
 