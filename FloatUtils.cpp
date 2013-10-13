// Floating point utilites
// ***********************
// Round()  Rounds a number to a specified number of digits.
// ***********************
// Designed and written by Simon Hughes (shughes@netcomuk.co.uk)
// This code has been fully tested, but should you find any bugs, then please
// let me know. The code is free, but please leave my name and e-mail address intact.
// ***********************
// File: FloatUtils.cpp
// Date: 30th November 1999
// Notice: If you modify the code in any way and redistribute it, please make a comment.
// ***********************
// Modifications:
// Removed all code except for the Round function.

#include "FloatUtils.h"
#include <math.h>

// Rounds a number to a specified number of digits.
// Number is the number you want to round.
// Num_digits specifies the number of digits to which you want to round number.
// If num_digits is greater than 0, then number is rounded to the specified number of decimal places.
// If num_digits is 0, then number is rounded to the nearest integer.
// Examples
//		ROUND(2.15, 1)		equals 2.2
//		ROUND(2.149, 1)		equals 2.1
//		ROUND(-1.475, 2)	equals -1.48
float Round(const float &number, const int num_digits)
{
	float doComplete5i, doComplete5(number * powf(10.0f, (float) (num_digits + 1)));
	
	if(number < 0.0f)
		doComplete5 -= 5.0f;
	else
		doComplete5 += 5.0f;
	
	doComplete5 /= 10.0f;
	modff(doComplete5, &doComplete5i);
	
	return doComplete5i / powf(10.0f, (float) num_digits);
}

double RoundDouble(double doValue, int nPrecision)
{
	static const double doBase = 10.0;
	double doComplete5, doComplete5i;
	
	doComplete5 = doValue * pow(doBase, (double) (nPrecision + 1));
	
	if(doValue < 0.0)
		doComplete5 -= 5.0;
	else
		doComplete5 += 5.0;
	
	doComplete5 /= doBase;
	modf(doComplete5, &doComplete5i);
	
	return doComplete5i / pow(doBase, (double) nPrecision);
}
