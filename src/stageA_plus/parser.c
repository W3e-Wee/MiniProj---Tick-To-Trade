#include <stdio.h>
#include "parser.h"

/**
 * Reads each row from `*fp` and hands the values back through the pointers.
 *
 * @param fp `FILE` pointer to read from.
 * @param ts Pointer to a `long long` where the timestamp is stored.
 * @param price Pointer to a `double` where the price is stored.
 * @param qty Pointer to a `double` where the quantity is stored.
 * @param is_buy Pointer to an `int` where BUY/SELL indicator is stored (1 for buy, 0 for sell).
 *
 * @return
 * `1` - Trade was read successfully;
 * `0` - End of read file.
 */
int read_trade(FILE *fp, long long *ts, double *price, double *qty, int *is_buy)
{
    char line[256];
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
 * Converts ASCII value in csv file to decimal (e.g. '7' (ASCII 55) to 7)
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
        /*Goes through each no. char and converts from ascii to decimal value and shifts it left by multiplying it by 10*/
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
 * Does same job as `read_trade`, but walks the line character by character.
 */
int fast_parser(FILE *fp, long long *ts, double *price, double *qty, int *is_buy)
{
    char line[256];
    int i;
    long long t;

    while (fgets(line, sizeof line, fp) != NULL)
    {
        if (line[0] < '0' || line[0] > '9')     /* Goes to next row if trade id is invalid */
            continue;
        i = 0;
        while (line[i] != ',' && line[i] != '\0')   /* Goes through each character in the row till it hit ',' or end of row */
            i++;
        if (line[i] == '\0')     /* Goes to next row if end of row */
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