/*
  Input: k listHashingFile L B

  1. Load the listHashingFile generated by assignAll.c. For each k-mer x,
     the loaded hash value is a list of pairs (c, d) where c is 
     (the index of) a center and edit(x, c)=d. The list is sorted by d,
     break ties with c.
     Store the hash as a list of centers where each c is repeated PROD/d times.
     Here PROD is the product of all d's in the list. This way, a center
     is picked with probability proportional to the reciprocal of d.
     To save space, this list is compressed as a list of pairs (c, i) where
     i is the largest index of c in the original list. 

  2. For d = 1 .. k/2
     i) generata N pairs of k-mers with edit distance d
     ii) for a pair (s, t), generate vectors sig_s and sig_t of length L 
         such that vx[i] = a random center from the hashing list of x
     iii) divide sig_s and sig_t each into B equal-sized parts, say s and t
          has a collision if they share at least one part.
 
  ************************
  The above does not work well comparing to the simple method that say s and
  t has a collision if they share a center.

  By: Ke@PSU
  Last edited: 01/18/2022
*/

#include "util.h"
#include <time.h>
#include <string.h>

#define N 100000

typedef struct{
    size_t center;
    int last_idx;
}Center;

typedef struct{
    int len;
    int total;
    Center* centers;
}HashList;

//find the smallest last_idx that is >= roll, return the corresponding center
size_t binarySearch(Center* list, int len, int roll){
    int st = 0;
    int ed = len;
    int mid, r;

    while(st < ed){
	mid = (st+ed)/2;
	r = list[mid].last_idx - roll;
	if(r == 0) return list[mid].center;
	else if(r > 0) ed = mid;
	else st = mid + 1;
    }

    return list[st].center;
}

//each center is assigned with prob proportional to PROD/dist
void distributeHash(HashList* h, size_t n){
    int i, j;
    int size;
    Center* list;
    int prod;
    
    for(i=0; i<n; i+=1){
	size = h[i].len;
	list = h[i].centers;
	prod = 1;
	if(size == 1){//is a center
	    list[0].last_idx = 0;
	    h[i].total = 1;
	}else{
	    for(j=0; j<size; j+=1){
		prod *= list[j].last_idx; //last_idx is just dist at this point
	    }
	    h[i].total = 0;
	    for(j=0; j<size; j+=1){
		h[i].total += prod/list[j].last_idx;
		list[j].last_idx = h[i].total - 1;
	    }
	}
    }
}

void readHashFromFile(char* hash_file, int k, HashList* h, size_t n){
    FILE* fin = fopen(hash_file, "r");

    char* line = NULL;
    size_t len;

    size_t i, j;
    int size;
    
    
    for(i=0; i<n; i+=1){
	getdelim(&line, &len, ' ', fin); //kmer
	getdelim(&line, &len, ' ', fin); //ct
	size = atoi(line);
	h[i].len = size;
	h[i].centers = malloc_harder(sizeof(Center) * size);

	for(j=0; j<size-1; j+=1){
	    getdelim(&line, &len, ' ', fin); //center_idx
	    h[i].centers[j].center = atol(line);
	    getdelim(&line, &len, ' ', fin); //dist
	    h[i].centers[j].last_idx = atoi(line);
	}
	getdelim(&line, &len, ' ', fin); //center_idx
	h[i].centers[j].center = atol(line);
	getdelim(&line, &len, '\n', fin); //dist
	h[i].centers[j].last_idx = atoi(line);
    }
    
    fclose(fin);
    free(line);

    //distributeHash(h, n); //calculate probability of each center
}

void calcSig(kmer s, int k, size_t* sig_s, int sig_len, HashList* h){
    int i, roll;
    for(i=0; i<sig_len; i+=1){
	roll = rand()%h[s].total;
	sig_s[i] = binarySearch(h[s].centers, h[s].len, roll);
    }
}

int hasCollisionSizeT(size_t* sig_s, size_t* sig_t, int len, int num_bands){
    int i, j;
    int found;
    int size_band = len/num_bands;
    for(i=0; i<len; i+=size_band){
	found = 1;
	for(j=0; j<size_band; j+=1){
	    if(sig_s[i+j] != sig_t[i+j]){
		found = 0;
		break;
	    }
	}
	if(found) break;
    }
    return found;
}

int hasCollisionInt(int* sig_s, int* sig_t, int len){
    int i, j;
    for(i=0; i<len && sig_s[i]>=0; i+=1){
	for(j=0; j<len && sig_t[j]>=0; j+=1){
	    if(sig_s[i] == sig_t[j]){
		return 1;
	    }
	}
    }
    return 0;
}

int shareCenter(kmer s, kmer t, HashList* h){
    int i, j;
    int ls = h[s].len;
    Center* cs = h[s].centers;
    int lt = h[t].len;
    Center* ct = h[t].centers;

    for(i=0; i<ls; i+=1){
	for(j=0; j<lt; j+=1){
	    if(cs[i].center == ct[j].center){
		return 1;
	    }
	}
    }

    return 0;
}

//sig_s[i] = 1 if the i-th random k-mer share a center with s
void calcSig2(kmer s, int k, int* sig_s, int len,
	      HashList* h, kmer* rand_shots, int num_shots){
    int i, j = 0;
    for(i=0; i<num_shots; i+=1){
	if(shareCenter(s, rand_shots[i], h)){
	    sig_s[j] = i;
	    j += 1;
	    if(j >= len) break;
	}
    }
    for(; j<len; j+=1){
	sig_s[j] = -1;
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
	printf("usage: centerListLSH.out k listHashingFile\n");
	return 1;
    }

    int k = atoi(argv[1]);
    char* hash_file = argv[2];
    //int L = atoi(argv[3]);
    //int B = atoi(argv[4]);

    size_t NUM_KMERS = 1<<(k<<1);
    HashList* h = malloc_harder(sizeof *h *NUM_KMERS);

    readHashFromFile(hash_file, k, h, NUM_KMERS);

    srand(time(0));
    
    size_t i, j;
    kmer s, t;
    //size_t sig_s[L];
    //size_t sig_t[L];
    int collision_ct, share_center;

    printf("dist #col col%% #sha sha%%\n");
    for(i=1; i<=(k>>1)+1; i+=1){
	collision_ct = 0;
	share_center = 0;
	for(j=0; j<N; j+=1){
	    s = randomKMer(k);
	    t = randomEdit(s, k, i);

	    /*
	    calcSig(s, k, sig_s, L, h);
	    calcSig(t, k, sig_t, L, h);
	    if(hasCollisionSizeT(sig_s, sig_t, L, B)) collision_ct += 1;
	    */
	    
	    if(shareCenter(s, t, h)){
		share_center += 1;
	    }
	}
	printf("%zu %d %.2f%% %d %.2f%%\n",
	       i, collision_ct, collision_ct*100.0/N,
	       share_center, share_center*100.0/N);
    }
    

    for(i=0; i<NUM_KMERS; i+=1){
	free(h[i].centers);
    }
    free(h);
    
    return 0;
}
