class short2{
	public:
		short x;
		short y;
		short2():x(0),y(0){}
		short2(int m){
			x=m;
			y=m>>16;
		}
};
int as_int(short2 &s){
	int m = s.y;
	return (m<<16) | s.x;
}
#define N 85
	extern "C"
void smithwaterman (int *matrix,
		int *maxIndex,
		const char *s1,
		const char *s2)
{
#pragma HLS interface m_axi port=matrix bundle=gmem0
#pragma HLS interface m_axi port=maxIndex bundle=gmem1
#pragma HLS interface m_axi port=s1 bundle=gmem2
#pragma HLS interface m_axi port=s2 bundle=gmem3
#pragma HLS interface s_axilite port=matrix bundle=control
#pragma HLS interface s_axilite port=maxIndex bundle=control
#pragma HLS interface s_axilite port=s1 bundle=control
#pragma HLS interface s_axilite port=s2 bundle=control
#pragma HLS interface s_axilite port=return bundle=control
	short north = 0;
	short west = 0;
	short northwest = 0;
	const short GAP = -1;
	const short MATCH = 2;
	const short MISS_MATCH = -1;
	const short CENTER = 0;
	const short NORTH = 1;
	const short NORTH_WEST = 2;
	const short WEST = 3;
	int maxValue = 0;

	// Global memory transactions have high latency
	// Data is copied to a local memory for bettern performance

	loop_main:for (short index = 0; index < N * N; index++)
	{
#pragma HLS PIPELINE II=7
//#pragma HLS DEPENDENCE variable=matrix array RAW true
		short dir = CENTER;
		short val = 0;
		short j = index % N;
		if (index < N || j == 0) { // Skip the first column
			matrix[index] = 0;
			continue;
		}
		short i = index / N;
		short2 temp = matrix[index - N];
		north = temp.x;
		const short match = (s1[j] == s2[i]) ? MATCH : MISS_MATCH;
		short2 nw = matrix[index - N - 1];
		short val1 = nw.x + match;

		if (val1 > val) {
			val = val1;
			dir = NORTH_WEST;
		}
		val1 = north + GAP;
		if (val1 > val) {
			val = val1;
			dir = NORTH;
		}
		short2 wt = matrix[index - 1];
		val1 = wt.x + GAP;
		if (val1 > val) {
			val = val1;
			dir = WEST;
		}
		temp.x = val;
		temp.y = dir;
		matrix[index] = as_int(temp);
		if (val > maxValue) {
			*maxIndex = index;
			maxValue = val;
		}
	}
}

