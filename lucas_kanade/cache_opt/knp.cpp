#include "knp.h"

int PO(int x, int y){
	return x + y * WIDTH;
}

int Px(int x){
#pragma HLS INLINE
	if(x >=WIDTH)
		x =  WIDTH -1;
	else if (x < 0)
		x = 0;
	return x;
}
int Py(int y){
#pragma HLS INLINE
	if(y >=HEIGHT)
		y =  HEIGHT - 1;
	else if (y < 0)
		y = 0;
	return y;
}
int P(int x, int y){
#pragma HLS INLINE
	return Py(y) * WIDTH + Px(x);
}
//4.0 Structs and classes definitions
typedef struct colorW
{
	//public:
	int s0;
	int s1;
	int s2;
	//int s3;
}colS;
//5.0 Inline functions
//5.1 Calculate color by using color wheel:

void computeColor(float fx, float fy, unsigned char *pix)
{
	// hard-coded colorwheel constants
	const int l_ncols = 55;
	int l_colorwheel[] = {
		255, 0, 0,
		255, 17, 0,
		255, 34, 0,
		255, 51, 0,
		255, 68, 0,
		255, 85, 0,
		255, 102, 0,
		255, 119, 0,
		255, 136, 0,
		255, 153, 0,
		255, 170, 0,
		255, 187, 0,
		255, 204, 0,
		255, 221, 0,
		255, 238, 0,
		255, 255, 0,
		213, 255, 0,
		170, 255, 0,
		128, 255, 0,
		85, 255, 0,
		43, 255, 0,
		0, 255, 0,
		0, 255, 63,
		0, 255, 127,
		0, 255, 191,
		0, 255, 255,
		0, 232, 255,
		0, 209, 255,
		0, 186, 255,
		0, 163, 255,
		0, 140, 255,
		0, 116, 255,
		0, 93, 255,
		0, 70, 255,
		0, 47, 255,
		0, 24, 255,
		0, 0, 255,
		19, 0, 255,
		39, 0, 255,
		58, 0, 255,
		78, 0, 255,
		98, 0, 255,
		117, 0, 255,
		137, 0, 255,
		156, 0, 255,
		176, 0, 255,
		196, 0, 255,
		215, 0, 255,
		235, 0, 255,
		255, 0, 255,
		255, 0, 213,
		255, 0, 170,
		255, 0, 128,
		255, 0, 85,
		255, 0, 43
	};
	float rad = sqrt(fx * fx + fy * fy);
	float a = atan2(-fy, -fx) / (float)M_PI;
	float fk = (a + 1.0f) / 2.0f * (l_ncols - 1);
	int k0 = (int)fk;

	colS col;
	col.s0 = *(l_colorwheel + k0 * 3 + 0);
	col.s1 = *(l_colorwheel + k0 * 3 + 1);
	col.s2 = *(l_colorwheel + k0 * 3 + 2);
	if (rad > 1) rad = 1; // max at 1
	pix[0] = (unsigned char) (255 - rad * (255 - col.s0));
	pix[1] = (unsigned char) (col.s1 = 255 - rad * (255 - col.s1));
	pix[2] = (unsigned char) (255 - rad * (255 - col.s2));
}
//5.2. Find inverse of 2x2 int matrix,  returns 1 on success, 0 on failure
int get_matrix_inv(mType* G, float* G_inv)
{
	float detG = (float)G[0] * G[3] - (float)G[1] * G[2];
	if (detG <= 1.0f) { return 0; }
	float detG_inv = 1.0f / detG;
	G_inv[0] = G[3] * detG_inv;		G_inv[1] = -G[1] * detG_inv;
	G_inv[2] = -G[2] * detG_inv;	G_inv[3] = G[0] * detG_inv;
	return 1;
}
extern "C"
void knp(CacheM::DataType *im1p,
		CacheN::DataType *im2p,
		CacheF::DataType  *  outp
		)
{

#pragma HLS INTERFACE m_axi port=im1p bundle=gmem0
#pragma HLS INTERFACE m_axi port=im2p bundle=gmem1
#pragma HLS INTERFACE m_axi port=outp bundle=gmem2

#pragma HLS INTERFACE s_axilite port=im1p bundle=control
#pragma HLS INTERFACE s_axilite port=im2p bundle=control
#pragma HLS INTERFACE s_axilite port=outp bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	CacheM im1(im1p);
	CacheN im2(im2p);
	CacheF out(outp);
loop_main:for(int j=0;j<HEIGHT;j++)
	for(int i = 0; i<WIDTH;i++){
		float G_inv[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#pragma HLS ARRAY_PARTITION variable=G_inv complete dim=1
		mType G[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#pragma HLS ARRAY_PARTITION variable=G complete dim=1
		mType b_k[2] = { 0.0f, 0.0f };
#pragma HLS ARRAY_PARTITION variable=b_k complete dim=1
		int x;

		unsigned char tmp0 = im1[Py(j-WINDOW_SIZE-1)*WIDTH];
		unsigned char tmp1 = im1[Py(j-WINDOW_SIZE)*WIDTH];
loop_column:for (int wj = -WINDOW_SIZE; wj<=WINDOW_SIZE;wj++)
		{
			unsigned char tmp1 = im1[Py(j+wj+1)*WIDTH];
loop_row:	for (int wi = -WINDOW_SIZE; wi<=WINDOW_SIZE;wi++)
			{
#pragma HLS PIPELINE
				int px = Px(i+wi), py = Py(j+wj);
				int im2_val = im2[px + py * WIDTH];

			//	int deltaIk = im1[px + py * WIDTH] - im2_val;
				int deltaIk = im1.retrieve(px + py * WIDTH) - im2_val;
				int cx=Px(i + wi +1) + py * WIDTH, dx = Px(i + wi - 1) + py * WIDTH;
				int cIx=im1.retrieve(cx);
				
				cIx-= im1.retrieve(dx);
				cIx >>= 1;
				int cy=px + Py(j + wj+1) * WIDTH, dy=px + Py(j + wj-1)*WIDTH;
			
				int cIy =im1.retrieve(cy);
				cIy-= im1.retrieve(dy);
				cIy >>=1;

				G[0] += cIx * cIx;
				G[1] += cIx * cIy;
				G[2] += cIx * cIy;
				G[3] += cIy * cIy;
				b_k[0] += deltaIk * cIx;
				b_k[1] += deltaIk * cIy;
			}
		}

		get_matrix_inv(G, G_inv);

		float fx = 0.0f, fy = 0.0f;
		fx = G_inv[0] * b_k[0] + G_inv[1] * b_k[1];
		fy = G_inv[2] * b_k[0] + G_inv[3] * b_k[1];

		out.continue_write(2*(j*WIDTH+i),fx);
		out.continue_write(2*(j*WIDTH+i)+1,fy);
#if 0
		unsigned char result[3];
		computeColor(fx * FLOW_SCALING_FACTOR, fy * FLOW_SCALING_FACTOR, result);

		out[3 * (j*WIDTH+i)] = result[0];
		out[3 * (j*WIDTH+i) + 1] = result[1];
		out[3 * (j*WIDTH+i) + 2] = result[2];

		if (fabs(fx*fy)>1.0)
		{
			fx*=coefx[i*WIDTH+j];
			fy*=coefy[i*WIDTH+j];
			movx1[i*WIDTH+j]= fx;
			movy1[i*WIDTH+j]= fy;
		}
		else {
			movx1[i*WIDTH+j]= 0.0f;
			movy1[i*WIDTH+j]= 0.0f;
		}
#endif
	}
#ifdef CACHE
	printf("hit ratio of im1:%f\n",im1.getHitRatio());
	printf("hit ratio of im2:%f\n",im2.getHitRatio());
	printf("hit ratio of out:%f\n",out.getHitRatio());
#endif
}
