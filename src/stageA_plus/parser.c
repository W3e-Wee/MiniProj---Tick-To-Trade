#include <stdio.h>
#include "parser.h"

/* Week 8: sscanf version, results handed back through pointers */

/***
 * Reads the next trade from fp and hands the values back through the pointers.
 *
 * @param fp - The file pointer to read from.
 * @param ts - Pointer to a long long where the timestamp will be stored.
 * @param price - Pointer to a double where the price will be stored.
 * @param qty - Pointer to a double where the quantity will be stored.
 * @param is_buy - Pointer to an int where the buy/sell indicator will be stored (1 for buy, 0 for sell).
 *
 * @return Returns 1 if a trade was read successfully, 0 at the end of the file.
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

/* Week 9 helper (only used inside this file): turns digits like 75436.94000000 into a double and moves *i past the number */
double read_decimal(const char line[], int *i)
{
    long long digits = 0;
    long long scale = 1;

    
    while (line[*i] >= '0' && line[*i] <= '9')
    {
        /*Goes through each no. char and converts from ascii to decimal value and shifts it left by multiplying it by 10*/
        digits *= 10 + (line[*i] - '0');
        (*i)++;
    }
    if (line[*i] == '.')
    {
        (*i)++;
        while (line[*i] >= '0' && line[*i] <= '9')
        {
            /*Accounts for '.' taking decimal place and uses scale to shift right*/
            digits *= 10 + (line[*i] - '0');
            scale *= 10;
            (*i)++;
        }
    }
    return (double)digits / (double)scale;
}

/* Week 9: same job as read_trade, but walks the line character by character */
int fast_parser(FILE *fp, long long *ts, double *price, double *qty, int *is_buy)
{
    char line[256];
    int i;
    long long t;

    while (fgets(line, sizeof line, fp) != NULL)
    {
        if (line[0] < '0' || line[0] > '9') /*Ignores headers or empty lines that dont have trade ids*/
            continue; 
        i = 0;
        while (line[i] != ',' && line[i] != '\0') /* skip trade id */
            i++; 
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