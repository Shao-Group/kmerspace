/*
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
  Different from the first version (partitionByLayers.c), here this 
  condition is checked by the following (potentially stricter) rule:
  for each s with h(s)=k, 
  dist(s, c_i) - dist(s, c_k) >= p for all i != k. 
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
  3. For each center c_k, calculating the set N(c_k) which 
  contains all the other centers c_i with 
  dist(c_k, c_i) <= q/2 + p + q/2 = p + q.
  Note that for c_j\notin N(c_k) and s\in A_k, by triangle 
  inequality, dist(s, c_j) >= dist(c_j, c_k) - dist(s, c_k).
  So dist(s, c_j) - dist(s, c_k) >= dist(c_j, c_k) - 2*dist(s, c_k)
  >p+q-2*(q/2) = p.
  4. For radius r = 1, 2, ..., q/2:
       For k = 1, 2, ..., m:
         Enumerate all k-mers s with dist(s, c_k) = r;
         For each s:
           If h(s) > -2, continue (s has been assigned)
	   For all centers c_j in N(c_k):
	     If dist(s, c_j) - r < p, assign h(s) = -1 and break
	   If s remains unassigned, assign h(s) = k

  Note - values for h: 
  h(s) >= 0 assigned to island c_h(s)
  h(s) = -1 assigned to gray area
  h(s) = -2 bfs visited
  h(s) = -3 untouched
 
  By: Ke@PSU
  Last edited: 10/16/2021
*/

#define _GNU_SOURCE //to use qsort_r

#include "util.h"
#include "ArrayList.h"
#include <time.h>
#include <unistd.h>

typedef int bool;
#define TRUE 1
#define FALSE 0

//mask for (k-1)-mers
#define luMSB 0x8000000000000000lu

typedef struct{
    kmer center;
    ArrayList* neighbor_indices; // indices of all the neighbor centers with dist<=p+q
    //(k-1)-mers has their msb set to 1
    ArrayList* bfs_layer; //all the k- and (k-1)-mers to be examined by next step of bfs
} Island;

void IslandInit(Island* id, const kmer c){
    id->center = c;
    id->neighbor_indices = malloc_harder(sizeof *(id->neighbor_indices));
    AListInit(id->neighbor_indices);

    id->bfs_layer = malloc_harder(sizeof *(id->bfs_layer));
    AListInit(id->bfs_layer);
    AListInsert(id->bfs_layer, (void*)c);
}

void IslandFree(Island* id){
    AListFree(id->neighbor_indices, NULL);
    free(id->neighbor_indices);
    AListFree(id->bfs_layer, NULL);
    free(id->bfs_layer);
}

/*
  Comparison function used by qsort_s to sort the kmers in neighbor list
  from closest to farthest.
*/
struct origin{
    kmer o;
    int k;
    kmer* centers;
};

int cmpKmerByDist(const void* s1, const void* s2, void* s3){
    struct origin* context = (struct origin*) s3;
    kmer a = context->centers[*(int*) s1];
    int d1 = editDist(a, context->o, context->k, -1);
    kmer b = context->centers[*(int*) s2];
    int d2 = editDist(b, context->o, context->k, d1+1);
    return d1 - d2;
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

int main(int argc, char* argv[]){
    if(argc != 5){
	printf("usage: partitionByLayersCheckByCenters.out k p q centers_file\n");
	return 1;
    }

    int k = atoi(argv[1]);
    int p = atoi(argv[2]);
    int q = atoi(argv[3]);
    char* centers_file = argv[4];

    size_t NUM_KMERS = (1<<(k<<1));
    int* h = malloc_harder(sizeof *h *NUM_KMERS);

    size_t i, j, m;
    for(i=0; i<NUM_KMERS; i+=1){
	h[i] = -3;
    }

    size_t NUM_KM1MERS = NUM_KMERS >> 2;
    bool* visited = calloc_harder(NUM_KM1MERS, sizeof *visited);

    size_t NUM_CENTERS;
    kmer* centers = readCentersFromFile(centers_file, k, &NUM_CENTERS);
    Island* islands = malloc_harder(sizeof *islands *NUM_CENTERS);

    for(i=0; i<NUM_CENTERS; i+=1){
	h[centers[i]] = i;
	IslandInit(&islands[i], centers[i]);
    }

    int dist;
    int threshold = p + q;
    
    struct origin cur_origin;
    cur_origin.k = k;
    cur_origin.centers = centers;
    
    for(i=0; i<NUM_CENTERS; i+=1){
	for(j=i+1; j<NUM_CENTERS; j+=1){
	    dist = editDist(islands[i].center, islands[j].center, k, -1);
	    if(dist <= threshold){
		AListInsert(islands[i].neighbor_indices, (void*)j);
		AListInsert(islands[j].neighbor_indices, (void*)i);
	    }
	}
	//may save some space
	//AListTrim(islands[i].neighbor_indices);

	//sort the neighbor list
	cur_origin.o = islands[i].center;
	qsort_r(islands[i].neighbor_indices->arr,
		islands[i].neighbor_indices->used,
		sizeof *islands[i].neighbor_indices->arr,
		cmpKmerByDist, &cur_origin);
    }
    free(centers);

    int radius;
    Island *cur_center, neighbor;
    kmer s;
    bool conflict;
    threshold = q>>1;
    for(radius=1; radius<=threshold; radius+=1){
	//for each ci
	for(i=0; i<NUM_CENTERS; i+=1){
	    cur_center = &islands[i];
	    //generate all k-mers radius away from ci
	    getNextLayer(cur_center, k, h, visited);
	    kmer* layer = (kmer*) cur_center->bfs_layer->arr;
	    
	    for(j=0; j<cur_center->bfs_layer->used; j+=1){
		s = layer[j];
		//skip if s is a (k-1)-mer or s has been assigned
		if(s >= luMSB || h[s] > -2) continue;
		conflict = FALSE;
		//for each neighbor of ci
		for(m=0; m<cur_center->neighbor_indices->used; m+=1){
		    neighbor = islands[(size_t)(cur_center->neighbor_indices->arr[m])];
		    dist = editDist(s, neighbor.center, k, -1);
		    //if dist<r, s would have been examined by neighbor before
		    if(dist - radius < p){
			h[s] = -1;
			conflict = TRUE;
			break;
		    }
		}
		if(!conflict){
		    h[s] = i;
		}
	    }
	}
    }

    for(i=0; i<NUM_CENTERS; i+=1){
	IslandFree(&islands[i]);
    }
    free(islands);
    free(visited);

    char output_filename[50];
    sprintf(output_filename, "h%d-%d-%d-%.*s.hash-v2", k, p, q, 4, centers_file);
    FILE* fout = fopen(output_filename, "w");

    for(i=0; i<NUM_KMERS; i+=1){
	//printf("%.*s %d\n", k, decode(i, k, output_filename), h[i]);
	if(h[i] > -3){
	    fprintf(fout, "%.*s %d\n", k, decode(i, k, output_filename), h[i]);
	}
	//fprintf(fout, "%lu\t%d\n", i, h[i]);
    }

    fclose(fout);
    free(h);
    return 0;
}
