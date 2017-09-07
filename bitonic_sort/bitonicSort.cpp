#include <cstring>
#include <iostream>

#ifdef CACHE
#include "BitonicSortCache.h"
extern "C"
void bitonicSort(CacheA::DataType *src, bool dir){
#pragma HLS interface m_axi port = src bundle = gmem depth = 8

#pragma HLS interface s_axilite port = src bundle = control
#pragma HLS interface s_axilite port = dir bundle = gmem
#pragma HLS interface s_axilite port = dir bundle = control
#pragma HLS interface s_axilite port = return bundle = control
	BitonicSortCache<int, EXP, CACHE> sorter(src);
	sorter.sort(dir);
}
#else

#include "BitonicSort.h"
#ifdef GLOBAL
extern "C"
void bitonicSort(int *src, bool dir){
#pragma HLS interface m_axi port = src bundle = gmem
#pragma HLS interface s_axilite port = src bundle = control
#pragma HLS interface s_axilite port = dir bundle = gmem
#pragma HLS interface s_axilite port = dir bundle = control
#pragma HLS interface s_axilite port = return bundle = control

	BitonicSort<int, EXP> sorter(src);
	sorter.sort(dir);
}
#else
extern "C"
void bitonicSort(int *src, bool dir){
#pragma HLS interface m_axi port = src bundle = gmem

#pragma HLS interface s_axilite port = src bundle = control
#pragma HLS interface s_axilite port = dir bundle = gmem
#pragma HLS interface s_axilite port = dir bundle = control
#pragma HLS interface s_axilite port = return bundle = control

	BitonicSortLocal<int, EXP, LIMIT> sorter(src);
	sorter.sort(dir);
}
#endif
#endif
