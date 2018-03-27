#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

typedef struct {
    bool valid;
    unsigned tag;
    unsigned lru_mark;
} line;
typedef struct {
    line* lines;
} set;
typedef struct {
    int set_num;
    int line_num;
    set* sets;
} cache_struct;

int misses;
int hits;
int evictions;

void init_cache(cache_struct* cache, int s, int E)
{
    int i, j;
    cache -> set_num = 1 << s;
    cache -> line_num = E;
    cache->sets = (set*) malloc(cache->set_num * sizeof(set));
    if (!cache->sets) {
        printf("failed to allocate space!");
        exit(0);
    }
    for (i = 0; i < cache->set_num; ++i)
    {
        cache->sets[i].lines = (line*) malloc(cache->line_num * sizeof(line));
        if (!cache->sets[i].lines) {
            printf("failed to allocate space!");
            exit(0);
        }
        for (j = 0; j < cache->line_num; ++j)
        {
            cache->sets[i].lines[j].valid = false;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lru_mark = UINT_MAX;
        }
    }
}

void update_lru(cache_struct* cache, int set_index, int line_index)
{
    int i = 0;
    cache->sets[set_index].lines[line_index].lru_mark = UINT_MAX;
    for (i = 0; i < cache->line_num; ++i)
        if (i != line_index) --cache->sets[set_index].lines[i].lru_mark;
}

bool update_line(cache_struct* cache, int set_index, unsigned tag)
{
    bool is_full = true;
    int i;
    for (i = 0; i < cache->line_num; ++i)
        if (!cache->sets[set_index].lines[i].valid)
        {
            is_full = false;
            break;
        }
    if (is_full)
    {
        int min_lru_index = 0;
        unsigned min_lru_mark = UINT_MAX;
        for (i = 0; i < cache->line_num; ++i)
            if (cache->sets[set_index].lines[i].lru_mark < min_lru_mark) {
                min_lru_index = i;
                min_lru_mark = cache->sets[set_index].lines[i].lru_mark;
            }
        cache->sets[set_index].lines[min_lru_index].valid = true;
        cache->sets[set_index].lines[min_lru_index].tag = tag;
        update_lru(cache, set_index, min_lru_index);
        return true;
    }
    else
    {
        cache->sets[set_index].lines[i].valid = true;
        cache->sets[set_index].lines[i].tag = tag;
        update_lru(cache, set_index, i);
        return false;
    }
}

void load_cache(cache_struct* cache, int set_index, unsigned tag, bool verbose)
{
    int i;
    bool found_data = false;
    for (i = 0; i<cache->line_num; ++i)
        if (cache->sets[set_index].lines[i].valid && cache->sets[set_index].lines[i].tag == tag) {
            found_data = true;
            update_lru(cache, set_index, i);
            ++hits;
            if (verbose) printf("hit ");
            break;
        }
    if (!found_data) {
        ++misses;
        if (verbose) printf("miss ");
        if (update_line(cache, set_index, tag)) {
            ++evictions;
            if (verbose) printf("eviction ");
        }
    }
}

void store_cache(cache_struct* cache, int set_index, unsigned tag, bool verbose)
{
    load_cache(cache, set_index, tag, verbose);
}

void modify_cache(cache_struct* cache, int set_index, unsigned tag, bool verbose)
{
    load_cache(cache, set_index, tag, verbose);
    load_cache(cache, set_index, tag, verbose);
}

void print_help_info()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n\n");
}
int main(int argc, char **argv)
{
    bool verbose;
    int s, E, b;
    cache_struct cache;

    unsigned addr;
    int size;
    char operation[5];
    int set_index;
    unsigned tag;

    int c;
    char trace_file_name[100];

    while ((c = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch(c)
        {
            case 'v':
                verbose = true;
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
                strcpy(trace_file_name, optarg);
                break;
            case 'h':
                print_help_info();
                exit(0);
            default:
                exit(0);
        }
    }

    init_cache(&cache, s, E);

    FILE* trace_file = fopen(trace_file_name, "r");
    while(fscanf(trace_file ,"%s %x,%d", operation, &addr, &size) != EOF)
    {
        if (strcmp(operation, "I") == 0) continue;
        set_index = (addr >> b) & ((1 << s) - 1);
        tag = addr >> (b + s);
        if (verbose) printf("%s %x,%d ",operation, addr, size);
        if (strcmp(operation, "L") == 0) {
            load_cache(&cache, set_index, tag, verbose);
        }
        if (strcmp(operation, "S") == 0) {
            store_cache(&cache, set_index, tag, verbose);
        }
        if (strcmp(operation, "M") == 0) {
            modify_cache(&cache, set_index, tag, verbose);
        }
        if (verbose) printf("\n");
    }

    printSummary(hits, misses, evictions);
    return 0;
}
