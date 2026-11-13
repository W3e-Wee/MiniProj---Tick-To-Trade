void check_signal(double d_prev, double d, int *buy, int *sell)
{
    if (d_prev >= 0 && d < 0)
    {
        (*sell)++; /* SELL */
    }
    else if (d_prev <= 0 && d > 0)
    {
        (*buy)++; /* BUY */
    }
}