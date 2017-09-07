#pragma once
#include "dataOpera.h"
/*
 * Set Associative Cache in this implementation can be used in mix mode, write and read can be
 * done to the same cache
 *
 */
namespace CACHE_ASSOC{
#include "ap_int.h"
/*
 * Template Cache implementation with template data type, set_i bits and block bits.
 */

template<typename T, int SET_BITS, int BLOCK_BITS, int WAY_BITS>
class Cache {

	static const int CACHE_SETS = 1 << SET_BITS;
	static const int LINE_SIZE = 1 << BLOCK_BITS;
	static const int SET_WAYS = 1 << WAY_BITS;
	static const int DATA_BITS = sizeof(T) * 8;
	typedef ap_uint<DATA_BITS> LocalType;

public:
	typedef ap_uint<DATA_BITS*LINE_SIZE> DataType;
	typedef T OrigType;

private:
	T get(const ap_int<32> addr){
#pragma HLS INLINE
		const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;
		requests++;

		DataType dt;
		bool match = false;

		int index = 0;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[set_i][j] == tag);
			match = bTag  && valid[set_i][j] ;
			if(match){
				hits++;
				dt = array[set_i][j];
				index = j;
				break;
			}
		}

		if(!match) {
			int Min = LRU[set_i][0];
			for(int j = 1; j< SET_WAYS; j++){
				if(LRU[set_i][j] < Min){
					Min = LRU[set_i][j];
					index = j;
				}
			}
			if(dirty[set_i][index]) {
				ap_uint<32> paddr = tags[set_i][index];
				ptr_mem[paddr << SET_BITS | set_i] = array[set_i][index];
			}
			dirty[set_i][index] = false;
			dt = ptr_mem[addr >> BLOCK_BITS];
			array[set_i][index] = dt;
		}

		LRU[set_i][index] = requests;
		tags[set_i][index] = tag;

		valid[set_i][index] = true;
		LocalType data = lm_data::GetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::get(dt, block);
		return *(T*)&data;
	}

	void set(const ap_int<32> addr, const T& data){
#pragma HLS INLINE
		const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;

		requests++;
		bool match = false;

		int index = 0;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[set_i][j] == tag);
			match = bTag  && valid[set_i][j] ;
			if(match){
				hits++;
				index = j;
				break;
			}
		}

		if(!match) {
			int Min = LRU[set_i][0];
			for(int j = 1; j< SET_WAYS; j++){
				if(LRU[set_i][j] < Min){
					Min = LRU[set_i][j];
					index = j;
				}
			}
			if(dirty[set_i][index]) {
				ap_uint<32> paddr = tags[set_i][index];
				ptr_mem[paddr << SET_BITS | set_i] = array[set_i][index];
			}
			array[set_i][index] = ptr_mem[addr >> BLOCK_BITS];
		}

		LocalType ldata = *(LocalType*)&data;
		array[set_i][index] = lm_data::SetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::set(array[set_i][index], ldata, block);

		LRU[set_i][index] = requests;
		tags[set_i][index] = tag;
		valid[set_i][index] = true;
		dirty[set_i][index] = true;

	}

	class inner {
	public:
		inner(Cache *cache, const ap_uint<32> addr):cache(cache),addr(addr) {
		}
		operator T() const{
#pragma HLS INLINE
			return cache->get(addr);
		}
		void operator= (T data){
#pragma HLS INLINE
			cache->set(addr, data);
		}
	private:
		Cache *cache;
		const ap_uint<32> addr;
	};

public:


	Cache(DataType * mem):ptr_mem(mem){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
#pragma HLS ARRAY_PARTITION variable=LRU complete dim=1
		for(int i=0;i<CACHE_SETS;++i) {
			for(int j=0;j<SET_WAYS;++j){
#pragma HLS UNROLL
				valid[i][j] = false;
				dirty[i][j] = false;
				LRU[i][j] = 0;
			}
		}
		requests = 0;
		hits = 0;
	}

	double getHitRatio() {
		return hits/(requests * 1.0);
	}

	T retrieve(int addr){
#pragma HLS INLINE
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType data = array[set_i][index] >> (block * DATA_BITS);
		requests++;
		if(hit(addr))hits++;
		return *(T*)&data;
	}

	void modify(int addr, T data){
#pragma HLS INLINE
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType ldata = *(LocalType*)&data;
		array[set_i][index]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
		dirty[set_i][index] = true;
		requests++;
		if(hit(addr))hits++;
	}

	int getIndex(int addr){
		const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[set_i][j] == tag);
			bool match = bTag  && valid[set_i][j] ;
			if(match){
				return j;
			}
		}
		return 0;
	}
	bool hit(int addr) {
#pragma HLS INLINE
		const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
		bool match = false;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[set_i][j] == tag);
			match = bTag  && valid[set_i][j] ;
			if(match){
				break;
			}
		}
		return match;
	}



	inner operator[](const ap_uint<32> addr) {
#pragma HLS INLINE
		return inner(this, addr);
	}

	~Cache(){
		for(int i=0;i<CACHE_SETS;i++){
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(dirty[i][j]) {
					ap_uint<32> paddr = tags[i][j];
					ptr_mem[paddr << SET_BITS | i]=array[i][j];
				}
			}
		}
	}
	void update(){
		for(int i=0;i<CACHE_SETS;i++){
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(dirty[i][j]) {
					ap_uint<32> paddr = tags[i][j];
					ptr_mem[paddr << SET_BITS | i]=array[i][j];
					dirty[i][j] = false;
				}
			}
		}
	}
public:

	long long int requests, hits;
	DataType * const ptr_mem;
	DataType array[CACHE_SETS][SET_WAYS];
	ap_uint<32 - SET_BITS - BLOCK_BITS> tags[CACHE_SETS][SET_WAYS];
	int LRU[CACHE_SETS][SET_WAYS];
	bool valid[CACHE_SETS][SET_WAYS], dirty[CACHE_SETS][SET_WAYS];
};


template<typename T, int BLOCK_BITS, int WAY_BITS>
class Cache<T, 0, BLOCK_BITS, WAY_BITS> {

	static const int LINE_SIZE = 1 << BLOCK_BITS;
	static const int SET_WAYS = 1 << WAY_BITS;
	static const int DATA_BITS = sizeof(T) * 8;
	typedef ap_uint<DATA_BITS> LocalType;

public:
	typedef ap_uint<DATA_BITS*LINE_SIZE> DataType;
	typedef T OrigType;

private:
	T get(const ap_int<32> addr){
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;
		requests++;

		DataType dt;
		bool match = false;

		int index = 0;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[j] == tag);
			match = bTag  && valid[j] ;
			if(match){
				hits++;
				dt = array[j];
				index = j;
				break;
			}
		}

		if(!match) {
			int Min = LRU[0];
			for(int j = 1; j< SET_WAYS; j++){
				if(LRU[j] < Min){
					Min = LRU[j];
					index = j;
				}
			}
			if(dirty[index]) {
				ap_uint<32> paddr = tags[index];
				ptr_mem[paddr] = array[index];
			}
			dirty[index] = false;
			dt = ptr_mem[addr >> BLOCK_BITS];
			array[index] = dt;
		}

		LRU[index] = requests;
		tags[index] = tag;

		valid[index] = true;
		LocalType data = lm_data::GetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::get(dt, block);
		return *(T*)&data;
	}

	void set(const ap_int<32> addr, const T& data){
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (BLOCK_BITS);
		const ap_uint<BLOCK_BITS> block = addr;

		requests++;
		bool match = false;

		int index = 0;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[j] == tag);
			match = bTag  && valid[j] ;
			if(match){
				hits++;
				index = j;
				break;
			}
		}

		if(!match) {
			int Min = LRU[0];
			for(int j = 1; j< SET_WAYS; j++){
				if(LRU[j] < Min){
					Min = LRU[j];
					index = j;
				}
			}
			if(dirty[index]) {
				ap_uint<32> paddr = tags[index];
				ptr_mem[paddr] = array[index];
			}
			array[index] = ptr_mem[addr >> BLOCK_BITS];
		}

		LocalType ldata = *(LocalType*)&data;
		array[index] = lm_data::SetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::set(array[index], ldata, block);

		LRU[index] = requests;
		tags[index] = tag;
		valid[index] = true;
		dirty[index] = true;

	}

	class inner {
	public:
		inner(Cache *cache, const ap_uint<32> addr):cache(cache),addr(addr) {
		}
		operator T() const{
#pragma HLS INLINE
			return cache->get(addr);
		}
		void operator= (T data){
#pragma HLS INLINE
			cache->set(addr, data);
		}
	private:
		Cache *cache;
		const ap_uint<32> addr;
	};

public:


	Cache(DataType * mem):ptr_mem(mem){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=0
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=0
#pragma HLS ARRAY_PARTITION variable=valid complete dim=0
#pragma HLS ARRAY_PARTITION variable=array complete dim=0
#pragma HLS ARRAY_PARTITION variable=LRU complete dim=0

		for(int j=0;j<SET_WAYS;++j){
#pragma HLS UNROLL
			valid[j] = false;
			dirty[j] = false;
			LRU[j] = 0;
		}
		requests = 0;
		hits = 0;
	}

	double getHitRatio() {
		return hits/(requests * 1.0);
	}

	T retrieve(int addr){
#pragma HLS INLINE
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType data = array[index] >> (block * DATA_BITS);
		requests++;
		if(hit(addr))hits++;
		return *(T*)&data;
	}

	void modify(int addr, T data){
#pragma HLS INLINE
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType ldata = *(LocalType*)&data;
		array[index]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
		dirty[index] = true;
		requests++;
		if(hit(addr))hits++;
	}

	int getIndex(int addr){
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (BLOCK_BITS);
		int index = 0;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[j] == tag);
			bool match = bTag  && valid[j] ;
			if(match){
				index = j;
				break;
			}
		}
		return index;
	}
	bool hit(int addr) {
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (BLOCK_BITS);
		bool match = false;
		for(int j=0;j<SET_WAYS;j++){
			bool bTag = (tags[j] == tag);
			match = bTag  && valid[j] ;
			if(match){
				break;
			}
		}
		return match;
	}



	inner operator[](const ap_uint<32> addr) {
#pragma HLS INLINE
		return inner(this, addr);
	}

	~Cache(){
		for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
			if(dirty[j]) {
				ap_uint<32> paddr = tags[j];
				ptr_mem[paddr]=array[j];
			}
		}
	}
	void update(){
		for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
			if(dirty[j]) {
				ap_uint<32> paddr = tags[j];
				ptr_mem[paddr]=array[j];
				dirty[j] = false;
			}
		}
	}
public:

	long long int requests, hits;
	DataType * const ptr_mem;
	DataType array[SET_WAYS];
	ap_uint<32 - BLOCK_BITS> tags[SET_WAYS];
	int LRU[SET_WAYS];
	bool valid[SET_WAYS], dirty[SET_WAYS];
};
}
