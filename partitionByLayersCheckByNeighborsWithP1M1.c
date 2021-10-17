/*
  ******************************************************************
  Different from the second version (partitionByLayersCheckByCenters.c),
  here we also include (k+1)-mers and (k-1)-mers that are found
  during BFS into the hashing function.
  ******************************************************************
  Given a list of centers c_1, c_2, ..., c_m in the k-mer space S, 
  we partition S into islands A_1, A_2, ..., A_m, and a (connected)
  gray area. This gives a (partial) hashing function that maps
  a k-mer s to a nonnegative integer: h(s) = k iff s is in the
  island generated by c_k; for s in the gray area, h(s) is not
  defined and is represented by h(s) = -1 in this program.

  We design h to be local sensitive, in the sense that it 
  satisfies the following two conditions for all k-mers s and t
  with h(s), h(t) >= 0:
  1. if dist(s, t) < p, then h(s) = h(t)
  2. if dist(s, t) > q, then h(s) != h(t)
  where dist(s, t) is the Levenshtein distance between s and t
  and p, q are two parameters.

  The first condition is satisfied by making sure the "local 
  width" of the gray area is at least p. 
  ******************************************************************
  Similar to the first version (partitionByLayers.c), before assigning
  h(s) = i, we check s against assigned x-mers t in N(c_i). If
  dist(s, t) < p, h(s) = -1.
  
  But instead of keeping a list of assigned x-mers for each center, 
  we generate all (p-1)-neighbors of s and check if any of them is
  already assigned a center other than c_i. If so, h(s) = -1.
  ******************************************************************
  The second condition is satisfied by limiting the diameter of 
  each island to be at most q.

  Following is the heuristic implemented in this program.
  Input: k, p, q, and a file containing centers c_1, ..., c_m
  (see util.h for more information on the format of the file)
  Output: the hashing function h represented by an array
  Alg:
  1. Initiate h(s) = -3 for all k-mers, indicating that they are 
  all unassigned.
  2. Assign h(c_k) = k for all the centers.
  3. For radius r = 1, 2, ..., q/2:
       For k = 1, 2, ..., m:
         Enumerate all k-mers s with dist(s, c_k) = r;
         For each s:
           If h(s) > -2, continue (s has been assigned)
	   Enumerate all (p-1)-neighbors x of s;
	   For each x:
	     If h(x) >=0 and h(x) != k, assign h(s) = -1 and break
	   If s remains unassigned, assign h(s) = k

  Note - values for h: 
  h(s) >= 0 assigned to island c_h(s)
  h(s) = -1 assigned to gray area
  h(s) = -2 bfs visited
  h(s) = -3 untouched
 
  By: Ke@PSU
  Last edited: 10/16/2021
*/

#include "util.h"
#include "ArrayList.h"
#include "HashTable.h"
#include <time.h>

typedef int bool;
#define TRUE 1
#define FALSE 0

//mask for (k-1)-mers
#define luMSB 0x8000000000000000lu
//mask for (k+1)-mers
#define luSMSB 0x4000000000000000lu

typedef struct{
    kmer center;
    //(k-1)-mers has their msb set to 1
    ArrayList* bfs_layer; //all the k- and (k-1)-mers to be examined by next step of bfs
} Island;

void IslandInit(Island* id, const kmer c){
    id->center = c;

    id->bfs_layer = malloc(sizeof *(id->bfs_layer));
    AListInit(id->bfs_layer);
    AListInsert(id->bfs_layer, c);
}

void IslandFree(Island* id){
    AListFree(id->bfs_layer);
    free(id->bfs_layer);
}

/*
  Given the current bfs layer, for each k-mer, perform one
  substitution, one deletion, or one insertion; for each (k-1)-mer, 
  perform one insertion; do nothing for (k+1)-mers.
  Check all results against the h (for k-mers) 
  h_m1 (for (k-1)-mers) and h_p1 (for (k+1)-mers) arrays. 
  If not visited (h{, _m1, _p1}==-3), add to new_layer 
  and update h{, _m1, _p1}.
 */
void getNextLayer(Island* id, const int k, int* h, int* h_m1, int* h_p1){
    if(id->bfs_layer->used == 0) return;

    ArrayList* new_layer = malloc(sizeof *new_layer);
    AListInit(new_layer);
    
    size_t i, j;
    kmer s, head, body, tail, x, m;
    for(i=0; i<id->bfs_layer->used; i+=1){
	s = id->bfs_layer->arr[i];
	//(k-1)-mer, no need to ^luMSB as the head will shift MSB out
	if(s>=luMSB){
	    //insertion
	    for(j=0; j<k; j+=1){
		head = (s>>(j<<1))<<((j+1)<<1);
		tail = ((1lu<<(j<<1))-1) & s;
		for(m=0; m<4; m+=1){
		    body = m<<(j<<1);
		    x = head|body|tail;
		    //skip if visited
		    if(h[x] != -3) continue;
		    else{
			AListInsert(new_layer, x);
			h[x] = -2;
		    }
		}
	    }
	}
	//k-mer
	else if(s<luSMSB){
	    //insertion
	    for(j=0; j<=k; j+=1){
		head = (s>>(j<<1))<<((j+1)<<1);
		tail = ((1lu<<(j<<1))-1) & s;
		for(m=0; m<4; m+=1){
		    body = m<<(j<<1);
		    x = head|body|tail;
		    //skip if visited
		    if(h_p1[x] != -3) continue;
		    else{
			AListInsert(new_layer, x|luSMSB);
			h_p1[x] = -2;
		    }
		}
	    }
	    //deletion
	    for(j=0; j<k; j+=1){
		head = (s>>((j+1)<<1))<<(j<<1);
		tail = ((1lu<<(j<<1))-1) & s;
		x = head|tail;
		//skip if visited
		if(h_m1[x] != -3) continue;
		else{
		    AListInsert(new_layer, x|luMSB);
		    h_m1[x] = -2;
		}
	    }
	    //substitution
	    for(j=1; j<=k; j+=1){
		head = (s>>(j<<1))<<(j<<1);
		tail = ((1lu<<((j-1)<<1))-1) & s;
		for(m=0; m<4; m+=1){
		    body = m<<((j-1)<<1);
		    x = head|body|tail;
		    //skip if visited
		    if(h[x] != -3) continue;
		    else{
			AListInsert(new_layer, x);
			h[x] = -2;
		    }
		}
	    }
	}
    }//end of for each in layer
    
    AListFree(id->bfs_layer);
    free(id->bfs_layer);
    id->bfs_layer = new_layer;
}

bool cleanAndReturnConflict(bool conflict, HashTable* visited,
			    ArrayList* cur_layer, ArrayList* next_layer){
    HTableFree(visited);
    AListFree(cur_layer);
    AListFree(next_layer);
    return conflict;
}
/*
  Generate all x-mer neighbors of s up to depth away, if any of them
  is assigned a center other than c, return TRUE; otherwise return
  FALSE.

  s is a ks-mer, if ks = k, do insertion, deletion, substitution;
  if ks = k-1, only do insertion for the first layer;
  if ks = k+1, only do deletion for the first layer.
*/
bool conflictWithNeighbors(kmer s, int k, int ks, int depth, size_t c,
			   int* h, int* h_m1, int * h_p1){
    HashTable visited;
    HTableInit(&visited);

    ArrayList cur_layer, next_layer;
    AListInit(&cur_layer);
    AListInit(&next_layer);

    if(k>ks) s|=luMSB; //(k-1)-mer
    else if(k<ks) s|=luSMSB; //(k+1)-mer
    HTableInsert(&visited, s);
    AListInsert(&cur_layer, s);

    size_t i, j;
    kmer head, body, tail, x, m;

    while(depth > 0){
	depth -= 1;
	for(i=0; i<cur_layer.used; i+=1){
	    s = cur_layer.arr[i];
	    //(k-1)-mer, no need to ^luMSB as the head will shift MSB out
	    if(s>=luMSB){
		//insertion
		for(j=0; j<k; j+=1){
		    head = (s>>(j<<1))<<((j+1)<<1);
		    tail = ((1lu<<(j<<1))-1) & s;
		    for(m=0; m<4; m+=1){
			body = m<<(j<<1);
			x = head|body|tail;
			//check conflict
			if(h[x] >= 0 && h[x] != c){
			    return  cleanAndReturnConflict(TRUE, &visited,
							   &cur_layer,
							   &next_layer);
			}
			//add to next_layer if not visited
			else if(!HTableSearch(&visited, x)){
			    AListInsert(&next_layer, x);
			    HTableInsert(&visited, x);
			}
		    }
		}
	    }
	    //(k+1)-mer
	    else if(s>=luSMSB){
		s^=luSMSB;
		//deletion
		for(j=0; j<=k; j+=1){
		    head = (s>>((j+1)<<1))<<(j<<1);
		    tail = ((1lu<<(j<<1))-1) & s;
		    x = head|tail;
		
		    //check conflict
		    if(h[x] >= 0 && h[x] != c){
			return  cleanAndReturnConflict(TRUE, &visited,
						       &cur_layer,
						       &next_layer);
		    }
		    //add to next_layer if not visited
		    else if(!HTableSearch(&visited, x)){
			AListInsert(&next_layer, x);
			HTableInsert(&visited, x);
		    }
		}
		
	    }
	    //k-mer
	    else{
		//insertion
		for(j=0; j<=k; j+=1){
		    head = (s>>(j<<1))<<((j+1)<<1);
		    tail = ((1lu<<(j<<1))-1) & s;
		    for(m=0; m<4; m+=1){
			body = m<<(j<<1);
			x = head|body|tail;
			//check conflict
			if(h_p1[x] >= 0 && h_p1[x] != c){
			    return  cleanAndReturnConflict(TRUE, &visited,
							   &cur_layer,
							   &next_layer);
			}
			x |= luSMSB;
			//add to next_layer if not visited
			if(!HTableSearch(&visited, x)){
			    AListInsert(&next_layer, x);
			    HTableInsert(&visited, x);
			}
		    }
		}
		//deletion
		for(j=0; j<k; j+=1){
		    head = (s>>((j+1)<<1))<<(j<<1);
		    tail = ((1lu<<(j<<1))-1) & s;
		    x = head|tail;
		    //check conflict
		    if(h_m1[x] >= 0 && h_m1[x] != c){
			return  cleanAndReturnConflict(TRUE, &visited,
						       &cur_layer,
						       &next_layer);
		    }
		    x |= luMSB;
		    //add to next_layer if not visited
		    if(!HTableSearch(&visited, x)){
			AListInsert(&next_layer, x);
			HTableInsert(&visited, x);
		    }
		    
		}
		//substitution
		for(j=1; j<=k; j+=1){
		    head = (s>>(j<<1))<<(j<<1);
		    tail = ((1lu<<((j-1)<<1))-1) & s;
		    for(m=0; m<4; m+=1){
			body = m<<((j-1)<<1);
			x = head|body|tail;
			//check conflict
			if(h[x] >= 0 && h[x] != c){
			    return  cleanAndReturnConflict(TRUE, &visited,
							   &cur_layer,
							   &next_layer);
			}
			//add to next_layer if not visited
			else if(!HTableSearch(&visited, x)){
			    AListInsert(&next_layer, x);
			    HTableInsert(&visited, x);
			}
		    }
		}
	    }//end k-mer
	}//end for each in cur_layer

	AListClear(&cur_layer);
	AListSwap(&cur_layer, &next_layer);
    }
    
    AListFree(&cur_layer);
    AListFree(&next_layer);
    HTableFree(&visited);
    return FALSE;
}

int main(int argc, char* argv[]){
    if(argc != 5){
	printf("usage: partitionByLayersCheckByCentersWithP1M1.out k p q centers_file\n");
	return 1;
    }

    int k = atoi(argv[1]);
    int p = atoi(argv[2]);
    int q = atoi(argv[3]);
    char* centers_file = argv[4];

    size_t NUM_KMERS = (1<<(k<<1));
    int* h = malloc(sizeof *h *NUM_KMERS);

    size_t i, j;
    for(i=0; i<NUM_KMERS; i+=1){
	h[i] = -3;
    }

    size_t NUM_KM1MERS = NUM_KMERS >> 2;
    int* h_m1 = malloc(sizeof *h_m1 *NUM_KM1MERS);
    for(i=0; i<NUM_KM1MERS; i+=1){
	h_m1[i] = -3;
    }

    size_t NUM_KP1MERS = NUM_KMERS << 2;
    int* h_p1 = malloc(sizeof *h_p1 *NUM_KP1MERS);
    for(i=0; i<NUM_KP1MERS; i+=1){
	h_p1[i] = -3;
    }

    size_t NUM_CENTERS;
    kmer* centers = readCentersFromFile(centers_file, k, &NUM_CENTERS);
    Island* islands = malloc(sizeof *islands *NUM_CENTERS);

    for(i=0; i<NUM_CENTERS; i+=1){
	h[centers[i]] = i;
	IslandInit(&islands[i], centers[i]);
    }

    free(centers);

    
    int radius, k2;
    Island *cur_center;
    kmer s;
    bool conflict;
    int threshold = q>>1;
    for(radius=1; radius<=threshold; radius+=1){
	//for each ci
	for(i=0; i<NUM_CENTERS; i+=1){
	    cur_center = &islands[i];
	    //generate all k-mers radius away from ci
	    getNextLayer(cur_center, k, h, h_m1, h_p1);
	    kmer* layer = cur_center->bfs_layer->arr;
	    
	    for(j=0; j<cur_center->bfs_layer->used; j+=1){
		s = layer[j];
		//skip if s has been assigned
		if(s >= luMSB){
		    k2 = k-1;
		    s ^= luMSB;
		    if(h_m1[s] > -2) continue;
		}else if(s >= luSMSB){
		    k2 = k+1;
		    s ^= luSMSB;
		    if(h_p1[s] > -2) continue;
		}else{
		    k2 = k;
		    if(h[s] > -2) continue;
		}
		
		conflict = conflictWithNeighbors(s, k, k2, p-1, i,
						 h, h_m1, h_p1);
	        
		if(!conflict){
		    if(k2 < k){
			h_m1[s] = i;
		    }else if(k2 > k){
			h_p1[s] = i;
		    }else{
			h[s] = i;
		    }
		}else{
		    if(k2 < k){
			h_m1[s] = -1;
		    }else if(k2 > k){
			h_p1[s] = -1;
		    }else{
			h[s] = -1;
		    }
		}
	    }
	}
    }

    for(i=0; i<NUM_CENTERS; i+=1){
	IslandFree(&islands[i]);
    }
    free(islands);

    char output_filename[50];
    sprintf(output_filename, "h%d-%d-%d-%.*s.hash-v4", k, p, q, 4, centers_file);
    FILE* fout = fopen(output_filename, "w");

    fprintf(fout, "k-mers\n");
    for(i=0; i<NUM_KMERS; i+=1){
	//printf("%.*s %d\n", k, decode(i, k, output_filename), h[i]);
	if(h[i] > -3){
	    fprintf(fout, "%.*s %d\n", k, decode(i, k, output_filename), h[i]);
	}
	//fprintf(fout, "%lu\t%d\n", i, h[i]);
    }
    free(h);

    fprintf(fout, "(k-1)-mers\n");
    for(i=0; i<NUM_KM1MERS; i+=1){
	if(h_m1[i] > -3){
	    fprintf(fout, "%.*s %d\n", k-1, decode(i, k-1, output_filename), h_m1[i]);
	}
    }
    free(h_m1);

    fprintf(fout, "(k+1)-mers\n");
    for(i=0; i<NUM_KP1MERS; i+=1){
	if(h_p1[i] > -3){
	    fprintf(fout, "%.*s %d\n", k+1, decode(i, k+1, output_filename), h_p1[i]);
	}
    }
    free(h_p1);

    fclose(fout);
    return 0;
}
