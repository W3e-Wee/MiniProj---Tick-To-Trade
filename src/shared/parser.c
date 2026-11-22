#include <stdio.h>
#include "parser.h"
#include "constant.h"

/**
 * Reads a decimal number straight from the characters of a row.
 * Builds the digits as a whole number and counts how many digits sit after the '.',
 * then divides once at the end.
 *
 * @param line[] One row of the csv file
 * @param i `int` pointer to position in the row
 *
 * @return A `double` type decimal value
 */
static double read_decimal(const char line[], int *i)
{
    long long digits = 0;
    long long scale = 1;

    while (line[*i] >= '0' && line[*i] <= '9')
    {
        /* Appends the next digit. E.g. 7 then '1' becomes 7 * 10 + 1 = 71 */
        digits = digits * 10 + (line[*i] - '0');
        (*i)++;
    }
    if (line[*i] == '.')
    {
        (*i)++;
        while (line[*i] >= '0' && line[*i] <= '9')
        {
            /*Accounts for '.' taking decimal place and uses scale to shift right*/
            digits = digits * 10 + (line[*i] - '0');
            scale *= 10;
            (*i)++;
        }
    }
    return (double)digits / (double)scale;
}

/**
 * Reads next valid trade from `fp` using `sscanf`.
 *
 * @param fp `FILE` pointer to read from.
 * @param ts Timestamp of the trade
 * @param price Price of trade
 * @param qty Quantity of the trade
 * @param is_buy Indicates BUY/SELL signal
 *
 * @return
 * `1` - Trade was read successfully |
 * `0` - End of read file.
 */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy)
{
    char line[MAX_LINES];
    long long id, t;
    double p, q, quote;
    char maker[16], best[16];

    while (fgets(line, sizeof line, fp) != NULL)
    {
        if (sscanf(line, "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s",
                   &id, &p, &q, &quote, &t, maker, best) == 7)
        {
            *ts = t;
            *price = p;
            *qty = q;
            *is_buy = (maker[0] == 'F' || maker[0] == 'f');
            return 1;
        }
    }
    return 0;
}

/**
 * Does same job as `read_trade` and returns the same values.
 *
 * The change is instead of `sscanf`, walks each row character by character.
 * Skips rows that don't start with a digit.
 */
int fast_parser(FILE *fp, long long *ts, double *price, double *qty, int *is_buy)
{
    char line[MAX_LINES];
    int i;
    long long t;

    while (fgets(line, sizeof line, fp) != NULL)
    {
        /* Goes to next row if trade id is invalid */
        if (line[0] < '0' || line[0] > '9')
            continue;
        i = 0;

        /* Goes through each character in the row till it hit ',' or end of row */
        while (line[i] != ',' && line[i] != '\0')
            i++;

        /* Goes to next row if end of row */
        if (line[i] == '\0')
            continue;
        i++;

        *price = read_decimal(line, &i);

        if (line[i] != ',')
            continue;
        i++;

        *qty = read_decimal(line, &i);

        if (line[i] != ',')
            continue;
        i++;

        while (line[i] != ',' && line[i] != '\0')
            i++; /* skip quoteQty */

        if (line[i] == '\0')
            continue;
        i++;

        t = 0;
        while (line[i] >= '0' && line[i] <= '9')
        {
            t = t * 10 + (line[i] - '0');
            i++;
        }

        if (line[i] != ',')
            continue;
        i++;

        *ts = t;
        *is_buy = (line[i] == 'F' || line[i] == 'f');

        return 1;
    }

    return 0;
}