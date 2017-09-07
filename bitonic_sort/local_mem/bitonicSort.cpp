#include <cstring>
#include "../BitonicSort.h"

extern "C"
void bitonicSort(int *src, bool dir){
#pragma HLS interface m_axi port = src bundle = gmem 
#pragma HLS interface s_axilite port = src bundle = control
#pragma HLS interface s_axilite port = dir bundle = gmem
#pragma HLS interface s_axilite port = dir bundle = control
#pragma HLS interface s_axilite port = return bundle = control
	const int ARRAY_SIZE = 1 << EXP;
	int local[ARRAY_SIZE];
	memcpy(local, src, sizeof(int)*ARRAY_SIZE);
	BitonicSort<int, EXP> sorter(local);
	sorter.sort(dir);
	memcpy(src, local, sizeof(int)*ARRAY_SIZE);
}
