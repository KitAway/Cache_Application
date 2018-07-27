#pragma once
/*
 * Cache in this implementation can be used in mix mode, write and read can be
 * done to the same cache
 *
 */
#include "dataOpera.h"
namespace CACHE{
#include "ap_int.h"

	/*
	 * Template Cache implementation with template data type, set_i bits and block bits.
	 */
// General direct-mapped cache, read-write cache
	template<typename T, int MAX_SET_BITS, int BLOCK_BITS, bool READ_ONLY=false, bool CW=false>
		class Cache {

			static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
			static const int LINE_SIZE = 1 <<BLOCK_BITS;
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
				bool match = tags[set_i] == tag;

				DataType dt;
				if(valid[set_i] && match) {
					hits++;
					dt = array[set_i];
				}
				else {
					if(dirty[set_i]) {
						ap_uint<32> paddr = tags[set_i];
						ptr_mem[paddr << SET_BITS | set_i] = array[set_i];
					}
					dirty[set_i] = false;
					dt = ptr_mem[addr >> BLOCK_BITS];
					array[set_i] = dt;
				}
				tags[set_i] = tag;

				valid[set_i] = true;
				LocalType data = lm_data::GetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::get(dt, block);
				return *(T*)&data;
			}

			void set(const ap_int<32> addr, const T& data){
#pragma HLS INLINE
				const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
				const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
				const ap_uint<BLOCK_BITS> block = addr;

				requests++;

				bool match = tags[set_i] == tag;
				if(valid[set_i] && match) {
					hits++;
				}
				else {
					if(dirty[set_i]) {
						ap_uint<32> paddr = tags[set_i];
						ptr_mem[paddr << SET_BITS | set_i] = array[set_i];
					}
					array[set_i] = ptr_mem[addr >> BLOCK_BITS];
				}

				LocalType ldata = *(LocalType*)&data;
				array[set_i] = lm_data::SetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::set(array[set_i], ldata, block);

				tags[set_i] = tag;
				valid[set_i] = true;
				dirty[set_i] = true;

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


			Cache(DataType * mem, const unsigned int set_bits):ptr_mem(mem),CACHE_SETS(1<<set_bits), SET_BITS(set_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
				for(int i=0;i<MAX_CACHE_SETS;++i) {
#pragma HLS UNROLL
					if(i>=CACHE_SETS)
						break;
					valid[i] = false;
					dirty[i] = false;
				}
				requests = 0;
				hits = 0;
			}

			double getHitRatio() {
				return hits/(requests * 1.0);
			}

			void modify(int addr, T data){
#pragma HLS INLINE
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;

				LocalType ldata = *(LocalType*)&data;
				array[set_i]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
				dirty[set_i] = true;
				requests++;
				if(hit(addr))hits++;
			}

			bool hit(int addr) {
#pragma HLS INLINE
				const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;
				bool match = tags[set_i] == tag;
				return (valid[set_i] && match);
			}

			T retrieve(int addr){
#pragma HLS INLINE
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;
				LocalType data = array[set_i] >> (block * DATA_BITS);
				requests++;
				if(hit(addr))hits++;
				return *(T*)&data;
			}

			inner operator[](const ap_uint<32> addr) {
#pragma HLS INLINE
				return inner(this, addr);
			}

			~Cache(){
				for(int i=0;i<CACHE_SETS;i++){
					//#pragma HLS PIPELINE
					if(dirty[i]) {
						ap_uint<32> paddr = tags[i];
						ptr_mem[paddr << SET_BITS | i]=array[i];
					}
				}
			}
			void update(){
				for(int i=0;i<CACHE_SETS;i++){
#pragma HLS PIPELINE
					if(dirty[i]) {
						ap_uint<32> paddr = tags[i];
						ptr_mem[paddr << SET_BITS | i]=array[i];
						dirty[i] = false;
					}
				}
			}
			void continue_write(const ap_int<32> addr, const T& data){
#pragma HLS INLINE
				const ap_uint<32 - BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
				const ap_uint<MAX_SET_BITS> set_i = (addr >> BLOCK_BITS)&((1<<SET_BITS)-1);
				const ap_uint<BLOCK_BITS> block = addr;

				requests++;

				bool match = tags[set_i] == tag;
				if(valid[set_i] && match) {
					hits++;
				}
				else {
					if(dirty[set_i]) {
						ap_uint<32> paddr = tags[set_i];
						ptr_mem[paddr << SET_BITS | set_i] = array[set_i];
					}
					array[set_i] = 0;//ptr_mem[addr >> BLOCK_BITS];
				}

				LocalType ldata = *(LocalType*)&data;
				array[set_i] = lm_data::SetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::set(array[set_i], ldata, block);

				tags[set_i] = tag;
				valid[set_i] = true;
				dirty[set_i] = true;

			}
			public:

			long long int requests, hits;
			const unsigned int CACHE_SETS;
			const unsigned int SET_BITS;
			DataType * const ptr_mem;
			DataType array[MAX_CACHE_SETS];
			ap_uint<32-BLOCK_BITS> tags[MAX_CACHE_SETS];
			bool valid[MAX_CACHE_SETS], dirty[MAX_CACHE_SETS];
		};


// Write-only direct-mapped cache and write to continues memory address
	template<typename T, int MAX_SET_BITS, int BLOCK_BITS>
		class Cache<T, MAX_SET_BITS, BLOCK_BITS, false, true> {

			static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
			static const int LINE_SIZE = 1 <<BLOCK_BITS;
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

				bool match = tags[set_i] == tag;
				if(valid[set_i] && match) {
					hits++;
				}
				else {
					if(dirty[set_i]) {
						ap_uint<32> paddr = tags[set_i];
						ptr_mem[paddr << SET_BITS | set_i] = array[set_i];
					}
					array[set_i] = 0;
				}

				LocalType ldata = *(LocalType*)&data;
				array[set_i] = lm_data::SetData<DATA_BITS, DATA_BITS * LINE_SIZE, BLOCK_BITS >::set(array[set_i], ldata, block);

				tags[set_i] = tag;
				valid[set_i] = true;
				dirty[set_i] = true;

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


			Cache(DataType * mem, const unsigned int set_bits):ptr_mem(mem),CACHE_SETS(1<<set_bits), SET_BITS(set_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=dirty complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
				for(int i=0;i<MAX_CACHE_SETS;++i) {
#pragma HLS UNROLL
					if(i>=CACHE_SETS)
						break;
					valid[i] = false;
					dirty[i] = false;
				}
				requests = 0;
				hits = 0;
			}

			double getHitRatio() {
				return hits/(requests * 1.0);
			}

			void modify(int addr, T data){
#pragma HLS INLINE
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;

				LocalType ldata = *(LocalType*)&data;
				array[set_i]( block * DATA_BITS + DATA_BITS - 1 , block * DATA_BITS) = ldata;
				dirty[set_i] = true;
				requests++;
				if(hit(addr))hits++;
			}

			bool hit(int addr) {
#pragma HLS INLINE
				const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;
				bool match = tags[set_i] == tag;
				return (valid[set_i] && match);
			}

			inner operator[](const ap_uint<32> addr) {
#pragma HLS INLINE
				return inner(this, addr);
			}

			~Cache(){
				for(int i=0;i<CACHE_SETS;i++){
//#pragma HLS PIPELINE
					if(dirty[i]) {
						ap_uint<32> paddr = tags[i];
						ptr_mem[paddr << SET_BITS | i]=array[i];
					}
				}
			}
			void update(){
				for(int i=0;i<CACHE_SETS;i++){
#pragma HLS PIPELINE
					if(dirty[i]) {
						ap_uint<32> paddr = tags[i];
						ptr_mem[paddr << SET_BITS | i]=array[i];
						dirty[i] = false;
					}
				}
			}
			public:

			long long int requests, hits;
			const unsigned int CACHE_SETS;
			const unsigned int SET_BITS;
			DataType * const ptr_mem;
			DataType array[MAX_CACHE_SETS];
			ap_uint<32-BLOCK_BITS> tags[MAX_CACHE_SETS];
			bool valid[MAX_CACHE_SETS], dirty[MAX_CACHE_SETS];
		};



// Read-only direct-mapped cache
	template<typename T, int MAX_SET_BITS, int BLOCK_BITS>
		class Cache<T, MAX_SET_BITS, BLOCK_BITS, true, false> {

			static const int MAX_CACHE_SETS = 1 << MAX_SET_BITS;
			static const int LINE_SIZE = 1 <<BLOCK_BITS;
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
				bool match = tags[set_i] == tag;

				DataType dt;
				if(valid[set_i] && match) {
					hits++;
					dt = array[set_i];
				}
				else {
					dt = ptr_mem[addr >> BLOCK_BITS];
					array[set_i] = dt;
				}
				tags[set_i] = tag;

				valid[set_i] = true;
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


			Cache(DataType * mem, const unsigned int set_bits):ptr_mem(mem),CACHE_SETS(1<<set_bits), SET_BITS(set_bits){
#pragma HLS ARRAY_PARTITION variable=tags complete dim=1
#pragma HLS ARRAY_PARTITION variable=valid complete dim=1
#pragma HLS ARRAY_PARTITION variable=array complete dim=1
				for(int i=0;i<MAX_CACHE_SETS;++i) {
#pragma HLS UNROLL
					if(i>=CACHE_SETS)
						break;
					valid[i] = false;
				}
				requests = 0;
				hits = 0;
			}

			double getHitRatio() {
				return hits/(requests * 1.0);
			}

			bool hit(int addr) {
#pragma HLS INLINE
				const ap_uint<32 - SET_BITS-BLOCK_BITS> tag = addr >> (SET_BITS+BLOCK_BITS);
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;
				bool match = tags[set_i] == tag;
				return (valid[set_i] && match);
			}

			T retrieve(int addr){
#pragma HLS INLINE
				const ap_uint<SET_BITS> set_i = (addr >> BLOCK_BITS);
				const ap_uint<BLOCK_BITS> block = addr;
				LocalType data = array[set_i] >> (block * DATA_BITS);
				requests++;
				if(hit(addr))hits++;
				return *(T*)&data;
			}

			inner operator[](const ap_uint<32> addr) {
#pragma HLS INLINE
				return inner(this, addr);
			}

			public:

			long long int requests, hits;
			const unsigned int CACHE_SETS;
			const unsigned int SET_BITS;
			DataType * const ptr_mem;
			DataType array[MAX_CACHE_SETS];
			ap_uint<32-BLOCK_BITS> tags[MAX_CACHE_SETS];
			bool valid[MAX_CACHE_SETS];
		};

	template<typename T, int MAX_SET_BITS, int BLOCK_BITS>
		class Cache<T, MAX_SET_BITS, BLOCK_BITS, true, true>;
}
