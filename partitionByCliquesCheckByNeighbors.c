/*
  ******************************************************************
  Different from the partitionByLayers*.c family, 
  here the input centers are cliques (clique{k}{d}.MaxID). For each
  clique we enumerate all k-mers at most q/2 away and assign them to
  this clique (if no conflict).
  ******************************************************************
  Different from the fourth version 
  (partitionByLayersCheckByNeighborsWithP1M1.c),
  here we only assign k-mers.
  The result should agree with the first version (partitionByLayers.c)
  but is potentially faster for smaller d (more centers) and
  smaller p (do bfs (p-1)-neighbors for each s).
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
  
  But instead of keeping a list of assigned k-mers for each center, 
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
  Last edited: 10/30/2021
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

typedef struct{
    int num_centers;
    kmer* centers;
    //(k-1)-mers has their msb set to 1
    ArrayList* bfs_layer; //all the k- and (k-1)-mers to be examined by next step of bfs
} Island;

void IslandInit(Island* id, int num_centers, kmer* c){
    id->num_centers = num_centers;
    id->centers = c;

    id->bfs_layer = malloc_harder(sizeof *(id->bfs_layer));
    AListInit(id->bfs_layer);
    int i;
    for(i=0; i<num_centers; i+=1){
	AListInsert(id->bfs_layer, (void*) c[i]);
    }
}

void IslandFree(Island* id){
    free(id->centers);
    AListFree(id->bfs_layer, NULL);
    free(id->bfs_layer);
}

/*
  Given the current bfs layer, for each k-mer, perform one
  substitution or one deletion; for each (k-1)-mer, perform
  one insertion; check all results against the h (for k-mers)
  and visited (for (k-1)-mers) arrays. If not visited 
  (h==-3 or visited==TRUE), add to new_layer and update
  h and visited.
 */
void getNextLayer(Island* id, const int k, int* h, bool* visited){
    if(id->bfs_layer->used == 0) return;

    ArrayList* new_layer = malloc_harder(sizeof *new_layer);
    AListInit(new_layer);
    
    size_t i, j;
    kmer s, head, body, tail, x, m;
    for(i=0; i<id->bfs_layer->used; i+=1){
	s = (kmer) id->bfs_layer->arr[i];
	//k-mer
	if(s<luMSB){
	    //deletion
	    for(j=0; j<k; j+=1){
		head = (s>>((j+1)<<1))<<(j<<1);
		tail = ((1lu<<(j<<1))-1) & s;
		x = head|tail;
		//skip if visited
		if(visited[x]) continue;
		else{
		    AListInsert(new_layer, (void*)(x|luMSB));
		    visited[x] = TRUE;
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
			AListInsert(new_layer, (void*)x);
			h[x] = -2;
		    }
		}
	    }
	}
	//(k-1)-mer, no need to ^luMSB as the head will shift MSB out
	else{
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
			AListInsert(new_layer, (void*)x);
			h[x] = -2;
		    }
		}
	    }
	}
    }
    
    AListFree(id->bfs_layer, NULL);
    free(id->bfs_layer);
    id->bfs_layer = new_layer;
}

static inline bool cleanAndReturnConflict(bool conflict, HashTable* visited,
			    ArrayList* cur_layer, ArrayList* next_layer){
    HTableFree(visited);
    AListFree(cur_layer, NULL);
    AListFree(next_layer, NULL);
    return conflict;
}
/*
  Generate all k-mer neighbors of s up to depth away, if any of them
  is assigned a center other than c, return TRUE; otherwise return
  FALSE.
*/
bool conflictWithNeighbors(kmer s, int k, int depth, size_t c, int* h){
    HashTable visited;
    HTableInit(&visited);

    ArrayList cur_layer, next_layer;
    AListInit(&cur_layer);
    AListInit(&next_layer);

    HTableInsert(&visited, s);
    AListInsert(&cur_layer, (void*)s);

    size_t i, j;
    kmer head, body, tail, x, m;

    while(depth > 0){
	depth -= 1;
	for(i=0; i<cur_layer.used; i+=1){
	    s = (kmer) cur_layer.arr[i];
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
			    return cleanAndReturnConflict(TRUE, &visited,
							   &cur_layer,
							   &next_layer);
			}
			//add to next_layer if not visited
			else if(!HTableSearch(&visited, x)){
			    AListInsert(&next_layer, (void*) x);
			    HTableInsert(&visited, x);
			}
		    }
		}
	    }
	    //k-mer
	    else{
		//deletion
		for(j=0; j<k; j+=1){
		    head = (s>>((j+1)<<1))<<(j<<1);
		    tail = ((1lu<<(j<<1))-1) & s;
		    x = head|tail|luMSB;
		    //(k-1)-mer won't cause conflict
		    //add to next_layer if not visited
		    if(!HTableSearch(&visited, x)){
			AListInsert(&next_layer, (void*)x);
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
			    return cleanAndReturnConflict(TRUE, &visited,
							   &cur_layer,
							   &next_layer);
			}
			//add to next_layer if not visited
			else if(!HTableSearch(&visited, x)){
			    AListInsert(&next_layer, (void*)x);
			    HTableInsert(&visited, x);
			}
		    }
		}
	    }//end k-mer
	}//end for each in cur_layer

	AListClear(&cur_layer, NULL);
	AListSwap(&cur_layer, &next_layer);
    }

    return cleanAndReturnConflict(FALSE, &visited,
				   &cur_layer,
				   &next_layer);
}

int main(int argc, char* argv[]){
    if(argc != 5){
	printf("usage: partitionByCliquesCheckByNeighbors.out k p q centers_file\n");
	return 1;
    }

    int k = atoi(argv[1]);
    int p = atoi(argv[2]);
    int q = atoi(argv[3]);
    char* centers_file = argv[4];

    size_t NUM_KMERS = (1<<(k<<1));
    int* h = malloc_harder(sizeof *h *NUM_KMERS);

    size_t i, j;
    for(i=0; i<NUM_KMERS; i+=1){
	h[i] = -3;
    }

    size_t NUM_KM1MERS = NUM_KMERS >> 2;
    bool* h_m1 = calloc_harder(NUM_KM1MERS, sizeof *h_m1);
    

    size_t NUM_CENTERS;
    kmer** cliques = NULL;
    int* clique_sizes = readCliquesFromFile(centers_file, k, luMSB,
					    &cliques, &NUM_CENTERS);
    Island* islands = malloc_harder(sizeof *islands *NUM_CENTERS);

    kmer cur;
    for(i=0; i<NUM_CENTERS; i+=1){
	IslandInit(&islands[i], clique_sizes[i], cliques[i]);
	for(j=0; j<clique_sizes[i]; j+=1){
	    cur = cliques[i][j];
	    if(cur >= luMSB){//(k-1)-mer
		h_m1[cur^luMSB] = TRUE;
	    }else{
		h[cur] = i;
	    }
	}
    }
    free(cliques);
    free(clique_sizes);
    
    int radius;
    Island *cur_center;
    kmer s;
    bool conflict;
    int threshold = q>>1;
    for(radius=1; radius<=threshold; radius+=1){
	//for each ci
	for(i=0; i<NUM_CENTERS; i+=1){
	    cur_center = &islands[i];
	    //generate all k-mers radius away from ci
	    getNextLayer(cur_center, k, h, h_m1);
	    kmer* layer = (kmer*) cur_center->bfs_layer->arr;
	    
	    for(j=0; j<cur_center->bfs_layer->used; j+=1){
		s = layer[j];
		//skip if s is a (k-1)-mer or s has been assigned
		if(s >= luMSB || h[s] > -2) continue;
		
		conflict = conflictWithNeighbors(s, k, p-1, i, h);
	        
		if(!conflict){
			h[s] = i;
		}else{
			h[s] = -1;
		}
	    }
	}
    }

    for(i=0; i<NUM_CENTERS; i+=1){
	IslandFree(&islands[i]);
    }
    free(islands);
    free(h_m1);

    char output_filename[50];
    sprintf(output_filename, "h%d-%d-%d-%.*s.hash-c",
	    k, p, q, 4, centers_file+6); //skip the prefix of clique{k}{d}
    FILE* fout = fopen(output_filename, "w");

    for(i=0; i<NUM_KMERS; i+=1){
	if(h[i] > -3){
	    fprintf(fout, "%.*s %d\n", k, decode(i, k, output_filename), h[i]);
	}
    }
    free(h);

    fclose(fout);
    return 0;
}
