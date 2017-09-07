#pragma once


#ifdef DEBUG
#include <iostream>
using std::cout;
using std::endl;
#endif // DEBUG


template<typename T, int ARRAY_SIZE_BITS, int BITS_LIMIT> class BitonicSortLocal{
	public:
		static const int ARRAY_SIZE = 1 << ARRAY_SIZE_BITS;
		static const int LOCAL_SIZE = 1 << BITS_LIMIT;
	public:

		BitonicSortLocal(T* array):array(array){
		}

		void sort(bool dir){
#pragma HLS INLINE
			int iter = (ARRAY_SIZE >> BITS_LIMIT);
			for(int id = 0; id < iter;id++){
				sortLocal(dir, id);
			}
#ifndef LOCAL
			for(int bits = BITS_LIMIT + 1; bits <= ARRAY_SIZE_BITS; bits ++){
				for(int stride = bits - 1; stride >= 0; stride--){
					for(int i=0; i< ARRAY_SIZE/2; i++){
#pragma HLS PIPELINE
						bool l_dir = ((i >> (bits - 1)) & 0x01);
						l_dir = l_dir ^ dir;
						int step = 1 << stride;
						int POS = i *2 - (i & (step - 1));
						/*
						T p0 = array[POS], p1 = array[POS + step];
						compare(p0, p1, dir);
						array[POS] = p0;
						array[POS + step] = p1;
						*/
						compare(array + POS, array + POS + step, l_dir);
					}
				}
			}
#else
			for(int bits = BITS_LIMIT + 1; bits <= ARRAY_SIZE_BITS; bits ++){
				for(int stride = bits - 1; stride >= BITS_LIMIT; stride--){
					for(int i=0; i< ARRAY_SIZE/2; i++){
#pragma HLS PIPELINE
						bool l_dir = ((i >> (bits - 1)) & 0x01);
						l_dir = l_dir ^ dir;
						int step = 1 << stride;
						int POS = i *2 - (i & (step - 1));
						/*
						T p0 = array[POS], p1 = array[POS + step];
						compare(p0, p1, dir);
						array[POS] = p0;
						array[POS + step] = p1;
						*/
						compare(array + POS, array + POS + step, l_dir);
					}
				}
				for(int id = 0; id < iter;id++){
					mergeLocal(bits, id, dir);
				}
			}
#endif
		}

	private:

		void sortLocal(bool dir, int id){
#pragma HLS INLINE
			T localArray[LOCAL_SIZE];
#pragma HLS ARRAY_PARTITION variable=localArray complete dim=1
			memcpy(localArray, array + id * LOCAL_SIZE, LOCAL_SIZE * sizeof(T));

			for(int bits = 1; bits <= BITS_LIMIT; bits ++){
				for(int stride = bits - 1; stride >= 0; stride--){
					for(int i=0; i< LOCAL_SIZE/2; i++){
//#pragma HLS UNROLL factor = 4
#pragma HLS PIPELINE
						bool l_dir = ((i >> (bits - 1)) & 0x01) ;
						l_dir = dir ^ (l_dir || ((id & 0x01) && bits == BITS_LIMIT));
						int step = 1 << stride;
						int POS = i *2 - (i & (step - 1));
						compare(localArray + POS, localArray + POS + step, l_dir);
					}
				}
			}
			memcpy(array + id * LOCAL_SIZE, localArray, LOCAL_SIZE * sizeof(T));
		}
		void mergeLocal(int bits, int id, bool dir){
#pragma HLS INLINE
			T localArray[LOCAL_SIZE];
#pragma HLS ARRAY_PARTITION variable=localArray complete dim=1
			memcpy(localArray, array + id * LOCAL_SIZE, LOCAL_SIZE * sizeof(T));

			for(int stride = BITS_LIMIT - 1; stride >=0;stride--){
				for(int i=0; i< LOCAL_SIZE/2; i++){
#pragma HLS PIPELINE
					int index = i + id * LOCAL_SIZE/2;
					bool l_dir = ((index >> (bits - 1)) & 0x01) ^ dir;
					int step = 1 << stride;
					int POS = i * 2 - (i & (step - 1));
					compare(localArray + POS, localArray + POS + step, l_dir);
				}
			}
			memcpy(array + id * LOCAL_SIZE, localArray, LOCAL_SIZE * sizeof(T));
		}
		void compare(T* pos0, T* pos1, bool dir){
#pragma HLS INLINE
			T p0 = *pos0, p1= *pos1;
			if((p0 > p1) != dir){
				*pos0 = p1;
				*pos1 = p0;
			}
		}
		bool compare(T &pos0, T &pos1, bool dir){
#pragma HLS INLINE
			if((pos0 > pos1) != dir){
				T tmp = pos0;
				pos0 = pos1;
				pos1 = tmp;
				return true;
			}
			return false;
		}

#ifdef DEBUG
		void print(){
			for(int i = 0;i<ARRAY_SIZE;i++){
				cout<<"array["<<i<<"]="<<array[i]<<'\t';
			}
			cout<<endl;
		}
#endif // DEBUG

		T* const array;

};




template<typename T, int ARRAY_SIZE_BITS> class BitonicSort{
	public:
		static const int ARRAY_SIZE = 1 << ARRAY_SIZE_BITS;
		BitonicSort(T* array):array(array){
		}


		void sort(bool dir){
#pragma HLS INLINE

			for(int bits = 1; bits <= ARRAY_SIZE_BITS; bits ++){
				for(int stride = bits - 1; stride >= 0; stride--){
					for(int i=0; i< ARRAY_SIZE/2; i++){
#pragma HLS PIPELINE
						bool l_dir = ((i >> (bits - 1)) & 0x01);
						l_dir = l_dir ^ dir;
						int step = 1 << stride;
						int POS = i *2 - (i & (step - 1));
						compare(array + POS, array + POS + step, l_dir);
					}
#ifdef DEBUG
					print();
#endif // DEBUG
				}

			}
		}
	private:
#ifdef DEBUG
		void print(){
			for(int i = 0;i<ARRAY_SIZE;i++){
				cout<<"array["<<i<<"]="<<array[i]<<'\t';
			}
			cout<<endl;
		}
#endif // DEBUG
		void compare(T* pos0, T* pos1, bool dir){
#pragma HLS INLINE
			T p0 = *pos0, p1= *pos1;
			if((p0 > p1) != dir){
				*pos0 = p1;
				*pos1 = p0;
			}
		}
		bool compare(T &pos0, T &pos1, bool dir){
#pragma HLS INLINE
			if((pos0 > pos1) != dir){
				T tmp = pos0;
				pos0 = pos1;
				pos1 = tmp;
				return true;
			}
			return false;
		}
		T* const array;
};

