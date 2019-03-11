
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <cmath>
#include <utility> 
#include <limits>
#include <cstdlib>
#include <bf.h>
#include <random>


#define CLK_T signed long long int
#define COUNT_T unsigned long long int
#define ADDR_T unsigned long long int
#define NUM_ADDR_BITS 48
//#define GET_SET(x, y, z)  (((x << (NUM_ADDR_BITS - y)) & 0x03ffffffffff) >> (NUM_ADDR_BITS - y+ z))
#define GET_SET(x, y, z)  ((x >> z) & ((1UL << y) - 1))
#define GET_TAG(x, y)  x >> y
#define HIT true
#define MISS false
#define LOCAL_PART 0
#define REMOTE_PART 1
#define BOTH_PARTS 2
#define NO_PARTS -1
#define MRU_INSERT 0
#define LRU_INSERT 1
#define REMOTE_LRU_INSERT 1
#define NO_REMOTE_LRU_INSERT 0
#define BIMODEL_MRU_LOW_PROB_INSERT 2
#define BIMODEL_LRU_LOW_PROB_INSERT 3
#define PROB 64
#define BIMODEL_LOW_PROB_INSERT(M, N) ((M + rand() / (RAND_MAX / (N -M + 1) + 1)) == N)
#define CHECK_HIT true
#define DONT_CHECK_HIT false
#define EAF_0 0
#define EAF_N 1
#define EAF_2N_1 2
#define EAF_2N_2 3
#define EAF_N_2 4
#define ALPHA 8
#define NUM_OF_BLOCKS_IN_CACHE (cacheSize/blockSize)
#define BITS_IN_BF (alpha * NUM_OF_BLOCKS_IN_CACHE)
#define HASH_FUNCS_IN_BF(x,y)  (ceil(((x)/(y)) * log(2.0)))

#define BLK_SIZE 64

#define START_PC_ZERO 0

#define MAX_INSTRUCTION_CNT 1
#define MAX_PRIVATE_COUNT 2
#define MAX_SHARED_COUNT 6
#define MIN_SHARED_COUNT 1

#define MIN_INST_NUM 10
#define MAX_INST_NUM 30
#define PRIVATE_PROB 16
#define REACCESS_PROB 16
#define MIN_SHARED_NUM 1
#define MAX_SHARED_NUM 20

#define SHARED_NUM 10000000ULL
#define SHARED_SEG_SIZE (MAX_SHARED_COUNT*SHARED_NUM +(2*MAX_SHARED_COUNT))
#define SHARED_OFFSET 0
#define SHARED_END (SHARED_OFFSET+(BLK_SIZE*SHARED_SEG_SIZE))
#define INST_SEG_SIZE 100000ULL
#define INST_OFFSET (SHARED_END+BLK_SIZE)
#define INST_END (INST_OFFSET+(BLK_SIZE*INST_SEG_SIZE))
#define PRIVATE_SEG_SIZE 10000ULL
#define PRIVATE_OFFSET (INST_END+BLK_SIZE)
#define PRIVATE_END (PRIVATE_OFFSET+(BLK_SIZE*PRIVATE_SEG_SIZE))
#define LOCAL_DUMP_SIZE (MAX_SHARED_COUNT*SHARED_NUM +(2*MAX_SHARED_COUNT))
#define LOCAL_DUMP_OFFSET (PRIVATE_END+BLK_SIZE)
using namespace std;

using namespace bf;

class Barrier {
	private:
		COUNT_T count;
		mutex mut; 
		condition_variable cond;
	public:
		Barrier() : count(0) {}
		Barrier(COUNT_T n) : count(n) {}
		void changeCount(COUNT_T n) {count = n;}
		void arrive() {
			std::unique_lock<std::mutex> lk(mut);
			--count;
			while (count) cond.wait(lk);
			cond.notify_all();
		}
};

class Cache {
	public:
		void cacheLock() {
			mtx.lock();
		}
		void cacheUnlock() {
			mtx.unlock();
		}
		Cache(const unsigned int cs, const unsigned int bs, const unsigned int now, const unsigned int li, const bool pr, const bool partition, const unsigned int eafParam, const unsigned int a, const bool remote_lru) : cclock(new CLK_T(time(NULL))), rd(), mt(rd()), dist(6400000640,pow(2,NUM_ADDR_BITS)) {
			l_m =0; l_h=0; r_h=0; r_m=0; i_h=0; i_m=0; p_h=0; p_m=0; lc_h=0; lc_m=0; ls_h=0; ls_m=0;
			
			remote_lru_insert = remote_lru;
			local_misses = 0; local_hits = 0; remote_misses = 0; remote_hits = 0; total_misses = 0; total_hits = 0;
			remoteWays = 0;
			localWays = 0;
			addToRemotePartition = 0;
			addToLocalPartition = 0;
			alpha = a;
			eaf = eafParam;
			partitioning = partition;
			cacheSize = cs; blockSize = bs; numOfWays = now; insertPolicy = li; promote = pr;
			numOfLines = cacheSize/blockSize;
			numOfSets = numOfLines/numOfWays;
			numOfBitsForSet = ceil(log2(numOfSets));
			numOfBitsForOffset = ceil(log2(blockSize));
			numOfBitsForIdxOffset = numOfBitsForOffset + ceil(log2(numOfSets));
			// cout << "CacheSize: " << cacheSize << endl;
			// cout << "CacheNumOfLines: " << numOfLines << endl;
			// cout << "CacheNumOfSet: " << numOfSets << endl;
		        // cout << "NumBitsForOffset: " << numOfBitsForOffset << endl;
		         cout << "NumBitsForSet: " << ceil(log2(numOfSets)) << endl;	
		         cout << "NumBitsForIdxOffset: " << numOfBitsForIdxOffset << endl;
                        // cout << "NumOfWays: " << 	numOfWays << endl;						    
			//~ cout << cacheSize << endl << blockSize << endl<< numOfWays << endl << insertPolicy << endl;
			for (unsigned int i = 0; i < numOfWays; i++) {
				if (numOfWays - i <= 2 && partitioning){
					ways.push_back(new Way(cclock, numOfSets, pr, numOfBitsForIdxOffset, numOfBitsForOffset, REMOTE_PART));
					remoteWays++;
				}

				else {
					ways.push_back(new Way(cclock, numOfSets, pr, numOfBitsForIdxOffset, numOfBitsForOffset, LOCAL_PART));
					localWays++;
				}
			}
			filter_local = nullptr;
			filter_remote = nullptr;
			filter_local_cnt = 0;
			filter_remote_cnt = 0;
			
			if (!partitioning) {
				if (eaf == EAF_N)
					filter_local = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF(BITS_IN_BF, NUM_OF_BLOCKS_IN_CACHE)), BITS_IN_BF);
				else
					eaf = EAF_0;
			}
			else {
				if (eaf == EAF_N) {
					filter_local = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF(BITS_IN_BF, NUM_OF_BLOCKS_IN_CACHE)), BITS_IN_BF);
					numOfItemsInfilter = NUM_OF_BLOCKS_IN_CACHE;
				}
				else if (eaf == EAF_2N_1) {
					filter_local = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF((2*BITS_IN_BF), (2*NUM_OF_BLOCKS_IN_CACHE))), (2*BITS_IN_BF));
					numOfItemsInfilter = 2*NUM_OF_BLOCKS_IN_CACHE;
				}
				else if (eaf == EAF_2N_2) {
					filter_local = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF(BITS_IN_BF, NUM_OF_BLOCKS_IN_CACHE)), BITS_IN_BF);
					filter_remote = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF(BITS_IN_BF, NUM_OF_BLOCKS_IN_CACHE)), BITS_IN_BF);
					numOfItemsInfilter = NUM_OF_BLOCKS_IN_CACHE;
				}
				else if (eaf == EAF_N_2) {
					filter_local = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF((BITS_IN_BF/2), (NUM_OF_BLOCKS_IN_CACHE/2))), (BITS_IN_BF/2));
					filter_remote = new basic_bloom_filter(make_hasher(HASH_FUNCS_IN_BF((BITS_IN_BF/2), (NUM_OF_BLOCKS_IN_CACHE/2))), (BITS_IN_BF/2));
					numOfItemsInfilter = NUM_OF_BLOCKS_IN_CACHE/2;
				}
				else
					eaf = EAF_0;
			}
		}
		ADDR_T getAddr() {
			return dist(mt);
		}
                bool localQueryAllCache(const ADDR_T address, const bool store) {
	  	                bool x = queryCache(address, LOCAL_PART, CHECK_HIT, BOTH_PARTS, store);
				if (x) {
					if (address >= LOCAL_DUMP_OFFSET)
						lc_h++;
					else if (address >= PRIVATE_OFFSET)
						p_h++;
					else if (address >= INST_OFFSET)
						i_h++;
					else
						ls_h++;
				}
				
				else {
					if (address >= LOCAL_DUMP_OFFSET)
						lc_m++;
					else if (address >= PRIVATE_OFFSET)
						p_m++;
					else if (address >= INST_OFFSET)
						i_m++;
					else
						ls_m++;
				}
				x ? l_h++ : l_m++;
				return x;
		}
		bool remoteQueryAllCache(const ADDR_T address) {
		  bool x = queryCache(address, REMOTE_PART, CHECK_HIT, BOTH_PARTS, false);
				x ? r_h++ : r_m++;
				return x;
		}
		bool localQueryLocalCache(const ADDR_T address) {
		  bool x = queryCache(address, LOCAL_PART, DONT_CHECK_HIT, LOCAL_PART, false);
				if (x) {
					l_h++;
					lc_h++;
				}
				else {
					l_m++;
					lc_m++;
				}
				return x;
		}
		COUNT_T get_total_misses() const {
			return total_misses;
		}
		COUNT_T get_total_hits() const {
			return total_hits;
		}
		void printReport() const {
			
			cout << "Total Hits:\t" << total_hits << endl;
			cout << "Total Misses:\t" << total_misses << endl;
			
			if (partitioning) {
				cout << "Local Partition Hits:\t" << local_hits << endl; 
				cout << "Local Partition Misses:\t" << local_misses << endl;
				cout << "Remote Partition Hits:\t" << remote_hits << endl; 
				cout << "Remote Partition Misses:\t" << remote_misses << endl;
				
				cout << "Local Partition Size Increases: "  << addToLocalPartition << endl;
				cout << "Remote Partition Size Increases: "  << addToRemotePartition << endl;
				COUNT_T localparts = 0;
				COUNT_T remoteparts = 0;
				for (unsigned int i = 0; i < numOfWays; i++) {
					localparts += ways[i]->partition == LOCAL_PART;
					remoteparts += ways[i]->partition == REMOTE_PART;
				}
				cout << "Number of Local Partitions:\t" << localparts << endl;
				cout << "Number of Remote Partitions:\t" << remoteparts << endl;
			}
			
			
			cout << endl << endl;
			cout << "Number of Remote Hits:\t" << r_h << endl;
			cout << "Number of Remote Misses:\t" << r_m << endl;
			
			cout << "Number of Local Hits:\t" << l_h << endl;
			cout << "Number of Local Misses:\t" << l_m << endl;
			
			cout << "Number of Instruction Hits:\t" << i_h << endl;
			cout << "Number of Instruction Misses:\t" << i_m << endl;
			
			cout << "Number of Private Data Hits:\t" << p_h << endl;
			cout << "Number of Private Data Misses:\t" << p_m << endl;
			
			
			cout << "Number of Local Copy Hits:\t" <<lc_h << endl;
			cout << "Number of Local Copy Misses:\t" << lc_m << endl;
			
			cout << "Number of Local Shared Data Hits:\t" <<ls_h << endl;
			cout << "Number of Local Shared Data Misses:\t" << ls_m << endl;
			cout << "----------------------------------------------------\n";
			
		}
		void repart() {
			if (partitioning) {
				int partitionToIncrease = whichPartitionToIncrease();
				if (partitionToIncrease == NO_PARTS) return;
				unsigned int wayToAdd;
				if (partitionToIncrease == LOCAL_PART)
					wayToAdd = whichWayToAdd(REMOTE_PART);
				if (partitionToIncrease == REMOTE_PART)
					wayToAdd = whichWayToAdd(LOCAL_PART);
				//~ clearWay(wayToAdd);
				ways[wayToAdd]->partition = partitionToIncrease;
			}
		}
	private:
		int whichPartitionToIncrease() {
			if (partitioning) {
				if (( local_part_needed_evictions > remote_part_needed_evictions) && remoteWays > 1) {
					local_part_needed_evictions = 0;
					remote_part_needed_evictions = 0;
					return LOCAL_PART;
				}
				else if (( local_part_needed_evictions < remote_part_needed_evictions) && localWays > 1) {
					local_part_needed_evictions = 0;
					remote_part_needed_evictions = 0;
					return REMOTE_PART;
				}
			}
			return NO_PARTS;
		}
		unsigned int whichWayToAdd(const unsigned int partitionToSearch) {
			unsigned int wayToReturn;
			if (partitionToSearch == LOCAL_PART) {
				for (unsigned int i = 0; i < numOfWays; i++) {
					if(ways[i]->partition == LOCAL_PART)
						wayToReturn = i;
					else
						break;
				}
				remoteWays++;
				localWays--;
				addToRemotePartition++;
			}
			else if (partitionToSearch == REMOTE_PART) {
				for (unsigned int i = numOfWays-1; i >= 0; i--) {
					if(ways[i]->partition == REMOTE_PART)
						wayToReturn = i;
					else
						break;
				}
				remoteWays--;
				localWays++;
				addToLocalPartition++;
			}
			//~ if (partitionToSearch == LOCAL_PART) {
				//~ wayToReturn = --localWays;
				//~ addToRemotePartition++;
				//~ remoteWays++;
			//~ }
			//~ if (partitionToSearch == REMOTE_PART) {
				//~ wayToReturn = numOfWays-(remoteWays--);
				//~ addToLocalPartition++;
				//~ localWays++;
			//~ }
			return wayToReturn;
		}
		//~ unsigned int whichWayToAddAVGTime(const unsigned int partitionToSearch) {
			//~ unsigned int wayToReturn;
			//~ CLK_T lowestAVGWayTime = numeric_limits<CLK_T>::max();
			//~ for (unsigned int i = 0; i < numOfWays; i++) {
				//~ if (ways[i]->partition == partitionToSearch) {
					//~ CLK_T timestamp = ways[i]->getAVGTime();
					//~ if (timestamp < lowestAVGWayTime) {
						//~ lowestAVGWayTime = timestamp;
						//~ wayToReturn = i;
					//~ }
				//~ }
			//~ }
			//~ return wayToReturn;
		//~ }
		//~ void clearWay(unsigned wayToClear) {
			//~ for (unsigned int i = 0; i < numOfSets; i++) {
				//~ if (ways[wayToClear]->blocks[i] != nullptr) {
					//~ if (eaf != EAF_0) {
						//~ if (eaf == EAF_N || eaf == EAF_2N_1){
							//~ filter_local->add(ways[wayToClear]->blocks[i]->addr);
							//~ filter_local_cnt++;
							//~ if ((eaf == EAF_N && filter_local_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_2N_1 && filter_local_cnt == 2*NUM_OF_BLOCKS_IN_CACHE)) {
								//~ filter_local->clear();
								//~ filter_local_cnt = 0;
							//~ }
						//~ }
						//~ else {
							//~ if (ways[wayToClear]->partition == LOCAL_PART) {
								//~ filter_local->add(ways[wayToClear]->blocks[i]->addr);
								//~ filter_local_cnt++;
								//~ if ((eaf == EAF_2N_2 && filter_local_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_N_2 && filter_local_cnt == (NUM_OF_BLOCKS_IN_CACHE/2))) {
									//~ filter_local->clear();
									//~ filter_local_cnt = 0;
								//~ }
							//~ }
							//~ else if (ways[wayToClear]->partition == REMOTE_PART) {
								//~ filter_remote->add(ways[wayToClear]->blocks[i]->addr);
								//~ filter_remote_cnt++;
								//~ if ((eaf == EAF_2N_2 && filter_remote_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_N_2 && filter_remote_cnt == (NUM_OF_BLOCKS_IN_CACHE/2))) {
									//~ filter_remote->clear();
									//~ filter_remote_cnt = 0;
								//~ }
							//~ }
						//~ }
						
					//~ }
					//~ delete ways[wayToClear]->blocks[i];
					//~ ways[wayToClear]->blocks[i] = nullptr;
				//~ }
			//~ }
		//~ }
                bool queryCache(const ADDR_T address, const bool partitionToInsertTo, const bool checkForHit, const unsigned int partitionToCheck, const bool store) {
			(*cclock)++;
			//~ pair< signed long int, pair<time_t, bool>> returnWay;
			//~ returnWay.first = -1;
			//~ returnWay.second = make_pair(numeric_limits<time_t>::max(), false);
			signed long int wayToReplaceIfFree = -1; 
			unsigned int wayToReplaceIfNotFree;
			unsigned int wayToInsert;
			//~ time_t timeOfWayToReplace = ways[0]->blocks[GET_SET(address, numOfBitsForIdxOffset, numOfBitsForOffset)]->lastUseTimeStamp;

			CLK_T LRUtime =(*cclock);
			//cout << "TAG: " << (GET_TAG(address, numOfBitsForIdxOffset)) << "\t";
			for (unsigned int i = 0 ; i < numOfWays; i++) {

 			        pair<CLK_T, pair<bool, bool>> wayQueryRes = ways[i]->queryWay(address, store);
				if ((partitionToCheck == BOTH_PARTS || partitionToCheck == wayQueryRes.second.second || !partitioning) && wayQueryRes.second.first && checkForHit)  {
					
					//~ cout << address << "=> HIT;" << " Partition: " << (ways[i]->partition ? "remote" : "local")  << " Way: " << i << "; Set: "<<GET_SET(address, numOfBitsForIdxOffset, numOfBitsForOffset)<<"; time: " << ways[i]->blocks[GET_SET(address, numOfBitsForIdxOffset, numOfBitsForOffset)]-> lastUseTimeStamp <<endl;
					total_hits++;
					if (!partitioning || ways[i]->partition == LOCAL_PART) local_hits++;
					else if (ways[i]->partition == REMOTE_PART) remote_hits++;
					
					//cout << "WAY: " << i << endl;
					
					return HIT;
				}
				if ((wayQueryRes.second.second == partitionToInsertTo || !partitioning) &&
					wayQueryRes.first == numeric_limits<CLK_T>::min() && wayToReplaceIfFree == -1) {
					wayToReplaceIfFree = i;
				}
				
				else {
					if ((wayQueryRes.second.second == partitionToInsertTo || !partitioning) &&
						wayQueryRes.first != numeric_limits<CLK_T>::min() && wayQueryRes.first < LRUtime) {
						LRUtime = wayQueryRes.first;
						//~ cout << wayQueryRes.first << endl;
						wayToReplaceIfNotFree = i;
					}
						
				}
			}
			
			//insertion
			wayToInsert =  (wayToReplaceIfFree != -1) ? wayToReplaceIfFree : wayToReplaceIfNotFree;
			CLK_T timeToInsertWith;
			//if using EAF and address in EAF
			if (eaf != EAF_0) {
				//insert using EAF policy
				bool inEAF = (((eaf == EAF_2N_2) || (eaf == EAF_N_2)) ? filter_remote->lookup(address) : false) || filter_local->lookup(address);
				timeToInsertWith = inEAF ? (*cclock) : (BIMODEL_LOW_PROB_INSERT(0,(PROB-1)) ? (*cclock) : LRUtime-1);
			}
				
			//else inesrt using regular policy
			else {
				switch (insertPolicy) {
					case LRU_INSERT:
						timeToInsertWith = LRUtime-1;
						break;
					case MRU_INSERT:
						if(partitionToInsertTo == REMOTE_PART && remote_lru_insert)
							timeToInsertWith = LRUtime-1;
						else
							timeToInsertWith = (*cclock);
						break;
					case BIMODEL_MRU_LOW_PROB_INSERT:
						timeToInsertWith = BIMODEL_LOW_PROB_INSERT(0,(PROB-1)) ? (*cclock) : LRUtime-1;
						break;
					case BIMODEL_LRU_LOW_PROB_INSERT:
						timeToInsertWith = BIMODEL_LOW_PROB_INSERT(0,(PROB-1)) ? LRUtime-1 : (*cclock);
						break;
					default:
						timeToInsertWith = (*cclock);
						break;
				}
			}
				
				
			//eviction
			if (eaf != EAF_0 && ways[wayToInsert]->blocks[GET_SET(address, numOfBitsForSet, numOfBitsForOffset)] != nullptr) {
				ADDR_T evictedAddr = ways[wayToInsert]->blocks[GET_SET(address, numOfBitsForSet, numOfBitsForOffset)]->addr;
				if (eaf == EAF_N || eaf == EAF_2N_1) {
					if(filter_local->lookup(evictedAddr)) {
						if (ways[wayToInsert]->partition == LOCAL_PART)
							local_part_needed_evictions++;
						if (ways[wayToInsert]->partition == REMOTE_PART)
							remote_part_needed_evictions++;
					}
					else {
						filter_local->add(evictedAddr);
						filter_local_cnt++;
						
					}
					if ((eaf == EAF_N && filter_local_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_2N_1 && filter_local_cnt == 2*NUM_OF_BLOCKS_IN_CACHE)) {
						filter_local->clear();
						
						repart();
						filter_local_cnt = 0;
					}
				}
				else {
					if (partitionToInsertTo == LOCAL_PART) {
						if(filter_local->lookup(evictedAddr))
							local_part_needed_evictions++;
						if(filter_remote->lookup(evictedAddr))
							remote_part_needed_evictions++;
						if (!(filter_local->lookup(evictedAddr))) {
							filter_local->add(evictedAddr);
							filter_local_cnt++;
							
						}
						if ((eaf == EAF_2N_2 && filter_local_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_N_2 && filter_local_cnt == (NUM_OF_BLOCKS_IN_CACHE/2))) {
							filter_local->clear();
							
							repart();
							filter_local_cnt = 0;
						}
					}
					else if (partitionToInsertTo == REMOTE_PART) {
						if(filter_remote->lookup(evictedAddr))
							remote_part_needed_evictions++;
						if(filter_local->lookup(evictedAddr))
							local_part_needed_evictions++;
						if (!(filter_remote->lookup(evictedAddr))) {
							filter_remote->add(evictedAddr);
							filter_remote_cnt++;
							
						}
						if ((eaf == EAF_2N_2 && filter_remote_cnt == NUM_OF_BLOCKS_IN_CACHE) || (eaf == EAF_N_2 && filter_remote_cnt == (NUM_OF_BLOCKS_IN_CACHE/2))) {
							filter_remote->clear();
							
							repart();
							filter_remote_cnt = 0;
						}
					}
				}
			}
			
			ways[wayToInsert]->insertIntoWay(address, timeToInsertWith, store);
			//~ char buff[20];
			//~ strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&timeToInsertWith ));
			//~ cout << "\t time: "<< buff << endl;
			//~ cout << address << "=> MISS;" << " Partition: " << (ways[wayToInsert]->partition ? "remote" : "local")  << " Way: " << wayToInsert << "; Set: "<<GET_SET(address, numOfBitsForIdxOffset, numOfBitsForOffset)<<"; time: " << ways[wayToInsert]->blocks[GET_SET(address, numOfBitsForIdxOffset, numOfBitsForOffset)]-> lastUseTimeStamp <<endl;
			total_misses++;
			if (!partitioning || partitionToInsertTo == LOCAL_PART) local_misses++;
			else if (partitionToInsertTo == REMOTE_PART) remote_misses++;
			return MISS;				
				
		}
		struct Way{
			Way(CLK_T* cacheClock_s, const unsigned int setNum, const bool pr, const unsigned int nobfio, const unsigned int nobfo, const bool part) 
				: cacheClock(cacheClock_s), numOfSets(setNum), promote(pr),  numOfBitsForIdxOffset(nobfio), numOfBitsForOffset(nobfo), partition(part) {
			  numOfBitsForSet = ceil(log2(numOfSets));
				for (unsigned int i = 0; i < numOfSets; i++)
					blocks.push_back(nullptr);
			}
 		        pair<CLK_T, pair<bool, bool>> queryWay(const ADDR_T address, const bool store) {
				ADDR_T setNum = GET_SET(address, numOfBitsForSet, numOfBitsForOffset);
				ADDR_T tag = GET_TAG(address, numOfBitsForIdxOffset);
				//cout << setNum << endl;
				//~ cout << "setnum: " << setNum << " tag: " << tag << " bits for idx+offset: " << numOfBitsForIdxOffset  << " bits for offset: " << numOfBitsForOffset << endl;
				if (blocks[setNum] == nullptr ||
					!(blocks[setNum]->valid))
					return make_pair(numeric_limits<CLK_T>::min(), make_pair(false, partition));
				else if	(blocks[setNum]->tag != tag)
					return make_pair(blocks[setNum]->lastUseTimeStamp, make_pair(false, partition));
				else if (blocks[setNum]->valid && blocks[setNum]->tag == tag) {
					if (promote) blocks[setNum]->lastUseTimeStamp = (*cacheClock);
					blocks[setNum]->dirty = store;
					return make_pair(blocks[setNum]->lastUseTimeStamp, make_pair(true, partition));
				}
				else
					return make_pair(numeric_limits<CLK_T>::min(), make_pair(false, partition));
					
			}
                        void insertIntoWay(const ADDR_T address, const CLK_T timeToInsertWith, const bool store) {
				ADDR_T setNum = GET_SET(address, numOfBitsForSet, numOfBitsForOffset);
				ADDR_T tag = GET_TAG(address, numOfBitsForIdxOffset);
				if (blocks[setNum] != nullptr) {
				  if (blocks[setNum]->dirty) cerr << "W\t" << blocks[setNum]->addr << endl;
					delete blocks[setNum];
				}
				blocks[setNum] = new Block(address, tag, timeToInsertWith, store);
			}
			CLK_T getAVGTime() {
				CLK_T sum = 0;
				for (unsigned int i = 0; i < numOfSets; i++) {
					if (blocks[i] != nullptr)
						sum += blocks[i]->lastUseTimeStamp;
				}
				return sum/numOfSets;
			}
			struct Block {
			        Block(ADDR_T address, ADDR_T tagIn, CLK_T timeToInsertWith, bool store) :
			               addr(address), tag(tagIn), lastUseTimeStamp(timeToInsertWith), dirty(store) {
						valid = true;
					}
				ADDR_T addr;
				ADDR_T tag;
				COUNT_T lastUseTimeStamp;
				bool valid;
			        bool dirty;
			};
		  unsigned int numOfSets, numOfBitsForIdxOffset, numOfBitsForOffset, numOfBitsForSet;
			vector<Block*> blocks;
			CLK_T* cacheClock;
			bool promote;
			bool partition;
			
		};
		bool promote, partitioning, remote_lru_insert;
  unsigned long long int cacheSize, blockSize, numOfWays, numOfLines, numOfSets, numOfBitsForIdxOffset, numOfBitsForOffset, insertPolicy, eaf, alpha, remoteWays, localWays, addToLocalPartition, addToRemotePartition, numOfBitsForSet;
		basic_bloom_filter *filter_local, *filter_remote;
		unsigned long long int filter_local_cnt, filter_remote_cnt,local_part_needed_evictions, remote_part_needed_evictions, numOfItemsInfilter;
		vector<Way*> ways;
		CLK_T* cclock;
		mutex mtx;
		COUNT_T local_misses, local_hits, remote_misses, remote_hits, total_misses, total_hits;
		random_device rd;
		mt19937 mt;
		uniform_int_distribution<ADDR_T> dist;
		
		COUNT_T l_m, l_h, r_h, r_m, i_h, i_m, p_h, p_m, lc_h, lc_m, ls_h, ls_m;
		
};
mutex printMtx;
vector<Cache*> caches;
string folder;
Barrier b,c;
void nodeThread(COUNT_T myID) {
	//~ ifstream configFile(folder+"config"+to_string(myID));
	//~ unsigned int cacheSize, blockSize, numOfWays, insertPolicy, eaf, alpha; bool promote, partition;
	//~ configFile >> cacheSize >> blockSize >> numOfWays >> insertPolicy >> promote >> partition >> eaf >> alpha;
	//~ caches[myID] = new Cache(cacheSize, blockSize, numOfWays, insertPolicy, promote, partition, eaf, alpha);
	//~ b.arrive();
	//~ configFile.close();
	//~ ifstream traceFile(folder+"trace"+to_string(myID));
	//~ ADDR_T addr;
	//~ COUNT_T node;
	//~ while (traceFile >> addr >> node) {
		//~ printMtx.lock();
		//~ cout << myID << "\t" << addr << "\t" << node <<endl;
		//~ printMtx.unlock();
		//~ if (node == myID) {
			//~ caches[myID]->cacheLock();
			//~ caches[myID]->localQueryAllCache(addr);
			//~ caches[myID]->cacheUnlock();
		//~ }
		//~ else {
			//~ if (node < myID) {
				//~ caches[node]->cacheLock();
				//~ caches[myID]->cacheLock();
				//~ caches[node]->remoteQueryAllCache(addr);
				//~ caches[myID]->localQueryLocalCache((addr<<4)+node);
				//~ caches[myID]->cacheUnlock();
				//~ caches[node]->cacheUnlock();
				
			//~ }
			//~ else if (node > myID) {
				//~ caches[myID]->cacheLock();
				//~ caches[node]->cacheLock();
				//~ caches[node]->remoteQueryAllCache(addr);
				//~ caches[myID]->localQueryLocalCache((addr<<4)+node);
				//~ caches[node]->cacheUnlock();
				//~ caches[myID]->cacheUnlock();
				
			//~ }
		//~ }
	//~ }
	//~ traceFile.close();
	//~ c.arrive();
	//cout << addr1 << endl;
	//~ cout << folder+to_string(myID) << endl;
	//~ ifstream configFile(folder+"config"+to_string(myID));
		//~ unsigned int cacheSize, blockSize, numOfWays, insertPolicy, eaf, alpha; bool promote, partition;
		//~ configFile >> cacheSize >> blockSize >> numOfWays >> insertPolicy >> promote >> partition >> eaf >> alpha;
		//~ caches[myID] = new Cache(cacheSize, blockSize, numOfWays, insertPolicy, promote, partition, eaf, alpha);
		//~ b.arrive();
		//~ configFile.close();
	
	//~ ifstream traceFile(folder+"trace"+to_string(myID));
		//~ ADDR_T addr;
		//~ COUNT_T node;
		//~ while (traceFile >> addr >> node) {
			//~ printMtx.lock();
			//~ cout << myID << "\t" << addr << "\t" << node <<endl;
			//~ printMtx.unlock();
			//~ if (node == myID) {
				//~ caches[myID]->cacheLock();
				//~ caches[myID]->localQueryAllCache(addr);
				//~ caches[myID]->cacheUnlock();
			//~ }
			//~ else {
				//~ if (node < myID) {
					//~ caches[node]->cacheLock();
					//~ caches[myID]->cacheLock();
					//~ caches[node]->remoteQueryAllCache(addr);
					//~ caches[myID]->localQueryLocalCache((addr<<4)+node);
					//~ caches[myID]->cacheUnlock();
					//~ caches[node]->cacheUnlock();
					
				//~ }
				//~ else if (node > myID) {
					//~ caches[myID]->cacheLock();
					//~ caches[node]->cacheLock();
					//~ caches[node]->remoteQueryAllCache(addr);
					//~ caches[myID]->localQueryLocalCache((addr<<4)+node);
					//~ caches[node]->cacheUnlock();
					//~ caches[myID]->cacheUnlock();
					
				//~ }
			//~ }
		//~ }
		//~ traceFile.close();
		//~ c.arrive();
		ifstream configFile(folder+"config"+to_string(myID));
	unsigned int cacheSize, blockSize, numOfWays, insertPolicy, eaf, alpha; bool promote, partition, remoteLRUInsert;
	configFile >> cacheSize >> blockSize >> numOfWays >> insertPolicy >> remoteLRUInsert >> promote >> partition >> eaf >> alpha;
	caches[myID] = new Cache(cacheSize, blockSize, numOfWays, insertPolicy, promote, partition, eaf, alpha, remoteLRUInsert);
	b.arrive();
	configFile.close();
	ifstream traceFile(folder+"trace"+to_string(myID));
	ADDR_T addr;
	COUNT_T node;
	while (traceFile >> addr >> node) {
		if (node == myID) {
			caches[myID]->cacheLock();
			caches[myID]->localQueryAllCache(addr, false);
			caches[myID]->cacheUnlock();
		}
		else {
			if (node < myID) {
				caches[node]->cacheLock();
				caches[myID]->cacheLock();
				caches[node]->remoteQueryAllCache(addr);
				caches[myID]->localQueryLocalCache((addr<<4)+node);
				caches[myID]->cacheUnlock();
				caches[node]->cacheUnlock();
				
			}
			else if (node > myID) {
				caches[myID]->cacheLock();
				caches[node]->cacheLock();
				caches[node]->remoteQueryAllCache(addr);
				caches[myID]->localQueryLocalCache((addr<<4)+node);
				caches[node]->cacheUnlock();
				caches[myID]->cacheUnlock();
				
			}
		}
	}
		traceFile.close();
		c.arrive();
}

int main(int argc, char** argv) {
	if (argc < 3){
		cerr << "Invalid num of args!" << endl;
		//~ exit(1);
	}
	srand (time(NULL));
	
	COUNT_T numOfNodes = 1 ;
	istringstream ss(argv[1]);
	// if (!(ss >> numOfNodes))
	// 	cerr << "Invalid number " << argv[1] << '\n';
	// //~ cout << numOfNodes << endl;
	// folder = string(argv[2]) + "/";
	// //~ cout << folder << endl;
	// caches.resize(numOfNodes);
	caches.resize(1);
	//string configFN = string(argv[2]);
	//string traceFN = string(argv[3]);

	ifstream configFile(argv[1]);
	unsigned int cacheSize, blockSize, numOfWays, insertPolicy, eaf, alpha; bool promote, partition, remoteLRUInsert;
	configFile >> cacheSize >> blockSize >> numOfWays >> insertPolicy >> remoteLRUInsert >> promote >> partition >> eaf >> alpha;
	configFile.close();
	caches[0] = new Cache(cacheSize, blockSize, numOfWays, insertPolicy, promote, partition, eaf, alpha, remoteLRUInsert);

        ifstream trace(argv[2], ios::in);
	
	// vector<ifstream*> traces(numOfNodes);
	// for (int i = 0; i < numOfNodes; i++){
	// 	ifstream configFile(folder+"config"+to_string(i));
	// 	unsigned int cacheSize, blockSize, numOfWays, insertPolicy, eaf, alpha; bool promote, partition, remoteLRUInsert;
	// 	configFile >> cacheSize >> blockSize >> numOfWays >> insertPolicy >> remoteLRUInsert >> promote >> partition >> eaf >> alpha;
	// 	caches[i] = new Cache(cacheSize, blockSize, numOfWays, insertPolicy, promote, partition, eaf, alpha, remoteLRUInsert);
	// 	configFile.close();
	// 	//traces[i] = new ifstream(folder+"trace"+to_string(i), ios::binary | ios::in);
	// 	traces[i] = new ifstream(folder+"trace"+to_string(i),  ios::in);
	// }
	
	ADDR_T addr;
	unsigned char ld_st_md;
	int i = 0;
	while (trace >> ld_st_md >> hex  >> addr) {
	
					
	  bool store = false;
	  store = (ld_st_md == 'M') || (ld_st_md == 'S');
	  //cerr << addr << "STORE: " << store << endl;
	  bool hit = caches[i]->localQueryAllCache(addr, store);
	  if (!hit) cerr << "R\t"  <<  addr << endl;
					
	}
	trace.close();
}
