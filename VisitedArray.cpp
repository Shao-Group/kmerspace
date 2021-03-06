#include <cstdlib>
#include <cstring>
#include "VisitedArray.h"

/*
 * Constructor
 *
 * s: total number of elements
 */
VisitedArray::VisitedArray( unsigned long int s )
{
    sub_size = 1ul << 20; // Default sub_size is 2^20 bytes.
    num_subs = s / 8 / sub_size; // Each element occupies 1 bit (1/8 bytes).
    if ( (s / 8) % sub_size != 0 )
    {
        num_subs++;
    }
    aptrs = (char **) calloc( num_subs, sizeof(char *) );
    for (int i = 0; i < num_subs; ++i)
    {
        aptrs[i] = (char *) calloc( sub_size, 1 );
    }
    size = s;
}

/*
 * Constructor
 *
 * s : total number of elements
 * ss: 2^ss is the size of a subarray in bytes
 */
VisitedArray::VisitedArray( unsigned long int s, int ss )
{
    sub_size = 1ul << ss;
    num_subs = s / 8 / sub_size; // Each element occupies 1 bit (1/8 bytes).
    if ( (s / 8) % sub_size != 0 )
    {
        num_subs++;
    }
    aptrs = (char **) calloc( num_subs, sizeof(char *) );
    for (int i = 0; i < num_subs; ++i)
    {
        aptrs[i] = (char *) calloc( sub_size, 1 );
    }
    size = s;
}

/*
 * Destructor
 */
VisitedArray::~VisitedArray()
{
    for (int i = 0; i < num_subs; ++i)
    {
        free( aptrs[i] );
    }
    free( aptrs );
}

/*
 * Overload [] operator to return the visited value indexed by sub
 * 0 - unvisited
 * 1 - visited
 *
 * sub: The index of the element to be extract
 */
bool VisitedArray::operator[]( const unsigned long int &sub )
{
    // Find the bit offset
    int offset = sub % 8;

    // Find the byte in which the required element is located
    unsigned long int bytePos = sub / 8;

    unsigned int temp;
    memcpy( &temp, &aptrs[bytePos/sub_size][bytePos%sub_size], 1 );

    temp = temp & (1 << offset);
    return ( temp == (1 << offset) );
}
    
/*
 * Set the element indexed by sub as visited
 *
 * sub: The index of the element
 */
void VisitedArray::setVisit( const unsigned long int sub )
{
    // Find the bit offset
    int offset = sub % 8;

    // Find the byte in which the required element is located
    unsigned long int bytePos = sub / 8;

    unsigned int temp;
    memcpy( &temp, &aptrs[bytePos/sub_size][bytePos%sub_size], 1 );
    temp = temp | (1 << offset);
    memcpy( &aptrs[bytePos/sub_size][bytePos%sub_size], &temp, 1 );
}
