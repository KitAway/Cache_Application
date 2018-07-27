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

template<typename T, int MAX_SET_BITS, int BLOCK_BITS, int MAX_WAY_BITS, bool READ_ONLY=false, bool CW=false>
class Cache {

	static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
	static const int LINE_SIZE = 1 << BLOCK_BITS;
	static const int MAX_SET_WAYS = 1 << MAX_WAY_BITS;
	static const int DATA_BITS = sizeof(T) * 8;
	typedef ap_uint<DATA_BITS> LocalType;

public:
	typedef ap_uint<DATA_BITS*LINE_SIZE> DataType;
	typedef T OrigType;

private:
	T get(const ap_int<32> addr){
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		requests++;

		DataType dt;
		bool match = false;

		int index = 0;
		for(int j=0;j<MAX_SET_WAYS;j++){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
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
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;

		requests++;
		bool match = false;

		int index = 0;
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
			for(int j = 1; j< MAX_SET_WAYS; j++){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
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


	Cache(DataType * mem, const unsigned int set_bits, const unsigned int way_bits):ptr_mem(mem),SET_BITS(set_bits), WAY_BITS(way_bits), CACHE_SETS(1<<set_bits), SET_WAYS(1<<way_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
#pragma HLS ARRAY_PARTITION variable=LRU complete dim=1
		for(int i=0;i<MAX_CACHE_SETS;++i) {
			if(i>CACHE_SETS)
				break;
			for(int j=0;j<MAX_SET_WAYS;++j){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType data = array[set_i][index] >> (block * DATA_BITS);
		requests++;
		if(hit(addr))hits++;
		return *(T*)&data;
	}

	void modify(int addr, T data){
#pragma HLS INLINE
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType ldata = *(LocalType*)&data;
		array[set_i][index]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
		dirty[set_i][index] = true;
		requests++;
		if(hit(addr))hits++;
	}

	int getIndex(int addr){
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		bool match = false;
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		for(int i=0;i<MAX_CACHE_SETS;i++){
				if(i>=CACHE_SETS)
					break;
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(j>=SET_WAYS)
					break;
				if(dirty[i][j]) {
					ap_uint<32> paddr = tags[i][j];
					ptr_mem[paddr << SET_BITS | i]=array[i][j];
				}
			}
		}
	}
	void update(){
		for(int i=0;i<MAX_CACHE_SETS;i++){
				if(i>=CACHE_SETS)
					break;
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(j>=SET_WAYS)
					break;
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
	const unsigned int WAY_BITS;
	const unsigned int SET_WAYS;
	const unsigned int CACHE_SETS;
	const unsigned int SET_BITS;
	DataType array[MAX_CACHE_SETS][MAX_SET_WAYS];
	ap_uint<32 - BLOCK_BITS> tags[MAX_CACHE_SETS][MAX_SET_WAYS];
	int LRU[MAX_CACHE_SETS][MAX_SET_WAYS];
	bool valid[MAX_CACHE_SETS][MAX_SET_WAYS], dirty[MAX_CACHE_SETS][MAX_SET_WAYS];
};



template<typename T, int MAX_SET_BITS, int BLOCK_BITS, int MAX_WAY_BITS>
class Cache<T, MAX_SET_BITS, BLOCK_BITS, MAX_WAY_BITS, true, false> {

	static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
	static const int LINE_SIZE = 1 << BLOCK_BITS;
	static const int MAX_SET_WAYS = 1 << MAX_WAY_BITS;
	static const int DATA_BITS = sizeof(T) * 8;
	typedef ap_uint<DATA_BITS> LocalType;

public:
	typedef ap_uint<DATA_BITS*LINE_SIZE> DataType;
	typedef T OrigType;

private:
	T get(const ap_int<32> addr){
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		requests++;

		DataType dt;
		bool match = false;

		int index = 0;
		for(int j=0;j<MAX_SET_WAYS;j++){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
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
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
				if(LRU[set_i][j] < Min){
					Min = LRU[set_i][j];
					index = j;
				}
			}
			dt = ptr_mem[addr >> BLOCK_BITS];
			array[set_i][index] = dt;
		}

		LRU[set_i][index] = requests;
		tags[set_i][index] = tag;

		valid[set_i][index] = true;
		LocalType data = lm_data::GetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::get(dt, block);
		return *(T*)&data;
	}


	class inner {
	public:
		inner(Cache *cache, const ap_uint<32> addr):cache(cache),addr(addr) {
		}
		operator T() const{
#pragma HLS INLINE
			return cache->get(addr);
		}
	private:
		Cache *cache;
		const ap_uint<32> addr;
	};

public:


	Cache(DataType * mem, const unsigned int set_bits, const unsigned int way_bits):ptr_mem(mem),SET_BITS(set_bits), WAY_BITS(way_bits), CACHE_SETS(1<<set_bits), SET_WAYS(1<<way_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
#pragma HLS ARRAY_PARTITION variable=LRU complete dim=1
		for(int i=0;i<MAX_CACHE_SETS;++i) {
			if(i>CACHE_SETS)
				break;
			for(int j=0;j<MAX_SET_WAYS;++j){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
				valid[i][j] = false;
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
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType data = array[set_i][index] >> (block * DATA_BITS);
		requests++;
		if(hit(addr))hits++;
		return *(T*)&data;
	}

	int getIndex(int addr){
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		bool match = false;
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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

public:

	long long int requests, hits;
	DataType * const ptr_mem;
	const unsigned int WAY_BITS;
	const unsigned int SET_WAYS;
	const unsigned int CACHE_SETS;
	const unsigned int SET_BITS;
	DataType array[MAX_CACHE_SETS][MAX_SET_WAYS];
	ap_uint<32 - BLOCK_BITS> tags[MAX_CACHE_SETS][MAX_SET_WAYS];
	int LRU[MAX_CACHE_SETS][MAX_SET_WAYS];
	bool valid[MAX_CACHE_SETS][MAX_SET_WAYS];
};


template<typename T, int MAX_SET_BITS, int BLOCK_BITS, int MAX_WAY_BITS>
class Cache<T, MAX_SET_BITS, BLOCK_BITS, MAX_WAY_BITS, false, true> {

	static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
	static const int LINE_SIZE = 1 << BLOCK_BITS;
	static const int MAX_SET_WAYS = 1 << MAX_WAY_BITS;
	static const int DATA_BITS = sizeof(T) * 8;
	typedef ap_uint<DATA_BITS> LocalType;

public:
	typedef ap_uint<DATA_BITS*LINE_SIZE> DataType;
	typedef T OrigType;

private:

	void set(const ap_int<32> addr, const T& data){
#pragma HLS INLINE
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;

		requests++;
		bool match = false;

		int index = 0;
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
			for(int j = 1; j< MAX_SET_WAYS; j++){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
				if(LRU[set_i][j] < Min){
					Min = LRU[set_i][j];
					index = j;
				}
			}
			if(dirty[set_i][index]) {
				ap_uint<32> paddr = tags[set_i][index];
				ptr_mem[paddr << SET_BITS | set_i] = array[set_i][index];
			}
			array[set_i][index] = 0;//ptr_mem[addr >> BLOCK_BITS];
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
		void operator= (T data){
#pragma HLS INLINE
			cache->set(addr, data);
		}
	private:
		Cache *cache;
		const ap_uint<32> addr;
	};

public:


	Cache(DataType * mem, const unsigned int set_bits, const unsigned int way_bits):ptr_mem(mem),SET_BITS(set_bits), WAY_BITS(way_bits), CACHE_SETS(1<<set_bits), SET_WAYS(1<<way_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
#pragma HLS ARRAY_PARTITION variable=LRU complete dim=1
		for(int i=0;i<MAX_CACHE_SETS;++i) {
			if(i>CACHE_SETS)
				break;
			for(int j=0;j<MAX_SET_WAYS;++j){
#pragma HLS UNROLL
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType data = array[set_i][index] >> (block * DATA_BITS);
		requests++;
		if(hit(addr))hits++;
		return *(T*)&data;
	}

	void modify(int addr, T data){
#pragma HLS INLINE
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		const ap_uint<BLOCK_BITS> block = addr;
		int index = getIndex(addr);
		LocalType ldata = *(LocalType*)&data;
		array[set_i][index]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
		dirty[set_i][index] = true;
		requests++;
		if(hit(addr))hits++;
	}

	int getIndex(int addr){
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
		const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
		bool match = false;
		for(int j=0;j<MAX_SET_WAYS;j++){
				if(j>=SET_WAYS)
					break;
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
		for(int i=0;i<MAX_CACHE_SETS;i++){
				if(i>=CACHE_SETS)
					break;
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(j>=SET_WAYS)
					break;
				if(dirty[i][j]) {
					ap_uint<32> paddr = tags[i][j];
					ptr_mem[paddr << SET_BITS | i]=array[i][j];
				}
			}
		}
	}
	void update(){
		for(int i=0;i<MAX_CACHE_SETS;i++){
				if(i>=CACHE_SETS)
					break;
			for(int j=0;j<SET_WAYS;j++){
#pragma HLS PIPELINE
				if(j>=SET_WAYS)
					break;
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
	const unsigned int WAY_BITS;
	const unsigned int SET_WAYS;
	const unsigned int CACHE_SETS;
	const unsigned int SET_BITS;
	DataType array[MAX_CACHE_SETS][MAX_SET_WAYS];
	ap_uint<32 - BLOCK_BITS> tags[MAX_CACHE_SETS][MAX_SET_WAYS];
	int LRU[MAX_CACHE_SETS][MAX_SET_WAYS];
	bool valid[MAX_CACHE_SETS][MAX_SET_WAYS], dirty[MAX_CACHE_SETS][MAX_SET_WAYS];
};

template<typename T, int MAX_SET_BITS, int BLOCK_BITS, int MAX_WAY_BITS>
class Cache<T, MAX_SET_BITS, BLOCK_BITS, MAX_WAY_BITS, true, true>;


}
