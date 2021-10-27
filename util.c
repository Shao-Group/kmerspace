#include "util.h"

void printIntArray(const int* x, const int len){
  int i;
  for(i=0; i<len; i+=1){
    printf("%2d ", x[i]);
  }
  printf("\n");
}

int editDist(const kmer s1, const kmer s2, const int k, const int max_id){
    return editDist2(s1, k, s2, k, max_id);
}

int editDist2(const kmer s1, const int k1, const kmer s2, const int k2, const int max_d){
    if(k1 > k2) return editDist2(s2, k2, s1, k1, max_d);
    int diag_index = k2 - k1;
    if(max_d >= 0 && diag_index >= max_d) return diag_index;
    
    int row[k2+1];
    int i, j, diag, cur,tmp;
    for(i=0; i<k2+1; i+=1){
	row[i] = i;
    }

    kmer s1_copy, s2_copy;
    for(i=1, s1_copy=s1; i<k1+1; i+=1, s1_copy>>=2){
	diag_index += 1;
	diag = row[0];
	row[0] = i;
	
	for(j=1, s2_copy=s2; j<k2+1; j+=1, s2_copy>>=2){
	    //substitution
	    cur = diag + ((s1_copy & 3) == (s2_copy & 3) ? 0 : 1);
	    //deletion
	    tmp = row[j] + 1;
	    cur = cur > tmp ? tmp : cur;
	    //insertion
	    tmp = row[j-1] +1;
	    cur = cur > tmp ? tmp : cur;
	    
	    diag = row[j];
	    row[j] = cur;
	}

	if(max_d >= 0 && row[diag_index] >= max_d){
	    break;
	}
    }

    return row[diag_index];
}

int editDist3(const char* s1, const int l1, const char* s2, const int l2, const int max_d){
    if(l1 > l2) return editDist3(s2, l2, s1, l1, max_d);
    int diag_index = l2 - l1;
    if(max_d >= 0 && diag_index >= max_d) return diag_index;
    
    int row[l2+1];
    int i, j, diag, cur,tmp;
    for(i=0; i<l2+1; i+=1){
	row[i] = i;
    }

    for(i=1; i<l1+1; i+=1){
	diag_index += 1;
	diag = row[0];
	row[0] = i;
	
	for(j=1; j<l2+1; j+=1){
	    //substitution
	    cur = diag + (s1[i-1] == s2[j-1] ? 0 : 1);
	    //deletion
	    tmp = row[j] + 1;
	    cur = cur > tmp ? tmp : cur;
	    //insertion
	    tmp = row[j-1] +1;
	    cur = cur > tmp ? tmp : cur;
	    
	    diag = row[j];
	    row[j] = cur;
	}

	if(max_d >= 0 && row[diag_index] >= max_d){
	    break;
	}
    }

    return row[diag_index];
}

kmer encode(const char* str, const int k){
    kmer enc = 0;
    int i, x;
    for(i=0; i<k; i+=1){
	switch(str[i]){
	case 'A': x=0; break;
	case 'C': x=1; break;
	case 'G': x=2; break;
	case 'T': x=3; break;
	}
	enc = (enc << 2)|x;
    }
    return enc;
}

char* decode(const kmer enc, const int k, char* str){
    if(str == NULL){
	str = malloc_harder(sizeof *str *k);
    }
    kmer enc_copy = enc;
    char base[] = {'A', 'C', 'G', 'T'};
    int i;
    for(i=k-1; i>=0; i-=1){
	str[i] = base[enc_copy & 3];
	enc_copy >>= 2;
    }
    return str;
}

kmer* readCentersFromFile(const char* filename, const int k, size_t* numOfCenters){
  FILE* fin = fopen(filename, "r");

  fscanf(fin, "%zu\n", numOfCenters);
  kmer* centers = malloc_harder(sizeof *centers *(*numOfCenters));

  int buffersize = k+2;//for \n and \0
  char kmer_str[buffersize];

  int i;
  for(i=0; i<*numOfCenters; i+=1){
      fgets(kmer_str, buffersize, fin);
      centers[i] = encode(kmer_str, k);
  }
  
  fclose(fin);
  return centers;
}

void readKMerHashFromFile(const char* filename, const int k, int* h){
    FILE* fin = fopen(filename, "r");

    char format[20];
    sprintf(format, "%%%ds %%d\n", k);
    char kmer_str[k+1]; //for \0
    int kmer_hash;
    while(fscanf(fin, format, kmer_str, &kmer_hash) != EOF){
	h[encode(kmer_str, k)] = kmer_hash;
    }

    fclose(fin);
}

void* malloc_harder(size_t size){
    void* ptr;
    while((ptr = malloc(size)) == NULL){
	sleep(30);
    }
    return ptr;
}
void* calloc_harder(size_t num, size_t size){
    void* ptr;
    while((ptr = calloc(num, size)) == NULL){
	sleep(30);
    }
    return ptr;
}
void* realloc_harder(void* ptr, size_t new_size){
    void* new_ptr;
    while((new_ptr = realloc(ptr, new_size)) == NULL){
	sleep(30);
    }
    return new_ptr;
}
