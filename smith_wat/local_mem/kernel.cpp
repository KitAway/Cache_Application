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
#pragma HLS interface m_axi port=matrix bundle=gmem
#pragma HLS interface m_axi port=maxIndex bundle=gmem
#pragma HLS interface m_axi port=s1 bundle=gmem
#pragma HLS interface m_axi port=s2 bundle=gmem
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
    int localMaxIndex = 0;

    // Global memory transactions have high latency
		// Data is copied to a local memory for bettern performance
    char localS1[N];
    char localS2[N];
    int localMatrix[N*N];

    for (int i = 1; i < N; i++) {
#pragma HLS pipeline
        localS1[i] = s1[i];
    }
    for (int i = 1; i < N; i++) {
#pragma HLS pipeline
        localS2[i] = s2[i];
    }
    for (int i = 0; i < N * N; i++) {
#pragma HLS pipeline
        localMatrix[i] = 0;
    }

    for (short index = N; index < N * N; index++)
    {
#pragma HLS pipeline
        short dir = CENTER;
        short val = 0;
        short j = index % N;
        if (j == 0) { // Skip the first column
            west = 0;
            northwest = 0;
            continue;
        }
        short i = index / N;
        short2 temp = localMatrix[index - N];
        north = temp.x;
        const short match = (localS1[j] == localS2[i]) ? MATCH : MISS_MATCH;
        short val1 = northwest + match;

        if (val1 > val) {
            val = val1;
            dir = NORTH_WEST;
        }
        val1 = north + GAP;
        if (val1 > val) {
            val = val1;
            dir = NORTH;
        }
        val1 = west + GAP;
        if (val1 > val) {
            val = val1;
            dir = WEST;
        }
        temp.x = val;
        temp.y = dir;
        localMatrix[index] = as_int(temp);
        west = val;
        northwest = north;
        if (val > maxValue) {
            localMaxIndex = index;
            maxValue = val;
        }
    }
    *maxIndex = localMaxIndex;

    for (int i = 0; i < N * N; i++) {
#pragma HLS pipeline
        matrix[i] = localMatrix[i];
    }
}

