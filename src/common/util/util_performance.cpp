#include "util_performance.h"


/// reverse:  reverse string s in place
void util_performance::reverse(char *s, int size)
{
    int i, j;
    char c;

    for (i = 0, j = size-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/// itoa:  convert n to characters in s
/// flaw: it does not correctly handle the most negative number
void util_performance::itoa(int n, char *s)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s, i);
}

/// like itoa but avoid sign checks
void util_performance::uitoa(unsigned n, char *s)
{
    int i;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    s[i] = '\0';
    reverse(s, i);
}
