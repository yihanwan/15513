/* 
 * yihanwan
 */ 
#include"cachelab.h"
#include<getopt.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
int N=200;
int s,E,b;
char operator;
long long address;
int i,j,k;
int size;
int verbose = 0;
int thit = 0;
int tmiss=0;
int tevict=0;
int chit = 0;
int cmiss=0;
int cevict=0;
char* HIT = "hit";
char* MISS ="miss";
char* EVICT = "eviction";
long long tag_mask;
long long row_mask;
int ctag;
int crow;
int vic=0;
typedef struct			//use a 2-dimension array to simulate cache
{
        int valid;
        int tag;
        int last_update;
}Block;
Block** cache;

void scan_row(){
	int li=0;
	int max=0;
	int min = 0x7fffffff;
	//find max/min timestamp
	for (li=0;li<E;li++){
		if (cache[crow][li].valid==1 && max<cache[crow][li].last_update){
			max=cache[crow][li].last_update;
		}
		if (cache[crow][li].valid==1 && min>cache[crow][li].last_update){
			min=cache[crow][li].last_update;
			vic = li;
		}
	}
	//hit
	for (li=0;li<E;li++){
		if (ctag == cache[crow][li].tag && cache[crow][li].valid == 1){
			thit++;
			chit++;
			cache[crow][li].last_update=max+1;
			return;
		}
	}
	tmiss++;
	cmiss++;
	
	// find empty block
	for (li=0;li<E;li++){
		if (cache[crow][li].valid == 0){
			cache[crow][li].valid=1;
			cache[crow][li].last_update=max+1;
			cache[crow][li].tag = ctag;
			return;
		}
	}
	//replace
	cache[crow][vic].tag=ctag;
	cache[crow][vic].last_update=max+1;
	cevict++;
	tevict++;
	return;
}

void scan_cache(){
	ctag = (address>>(s+b))&tag_mask;
	crow = (address>>b)&row_mask;
	chit=0;
	cmiss=0;
	cevict=0;
	
	//printf("2");
	
	scan_row();
	if (operator == 'L'){
		
	}else if (operator == 'S'){
		
	}else{
		scan_row();
	}
}

int main(int argc, char* argv[]){
	char filepath[N];
	char line[N];
	int rows;
	int opt;
	FILE* file;
	// read parameters
	while((opt=getopt(argc,argv,"vs:E:b:t:")) != -1){   
        switch(opt){
			case 'v':
				verbose = 1;
				break;
			case 's':  
				s = atoi(optarg); 
				break;  
			case 'E':  
				E = atoi(optarg); 
				break;  
			case 'b':  
				b = atoi(optarg);
				break;  
			case 't':  
				strcpy(filepath, optarg);
				break;  
        }
    }
	// allocate cache
	rows = (1<<s);
	cache = (Block**)malloc(rows*sizeof(Block*));
	for (i=0;i<rows;i++){
		cache[i]=(Block*)malloc(E*sizeof(Block));
		for (j=0;j<E;j++){
			cache[i][j].valid = 0;
			cache[i][j].tag=0;
			cache[i][j].last_update=0;
		}
	}
	tag_mask=0x7fffffffffffffff;
	row_mask=(1<<s)-1;
	//printf("%llx\n",row_mask);
	// read file
	file = fopen(filepath, "r");
	while(fgets(line, N, file)){
		
		if (line[0] != ' '){
			continue;
		}
		//check each line 
		sscanf(line, " %c %llx,%d", &operator, &address, &size);
		//printf("%c %llx %d\n",operator,address,size);
		scan_cache();
		
		if (verbose){
			// print verbose
			printf("%c %llx,%d",operator,address,size);
			for (i=0;i<cmiss;i++){
				printf(" %s", MISS);
			}
			for (i=0;i<cevict;i++){
				printf(" %s", EVICT);
			}
			for (i=0;i<chit;i++){
				printf(" %s", HIT);
			}
			printf("\n");
		}
	}
	fclose(file);
	printSummary(thit,tmiss,tevict);
	free(cache);
	exit(0);
}
