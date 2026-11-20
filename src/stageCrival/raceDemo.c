#include <stdio.h>
#include "../shared/constant.h"
#include "../stageCrival/common/replay.h"
#include "../stageCrival/common/rival.h"
#include "../stageCrival/common/race.h"

#if defined(demo_v0)
#include "v0Bot.h"
#define BOT_NAME "V0 Bot"
#define BOT_STEP v0_step
#define BOT_START() v0_reset()
#define BOT_STOP() v0_close()
#elif defined(demo_fast)
#define BOT_NAME "FASTEST"
#define BOT_STEP check_trade
#define BOT_START() 1
#define BOT_STOP() ((void)0)
#else
#error "Build with -Ddemo_v0 or -Ddemo_fast (make run-demo BOT=v0|fast)"
#endif

int main(void)
{
    const Bot bot = {BOT_NAME, BOT_STEP};

    if (!load_trades(DATA_FILE))
    {
        printf("Error: Unable to open %s\n", DATA_FILE);
        return 1;
    }
    if (!BOT_START())
    {
        printf("Unable to open %s\n", DATA_FILE);
        free_trades();
        return 1;
    }

    run_race(&bot);

    BOT_STOP();
    free_trades();
    return 0;
}