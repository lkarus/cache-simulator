#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define BYTES_PER_WORD 4

/***************************************************************/
/*                                                             */
/* Procedure : cdump                                           */
/*                                                             */
/* Purpose   : Dump cache configuration                        */   
/*                                                             */
/***************************************************************/
void cdump(int capacity, int assoc, int blocksize){

	printf("Cache Configuration:\n");
    	printf("-------------------------------------\n");
	printf("Capacity: %dB\n", capacity);
	printf("Associativity: %dway\n", assoc);
	printf("Block Size: %dB\n", blocksize);
	printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : sdump                                           */
/*                                                             */
/* Purpose   : Dump cache stat		                       */   
/*                                                             */
/***************************************************************/
void sdump(int total_reads, int total_writes, int write_backs,
	int reads_hits, int write_hits, int reads_misses, int write_misses) {
	printf("Cache Stat:\n");
    	printf("-------------------------------------\n");
	printf("Total reads: %d\n", total_reads);
	printf("Total writes: %d\n", total_writes);
	printf("Write-backs: %d\n", write_backs);
	printf("Read hits: %d\n", reads_hits);
	printf("Write hits: %d\n", write_hits);
	printf("Read misses: %d\n", reads_misses);
	printf("Write misses: %d\n", write_misses);
	printf("\n");
}


/***************************************************************/
/*                                                             */
/* Procedure : xdump                                           */
/*                                                             */
/* Purpose   : Dump current cache state                        */ 
/* 							       */
/* Cache Design						       */
/*  							       */
/* 	    cache[set][assoc][word per block]		       */
/*      						       */
/*      						       */
/*       ----------------------------------------	       */
/*       I        I  way0  I  way1  I  way2  I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set0  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set1  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*      						       */
/*                                                             */
/***************************************************************/
void xdump(int set, int way, uint32_t** cache)
{
	int i,j,k = 0;

	printf("Cache Content:\n");
    	printf("-------------------------------------\n");
	for(i = 0; i < way;i++)
	{
		if(i == 0)
		{
			printf("    ");
		}
		printf("      WAY[%d]",i);
	}
	printf("\n");

	for(i = 0 ; i < set;i++)
	{
		printf("SET[%d]:   ",i);
		for(j = 0; j < way;j++)
		{
			if(k != 0 && j == 0)
			{
				printf("          ");
			}
			printf("0x%08x  ", cache[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

/********************************************************/
/*														*/
/*	Procedure	: lru_max								*/
/*														*/
/*	Purpose		: return the least recently used way	*/
/*														*/
/********************************************************/

long lru_max(long* lru, int way) {
	int result = 0;
	int max, i;

	for (i = 0; i < way; i++) {
		if (max < lru[i]) {
			max = lru[i];
			result = i;
		}
	}

	return result;
}

int main(int argc, char *argv[]) {                              

	uint32_t** cache;
	long** lru;
	short** dirty_bit;
	int i, j, k;	
	int capacity;
	int way;
	int blocksize;
	int activate_dump_cache = 0;
	char* partial;
	
	if(argc < 2){
		printf("Error: usage: %s [-c cap:assoc:bsize] [-x] input_trace\n", argv[0]);
		exit(1);
	}

	int count = 1;
	int state_bool = 0;

	while(count != argc - 1){
		if (strcmp(argv[count], "-c") == 0){
			state_bool = 1;
			partial = argv[count+1];
			
			strtok(partial, ":");
			capacity = atoi(partial);
			
			partial = strtok(NULL, ":");
			way = atoi(partial);

			partial = strtok(NULL, "\0");
			blocksize = atoi(partial);
		}

		if (strcmp(argv[count], "-x") == 0){
			activate_dump_cache = 1;
		}

		count++;
	}

	if (state_bool == 0){	// missing configuration
		printf("Error: usage: configuration needed.\n");
		exit(1);
	}

	int set = capacity/way/blocksize;
	int words = blocksize / BYTES_PER_WORD;	

	// allocate
	cache = (uint32_t**) malloc (sizeof(uint32_t*) * set);
	dirty_bit = (short**) malloc (sizeof(short*) * set);
	lru = (long**) malloc (sizeof(long*) * set);

	for(i = 0; i < set; i++) {
		cache[i] = (uint32_t*) malloc(sizeof(uint32_t) * way);
		dirty_bit[i] = (short*) malloc(sizeof(short) * way);
		lru[i] = (long*) malloc(sizeof(long) * way);
	}

	for(i = 0; i < set; i++) {
		for(j = 0; j < way; j ++) {
			cache[i][j] = 0x0;
			lru[i][j] = -1;
			dirty_bit[i][j] = 0;
		}
	}

	FILE *program;
	char buffer[13];
	char* partial2;
	uint32_t addr;
	int total_read = 0;
	int total_write = 0;
	int write_back = 0;
	int read_hit = 0;
	int write_hit = 0;
	int read_miss = 0;
	int write_miss = 0;

	program = fopen(argv[argc-1], "r");
	if(program == NULL){
		printf("Cannot open the file %s\n", argv[argc-1]);
		exit(1);
	}

	while(fgets(buffer,13,program) != NULL){
		partial2 = buffer;
		strtok(partial2, " ");
		
		if(strcmp(partial2, "R") == 0){	// read cache
			total_read += 1;	// increment total read
			partial2 = strtok(NULL, "x");
			partial2 = strtok(NULL, "\0");
			addr = (uint32_t)strtol(partial2, NULL, 16);
			addr = addr / blocksize;
			addr = addr * blocksize;

			uint32_t addr_1;
			int des_set;	// desginated set pointed from address
			addr_1 = addr/blocksize;
			des_set = addr_1 % set;
			int temp = -1;	// the first empty way in a set (if exists)
			int des_way = -1;	// the way equal with addr (if exists)

			for (i = 0; i < way; i++) {
				if (lru[des_set][i] == -1) {	// empty
					if (temp == -1) {
						temp = i;	
					}
					continue;
				}
				else {	// not empty
					lru[des_set][i]++;
					if (des_way != -1) {	// read hit; all is done
						continue;
					}
					else {
						if (cache[des_set][i] == addr) {	// read hit
							read_hit++;
							lru[des_set][i] = 0;
							des_way = i;
						}
					}
				}
			}

			if (des_way == -1) {	// read miss
				read_miss++;

				if (temp == -1) {	// there does not exist an empty way
					temp = lru_max(lru[des_set], way);
				}

				if (dirty_bit[des_set][temp] == 1) {
					write_back++;
				}

				cache[des_set][temp] = addr;	// renew cache address
				lru[des_set][temp] = 0;			// reset least recently used value
				dirty_bit[des_set][temp] = 0;	// not dirty (just fetched from DATA MEMORY)
			}
		}
		else if (strcmp(partial2, "W") == 0){	// write to cache
			total_write += 1;	// increment total writes
			partial2 = strtok(NULL, "x");
			partial2 = strtok(NULL, "\0");
			addr = (uint32_t)strtol(partial2, NULL, 16);
			
			addr = addr / blocksize;
			addr = addr * blocksize;

			uint32_t addr_1;
			int des_set;	// desginated set pointed from address
			addr_1 = addr/blocksize;
			des_set = addr_1 % set;
			int temp = -1;	// the first empty way in a set (if exists)
			int des_way = -1;	// the way equal with addr (if exists)

			for (i = 0; i < way; i++) {
				if (lru[des_set][i] == -1) {	// empty
					if (temp == -1) {
						temp = i;
					}
					continue;
				}
				else {	// not empty
					lru[des_set][i]++;
					if (des_way != -1) {	// write hit; all is done
						continue;
					}
					else {
						if (cache[des_set][i] == addr) {	// write hit
							write_hit++;
							lru[des_set][i] = 0;
							dirty_bit[des_set][i] = 1;
							des_way = i;
						}
					}
				}
			}

			if (des_way == -1) {	// write miss
				write_miss++;

				if (temp == -1) {	// there does not exist an empty way
					temp = lru_max(lru[des_set], way);
				}

				if (dirty_bit[des_set][temp] == 1) {
					write_back++;
				}

				cache[des_set][temp] = addr;	// renew cache address
				lru[des_set][temp] = 0;			// reset lru value
				dirty_bit[des_set][temp] = 1;	// dirty (write new data to a address)
			}				
		} 
	}

	// test example
    cdump(capacity, way, blocksize);
    sdump(total_read, total_write, write_back, read_hit, write_hit, read_miss, write_miss); 
    if (activate_dump_cache == 1){
    	xdump(set, way, cache);
	}
    return 0;
}
