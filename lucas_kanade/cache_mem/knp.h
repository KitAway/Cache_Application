#pragma once

#include <math.h>
#include <stdio.h>
#include "../param.h"


//2.0 #define of constant variables
#ifndef M_PI
#define M_PI 3.1415926535f
#endif
// Flow vector scaling factor
#define FLOW_SCALING_FACTOR (0.25f)  //(1.0f/4.0f)

typedef int mType;



#ifndef CACHE
extern "C" void knp(
		unsigned char * im1,
		unsigned char * im2,
		float * out
		);
#else

#include "/home/liang/WORKSPACE/Vivado/cache/data/cache_only.h"
typedef CACHE_ONLY::Cache<unsigned char, 3, 5> CacheM;
typedef CACHE_ONLY::Cache<unsigned char, 0, 6> CacheN;
typedef CACHE_ONLY::Cache<float, 0, 4> CacheF;
/*
typedef CACHE_ONLY::Cache<unsigned char, 2, 3> CacheM;
typedef CACHE_ONLY::Cache<unsigned char, 0, 3> CacheN;
typedef CACHE_ONLY::Cache<float, 0, 3> CacheF;

*/

extern "C" void knp(CacheM::DataType *im1,
		CacheN::DataType *im2,
		CacheF::DataType *  out
		);
#endif
