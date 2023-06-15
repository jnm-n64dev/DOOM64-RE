
/* m_fixed.c -- fixed point implementation */

#include "i_main.h"
#include "doomdef.h"
#include "p_spec.h"
#include "r_local.h"

/*
===============
=
= FixedDiv
=
===============
*/
fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    fixed_t aa, bb;
    unsigned c;
    int sign;

    sign = a ^ b;

    if (a < 0)
    {
        aa = -a;
    }
    else
    {
        aa = a;
    }

    if (b < 0)
    {
        bb = -b;
    }
    else
    {
        bb = b;
    }

    if ((unsigned)(aa >> 14) >= bb)
    {
        if (sign < 0)
        {
            c = MININT;
        }
        else
        {
            c = MAXINT;
        }
    }
    else
    {
        c = (fixed_t)FixedDiv2(a, b);
    }

    return c;
}

/*
===============
=
= FixedMul
=
===============
*/
fixed_t FixedMul(fixed_t a, fixed_t b)
{
    s64 result = ((s64)a * (s64)b) >> 16;

    return (fixed_t)result;
}

/*
===============
=
= FixedDiv2
=
===============
*/
fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    s64 result = ((s64)a << 16) / (s64)b;

    return (fixed_t)result;
}