#include <stdio.h>
#include "threads/fixed-point.h"

/* Convert an integer to a fixed point number. */
fixed_point
convert_int_to_fp (int x)
{
  fixed_point ret;
  ret.value = x << FP_FRACTION_BITS;
  return ret;
}

/* Convert a fixed point number to an integer
   (Rounding toward zero). */
int
convert_fp_to_int_zero (fixed_point x)
{
  return x.value >> FP_FRACTION_BITS;
}

/* Convert a fixed point number to an integer
   (Rounding to nearest). */
int
convert_fp_to_int_nearest (fixed_point x)
{
  if (x.value >= 0)
    return (x.value + (FP_FRACTION_BITS >> 1)) >> FP_FRACTION_BITS;
  return (x.value - (FP_FRACTION_BITS >> 1)) >> FP_FRACTION_BITS;
}

/* Addition of two fixed point numbers. */
fixed_point
fp_add (fixed_point a, fixed_point b)
{
  fixed_point ret;
  ret.value = a.value + b.value;
  return ret;
}

/* Addition of a fixed point number and an integer. */
fixed_point
fp_add_int (fixed_point a, int n)
{
  fixed_point ret;
  ret.value = a.value + convert_int_to_fp (n).value;
  return ret;
}

/* Subtraction of two fixed point numbers. */
fixed_point
fp_sub (fixed_point a, fixed_point b)
{
  fixed_point ret;
  ret.value = a.value - b.value;
  return ret;
}

/* Subtraction of a fixed point number and an integer. */
fixed_point
fp_sub_int (fixed_point a, int n)
{
  fixed_point ret;
  ret.value = a.value - convert_int_to_fp (n).value;
  return ret;
}

/* Subtraction of two fixed point numbers. */
fixed_point
fp_mul (fixed_point a, fixed_point b)
{
  fixed_point ret;
  ret.value = ((int64_t)a.value) * b.value >> FP_FRACTION_BITS;
  return ret;
}

/* Subtraction of a fixed point number and an integer. */
fixed_point
fp_mul_int (fixed_point a, int n)
{
  fixed_point ret;
  ret.value = a.value * n;
  return ret;
}

/* Subtraction of two fixed point numbers. */
fixed_point
fp_div (fixed_point a, fixed_point b)
{
  fixed_point ret;
  ret.value = (((int64_t)a.value) << FP_FRACTION_BITS) / b.value;
  return ret;
}

/* Subtraction of a fixed point number and an integer. */
fixed_point
fp_div_int (fixed_point a, int n)
{
  fixed_point ret;
  ret.value = a.value / n;
  return ret;
}