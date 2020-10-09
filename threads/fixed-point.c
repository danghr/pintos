#include <stdio.h>
#include "threads/fixed-point.h"

/* Convert an integer to a fixed point number */
fixed_point
convert_int_to_fp (int x)
{
  return x << FP_FRACTION_BITS;
}

/* Convert a fixed point number to an integer
   (Rounding toward zero) */
fixed_point
convert_fp_to_int_zero (fixed_point x)
{
  return x >> FP_FRACTION_BITS;
}

/* Convert a fixed point number to an integer
   (Rounding to nearest) */
fixed_point
convert_fp_to_int_nearest (fixed_point x)
{
  if (x >= 0)
    return (x + (FP_FRACTION_BITS >> 1)) >> FP_FRACTION_BITS;
  return (x - (FP_FRACTION_BITS >> 1)) >> FP_FRACTION_BITS;
}

/* Addition of two fixed point numbers */
fixed_point
fp_add (fixed_point a, fixed_point b)
{
  return a + b;
}

/* Addition of a fixed point number and an integer */
fixed_point
fp_add_int (fixed_point a, int n)
{
  return a + convert_int_to_fp (n);
}

/* Subtraction of two fixed point numbers */
fixed_point
fp_sub (fixed_point a, fixed_point b)
{
  return a - b;
}

/* Subtraction of a fixed point number and an integer */
fixed_point
fp_sub_int (fixed_point a, int n)
{
  return a - convert_int_to_fp (n);
}

/* Subtraction of two fixed point numbers */
fixed_point
fp_mul (fixed_point a, fixed_point b)
{
  return ((int64_t)a) * b >> FP_FRACTION_BITS;
}

/* Subtraction of a fixed point number and an integer */
fixed_point
fp_mul_int (fixed_point a, int n)
{
  return a * n;
}

/* Subtraction of two fixed point numbers */
fixed_point
fp_div (fixed_point a, fixed_point b)
{
  return ((int64_t)a) << FP_FRACTION_BITS / b;
}

/* Subtraction of a fixed point number and an integer */
fixed_point
fp_div_int (fixed_point a, int n)
{
  return a / n;
}