#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <boost/tokenizer.hpp>
#include "a3.hpp"

typedef long long ll;

using namespace std;

vector< pair<char, ll> > traces;
string::size_type sz = 0;   // alias of size_t

// parse single line
void parse_trace(std::string line) {
    boost::tokenizer<boost::char_separator<char>> tokens(line, boost::char_separator<char>(", \t"));
    char instruction;
    ll address;
    for(auto it = tokens.begin(); it != tokens.end(); it++) {
        if (it == tokens.begin()) {
            instruction = (*it)[0];
        } else {
            address = stoll("0x" + *it, &sz, 0);
        }
    }
    traces.push_back(make_pair(instruction, address));
}

// construct the traces vector from the input file
void construct_traces(std::ifstream &file) {
    string line;
    while (getline(file, line)) {
        parse_trace(line);
    }
    file.close();
}

ll BLOCKSIZE,L1_Size,L2_Size,L1Sets,L2Sets,L1_Assoc,L2_Assoc; //the paramters of caches
ll L1time=1,L2time=20,Memorytime=200; //access times

//we represent the tag array by a vector of vector of pairs where the first entry is the dirty bit, we ignore the cache array as we dont care
//about values
vector<vector<pair<bool,ll>>> L1Tag;
vector<vector<pair<bool,ll>>> L2Tag;

ll L1ReadHit=0,L1ReadMiss=0,L2ReadHit=0,L2ReadMiss=0;
ll L1WriteHit=0,L1WriteMiss=0,L2WriteHit=0,L2WriteMiss=0;
ll WriteBackFromL1=0,WriteBackFromL2=0;
ll L1update=0,L2update=0;
ll MemoryRead=0,MemoryWrite=0;

//cache functions start from here
ll MemoryBlock(ll byte)
{
    return byte/BLOCKSIZE;
}
ll L1set(ll blockindex)
{
    return blockindex%L1Sets;
}
ll L2set(ll blockindex)
{
    return blockindex%L2Sets;
}
ll L1tag(ll blockindex)
{
    return (blockindex-blockindex%L1Sets)/L1Sets;
}
ll L2tag(ll blockindex)
{
    return (blockindex-blockindex%L2Sets)/L2Sets;
}

void WriteMemory(ll memoryblock)
{
    MemoryWrite=MemoryWrite+1;
}
void ReadMemorytoBoth(ll memoryblock)
{
    MemoryRead=MemoryRead+1;
    AddL2(memoryblock);
    AddL1(memoryblock);
}

void ReadMemoryToL2(ll memoryblock)
{
    MemoryRead=MemoryRead+1;
    AddL2(memoryblock);
}

void WriteL2(ll memoryblock)
{
    ll index=L2set(memoryblock);
    ll tag=L2tag(memoryblock);
    bool Hit=false;
    pair<bool,ll> target;
    vector<pair<bool,ll>> tempTag;
    for(ll j=0;j<(ll)L2Tag[index].size();j++)
    {
        if(L2Tag[index][j].second == tag)
        {
            Hit=true;
            L2Tag[index][j].first=true;
            target=L2Tag[index][j];
        }
        else tempTag.push_back(L2Tag[index][j]);
    }
    if(Hit == true)
    {
        L2WriteHit=L2WriteHit+1;
        tempTag.push_back(target);
        L2Tag[index]=tempTag;
    }
    else
    {
        L2WriteMiss=L2WriteMiss+1;
        ReadMemoryToL2(memoryblock);

        //so make sure that the dirty bit of memoryblock is turned on, because ReadMemorytoL2 calls AddL2 and AddL2 doesnt change any
        //block's dirty bit. Basically write on the block after bringing it in the L2 cache
        //the block would be at the back of L2Tag[index] anyways
        for(ll j=0;j<(ll)L2Tag[index].size();j++)
        {
            if(L2Tag[index][j].second == tag)
            {
                L2Tag[index][j].first=true;
                break;
            }
        }
    }
}

void ReadL2(ll memoryblock)
{
    ll index=L2set(memoryblock);
    ll tag=L2tag(memoryblock);
    bool Hit=false;
    pair<bool,ll> target;
    vector<pair<bool,ll>> tempTag;
    for(ll j=0;j<(ll)L2Tag[index].size();j++) //L2Tag[index].size() can be atmost L2_Assoc
    {
        //comparing tag entries with tag of memoryblock
        if(L2Tag[index][j].second == tag)
        {
            Hit=true;
            target=L2Tag[index][j];
        }
        else tempTag.push_back(L2Tag[index][j]);
    }
    if(Hit == true)
    {
        L2ReadHit=L2ReadHit+1;
        tempTag.push_back(target);
        L2Tag[index]=tempTag;
        AddL1(memoryblock);
    }

    else
    {
        //so there was no tag of memoryblock in L1Tag,increment L2ReadMiss and go to main memory
        L2ReadMiss=L2ReadMiss+1;
        ReadMemorytoBoth(memoryblock);
    }   
}

void AddL2(ll memoryblock)
{
    L2update=L2update+1;
    //so we are sure that this block is not there in L2 cache
    ll index=L2set(memoryblock);
    ll tag=L2tag(memoryblock);
    if((ll) L2Tag[index].size() == L2_Assoc)
    {
        //so eviction needs to be done
        pair<bool,ll> evicted;
        vector<pair<bool,ll>> tempTag;
        //now using the LRU policy
        for(ll j=1;j<(ll)L2Tag[index].size();j++) tempTag.push_back(L2Tag[index][j]);
        tempTag.push_back({false,tag});
        evicted=L2Tag[index][0];
        L2Tag[index]=tempTag;

        if(evicted.first == true)
        {
            //dirty bit is on
            WriteBackFromL2=WriteBackFromL2+1;
            ll evicted_memoryblock=L2Sets*evicted.second+index;
            WriteMemory(evicted_memoryblock);
        }
    }

    else
    {
        //no eviction required
        L2Tag[index].push_back({false,tag});
    } 
}

void WriteL1(ll memoryblock)
{
    ll index=L1set(memoryblock);
    ll tag=L1tag(memoryblock);
    bool Hit=false;
    pair<bool,ll> target;
    vector<pair<bool,ll>> tempTag;

    for(ll j=0;j<(ll)L1Tag[index].size();j++)
    {
        if(L1Tag[index][j].second == tag)
        {
            Hit=true;
            L1Tag[index][j].first=true;
            target=L1Tag[index][j];
        }
        else tempTag.push_back(L1Tag[index][j]);
    }

    if(Hit == true)
    {
        L1WriteHit=L1WriteHit+1;
        tempTag.push_back(target);
        L1Tag[index]=tempTag;
    }
    else
    {
        L1WriteMiss=L1WriteMiss+1;
        ReadL2(memoryblock);
        //now turn the dirty bit of memoryblock on,basically write on the block that is brough into L1
        //the block would be at the back of L1Tag[index]
        for(ll j=0;j<(ll)L1Tag[index].size();j++)
        {
            if(L1Tag[index][j].second == tag)
            {
                L1Tag[index][j].first=true;
                break;
            }
        }
    }
}

void ReadL1(ll memoryblock)
{
    ll index=L1set(memoryblock);
    ll tag=L1tag(memoryblock);
    bool Hit=false;
    pair<bool,ll> target;
    vector<pair<bool,ll>> tempTag;
    for(ll j=0;j<(ll)L1Tag[index].size();j++) //L1Tag[index].size() can be atmost L1_Assoc
    {
        //comparing tag entries with tag of memoryblock
        if(L1Tag[index][j].second == tag)
        {
            Hit=true;
            target=L1Tag[index][j];
        }
        else tempTag.push_back(L1Tag[index][j]);
    }
    if(Hit)
    {
        L1ReadHit=L1ReadHit+1;
        tempTag.push_back(target);
        L1Tag[index]=tempTag;
    }
    else
    {
        //so there was no tag of memoryblock in L1Tag,increment L1ReadMiss and go to L2
        L1ReadMiss=L1ReadMiss+1;
        ReadL2(memoryblock);
    }
}

void AddL1(ll memoryblock)
{
    L1update=L1update+1;
    //so we are sure that this block is not there in L1 cache
    ll index=L1set(memoryblock);
    ll tag=L1tag(memoryblock);
    if((ll) L1Tag[index].size() == L1_Assoc)
    {
        //so eviction needs to be done as all ways are filled
        pair<bool,ll> evicted;
        vector<pair<bool,ll>> tempTag;
        //now using the LRU policy
        for(ll j=1;j<(ll)L1Tag[index].size();j++) tempTag.push_back(L1Tag[index][j]);
        tempTag.push_back({false,tag});
        evicted=L1Tag[index][0];
        L1Tag[index]=tempTag;

        if(evicted.first == true)
        {
            //dirty bit is on
            WriteBackFromL1=WriteBackFromL1+1;
            ll evicted_memoryblock=L1Sets*(evicted.second)+index;
            WriteL2(evicted_memoryblock);
        }
    }

    else
    {
        //no eviction required
        L1Tag[index].push_back({false,tag});
    }
}


int main (int argc, char *argv[]) {
    if (argc != 7) {
        cerr << "required arguments: blocksize, l1_size, l1_ssoc, l2_size, l2_assoc, filename\n";
        return 1;
    }
    BLOCKSIZE = stoll(argv[1], &sz, 0);
    L1_Size = stoll(argv[2], &sz, 0);
    L1_Assoc = stoll(argv[3], &sz, 0);
    L2_Size = stoll(argv[4], &sz, 0);
    L2_Assoc = stoll(argv[5], &sz, 0);
    string filename = argv[6];
    ifstream file(filename);
    if (file.is_open()) {
        // do caching
        construct_traces(file);
    } else {
        cerr << "File could not be opened. Terminating...\n";
        return 1;
    }

    //code starts from here
    L1Sets=(L1_Size)/(BLOCKSIZE*L1_Assoc);
    L2Sets=(L2_Size)/(BLOCKSIZE*L2_Assoc);
    L1Tag.resize(L1Sets);
    L2Tag.resize(L2Sets);

    for(ll i=0;i<(ll)traces.size();i++)
    {
        ll memoryblock=MemoryBlock(traces[i].second);
        if(traces[i].first == 'r') ReadL1(memoryblock);
        else WriteL1(memoryblock);
    }
    ll L1Reads = L1ReadHit + L1ReadMiss;
    ll L1Writes = L1WriteHit + L1WriteMiss;
    ll L2Reads = L2ReadHit + L2ReadMiss;
    ll L2Writes = L2WriteHit + L2WriteMiss;
    cout << "i. number of L1 reads : " << L1Reads << '\n';
    cout << "ii. number of L1 read misses : " << L1ReadMiss << '\n';
    cout << "iii. number of L1 writes : " << L1Writes << '\n';
    cout << "iv. number of L1 write misses : " << L1WriteMiss << '\n';
    cout << "v. L1 miss rate : " << fixed << setprecision(4) << float(L1ReadMiss + L1WriteMiss)/float(L1Reads + L1Writes) << '\n';
    cout << "vi. number of writebacks from L1 : " << WriteBackFromL1 << '\n';
    cout << "vii. number of L2 reads : " << L2Reads << '\n';
    cout << "viii. number of L2 read misses : " << L2ReadMiss << '\n';
    cout << "ix. number of L2 writes : " << L2Writes << '\n';
    cout << "x. number of L2 write misses : " << L2WriteMiss << '\n';
    cout << "xi. L2 miss rate : " << fixed << setprecision(4) << float(L2ReadMiss + L2WriteMiss)/float(L2Reads + L2Writes) << '\n';
    cout << "xii. number of writebacks from L2 : " << WriteBackFromL2 << '\n';

    ll L1_time_taken = (L1ReadHit+L1ReadMiss+L1WriteHit+L1WriteMiss+L1update)*L1time;
    ll L2_time_taken = (L2ReadHit+L2ReadMiss+L2WriteHit+L2WriteMiss+L2update)*L2time;
    ll Memory_time_taken = (MemoryRead+MemoryWrite)*Memorytime;

    ll Total_time_taken = (L1_time_taken + L2_time_taken + Memory_time_taken);

    cout << "xiii. total access time(in nanoseconds) : " << Total_time_taken << '\n';

    L1Tag.clear();
    L2Tag.clear();
    return 0;
}
