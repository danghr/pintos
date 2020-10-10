#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdio.h>

/* Definition of fixed-point numbers. */
typedef int fixed_point;

#define FP_FRACTION_BITS 14

fixed_point convert_int_to_fp (int x);
fixed_point convert_fp_to_int_zero (fixed_point x);
fixed_point convert_fp_to_int_nearest (fixed_point x);
fixed_point fp_add (fixed_point a, fixed_point b);
fixed_point fp_add_int (fixed_point a, int b);
fixed_point fp_sub (fixed_point a, fixed_point b);
fixed_point fp_sub_int (fixed_point a, int b);
fixed_point fp_mul (fixed_point a, fixed_point b);
fixed_point fp_mul_int (fixed_point a, int b);
fixed_point fp_div (fixed_point a, fixed_point b);
fixed_point fp_div_int (fixed_point a, int b);

#endif /* threads/fixed-point-h */
