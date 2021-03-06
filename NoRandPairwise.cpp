/*
 * This program addresses the following problem from Prof. Mingfu Shao (mxs2589@psu.edu).
 *
 * --------------------------------------------------------------------------------------
 * Let K be the set of all kmers over alphabet {A, C, G, T}. |K| = 4^k. We build a graph
 * G = (K, E) with K be the set of vertices. Let x and y be two kmers in K. We add an
 * edge to E iff the edit distance between x and y is at most d. We want to calculate the
 * an maximal independent set of G that covers all vertices.
 *
 * The input for this problem is two integers k and d; the output is an independent set
 * of G constructed above.
 *
 * We use a simple greedy algorithm. We first generate K. We then iteratively pick a kmer
 * x from K, remove x, and check if x is covered by any kmer in the current MIS. If not,
 * we add x to the MIS. We repeat this procedure until K becomes empty.
 * --------------------------------------------------------------------------------------
 *
 * In the program, the author uses a binary encoding {00, 01, 10, 11} for the alphabet
 * {A, C, G, T} in order to save space. Thus, any k-mer that is not longer than 32
 * characters can be represented by a 64-bit binary number stored in an 8-byte slot.
 *
 * Author: Leran Ma (lkm5463@psu.edu)
 */

#include <iostream>
#include <vector>
#include <unistd.h>
#include <ios>
#include <fstream>
#include <string>

using namespace std;

/*
 * Calculates the edit distance between 2 k-mers
 *
 * s1: The encoding of the first k-mer
 * s2: The encoding of the second k-mer
 * k : The length of the two k-mers
 * d : The maximum edit distance allowed
 */
int editDist(const unsigned long int s1, const unsigned long int s2, const int k, const int d)
{
    int DPtable[k + 1][k + 1];
    for (int i = 0; i < k + 1; ++i)
    {
        DPtable[0][i] = i;
        DPtable[i][0] = i;
    }
    for (int i = 1; i < k + 1; ++i)
    {
        for (int j = 1; j < k + 1; ++j)
        {
            if ( ((s1 >> (2*(i-1))) & 3) != ((s2 >> (2*(j-1))) & 3) )
            {
                DPtable[i][j] = DPtable[i-1][j-1] + 1;
            }
            else
            {
                DPtable[i][j] = DPtable[i-1][j-1];
            }
            if (DPtable[i-1][j] + 1 < DPtable[i][j])
            {
                DPtable[i][j] = DPtable[i-1][j] + 1;
            }
            if (DPtable[i][j-1] + 1 < DPtable[i][j])
            {
                DPtable[i][j] = DPtable[i][j-1] + 1;
            }
        }
        if ( DPtable[i][i] > d )
        {
            return DPtable[i][i];
        }
    }
    return DPtable[k][k];
}

/*
 * Prints a k-mer given its binary encoding
 *
 * enc: The binary encoding of the k-mer
 * k  : The length of the k-mer
 */
void printKmer(unsigned long int enc, int k)
{
    char base[4] = {'A', 'C', 'G', 'T'};
    char kmer[k + 1];
    kmer[k] = '\0';
    for (int i = k - 1; i >= 0; --i)
    {
        kmer[i] = base[enc & 3];
        enc = enc >> 2;
    }
    cerr << kmer;
}

/*
 * Reports the time and space usage
 */
void reportPerformance()
{
    string temp;
    unsigned long int utime, stime, vmpeak, vmhwm;

    // Find time usage in "/proc/self/stat"
    ifstream time_stream("/proc/self/stat", ios_base::in);

    // Skip all irrelevant attributes
    for (int i = 0; i < 13; ++i)
    {
        time_stream >> temp;
    }

    time_stream >> utime >> stime;
    time_stream.close();

    // Find space usage in "/proc/self/status"
    ifstream space_stream("/proc/self/status", ios_base::in);
    while ( temp.compare("VmPeak:") != 0 )
    {
        space_stream >> temp;
    }
    space_stream >> vmpeak;
    while ( temp.compare("VmHWM:") != 0 )
    {
        space_stream >> temp;
    }
    space_stream >> vmhwm;
    space_stream.close();

    cerr << "Time in user mode:        " << utime / sysconf(_SC_CLK_TCK) << " sec\n"
         << "Time in kernel mode:      " << stime / sysconf(_SC_CLK_TCK) << " sec\n"
         << "Peak virtual memory size: " << vmpeak << " kB\n"
         << "Peak resident set size:   " << vmhwm << " kB\n\n";
}

int main()
{
    int k;
    int d;
    cerr << "Please enter k: ";
    cin >> k;
    cerr << k << endl;
    cerr << "Plesae enter d: ";
    cin >> d;
    cerr << d << endl;

    unsigned long int kmerSpaceSize = 1;
    kmerSpaceSize = kmerSpaceSize << (2 * k);

    vector<unsigned long int> MIS;
    cerr << "\nList of independent nodes: " << endl;
    printKmer(0, k);
    cerr << ' ';
    MIS.push_back( 0 );

    unsigned long int lastFound = 0;
    for (unsigned long int i = 1; i < kmerSpaceSize; ++i)
    {
        bool flag = false;
        unsigned long int dist = lastFound;
        if ( MIS.size() - lastFound - 1 < dist )
        {
            dist = MIS.size() - lastFound - 1;
        }

        for (unsigned long int j = 0; j <= dist; ++j)
        {
            if ( editDist(i, MIS[lastFound + j], k, d) <= d )
            {
                lastFound = lastFound + j;
                flag = true;
                break;
            }
            if ( editDist(i, MIS[lastFound - j], k, d) <= d )
            {
                lastFound = lastFound - j;
                flag = true;
                break;
            }
        }

        if ( dist == lastFound )
        {
            for (unsigned long int j = lastFound + dist + 1; j < MIS.size(); ++j)
            {
                if ( editDist(i, MIS[j], k, d) <= d )
                {
                    lastFound = j;
                    flag = true;
                    break;
                }
            }
        }
        else
        {
            for (unsigned long int j = lastFound - dist - 1; j >= 0 && j < MIS.size(); --j)
            {
                if ( editDist(i, MIS[j], k, d) <= d )
                {
                    lastFound = j;
                    flag = true;
                    break;
                }
            }
        }
        if (flag)
        {
            continue;
        }

        printKmer(i, k);
        cerr << ' ';
        MIS.push_back( i );
        lastFound = MIS.size() - 1;
    }

    cerr << "\nThe graph has an independent set of size " << MIS.size() << ".\n\n";
    reportPerformance();

    return 0;
}
