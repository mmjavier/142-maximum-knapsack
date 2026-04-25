/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * Maximum Knapsack - Branch and Bound (Best-First with Fractional Bound)
    * Complexity : Worst-case O(2^n), O(n) stack space
    * Input format: Line 1=W, Line 2=n, Lines 3..n+2=weight value
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int    weight;
    int    value;
    double density;
    int    origIndex;
} Item;

/* Node struct used for the recursive stack frame accounting */
typedef struct {
    int level;
    int curWeight;
    int curValue;
} Node;

static Item   *g_items;
static int     g_n;
static int     g_W;
static int     g_bestValue;
static int    *g_bestSolution;
static int    *g_curSolution;
static long long g_nodeCount;

int cmpDensityDesc(const void *a, const void *b) {
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;
    if (ib->density > ia->density) return  1;
    if (ib->density < ia->density) return -1;
    return 0;
}

double upperBound(int level, int curWeight, int curValue) {
    double bound  = curValue;
    int    remCap = g_W - curWeight;
    for (int i = level; i < g_n; i++) {
        if (remCap <= 0) break;
        if (g_items[i].weight <= remCap) {
            bound  += g_items[i].value;
            remCap -= g_items[i].weight;
        } else {
            bound += (double)g_items[i].value * remCap / g_items[i].weight;
            remCap = 0;
        }
    }
    return bound;
}

void bnb(int level, int curWeight, int curValue) {
    g_nodeCount++;

    if (level == g_n) {
        if (curValue > g_bestValue) {
            g_bestValue = curValue;
            memcpy(g_bestSolution, g_curSolution, sizeof(int) * g_n);
        }
        return;
    }

    /* INCLUDE branch */
    if (curWeight + g_items[level].weight <= g_W) {
        g_curSolution[level] = 1;
        int newWeight = curWeight + g_items[level].weight;
        int newValue  = curValue  + g_items[level].value;
        if (newValue > g_bestValue) {
            g_bestValue = newValue;
            memcpy(g_bestSolution, g_curSolution, sizeof(int) * g_n);
        }
        if (upperBound(level + 1, newWeight, newValue) > g_bestValue)
            bnb(level + 1, newWeight, newValue);
        g_curSolution[level] = 0;
    }

    /* EXCLUDE branch */
    if (upperBound(level + 1, curWeight, curValue) > g_bestValue)
        bnb(level + 1, curWeight, curValue);
}

int main(int argc, char *argv[]) {
    FILE *fp;
    if (argc < 2) { fprintf(stderr, "Usage: %s <input_file>\n", argv[0]); return EXIT_FAILURE; }

    fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); return EXIT_FAILURE; }

    int W, n;
    fscanf(fp, "%d", &W);
    fscanf(fp, "%d", &n);

    Item *items = (Item *)malloc(sizeof(Item) * n);
    if (!items) { perror("malloc items"); fclose(fp); return EXIT_FAILURE; }

    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d %d", &items[i].weight, &items[i].value);
        items[i].origIndex = i;
        items[i].density   = (items[i].weight > 0)
                             ? (double)items[i].value / items[i].weight : 0.0;
    }
    fclose(fp);

    printf("Capacity W = %d, Items n = %d\n\n", W, n);

    qsort(items, n, sizeof(Item), cmpDensityDesc);

    g_items        = items;
    g_n            = n;
    g_W            = W;
    g_bestValue    = 0;
    g_nodeCount    = 0;

    g_bestSolution = (int *)calloc(n, sizeof(int));
    g_curSolution  = (int *)calloc(n, sizeof(int));
    if (!g_bestSolution || !g_curSolution) {
        perror("calloc solution arrays");
        free(items);
        return EXIT_FAILURE;
    }

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    bnb(0, 0, 0);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    double elapsed_sec = (ts_end.tv_sec - ts_start.tv_sec)
                       + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    printf("=== Branch and Bound Result ===\n");
    printf("Max Value    : %d\n", g_bestValue);
    printf("Nodes visited: %lld  (vs 2^%d = %lld worst-case)\n",
           g_nodeCount, n, (1LL << (n < 62 ? n : 62)));

    int totalWeight = 0;
    printf("Items chosen (sorted-index / original-index):\n{ ");
    for (int i = 0; i < n; i++) {
        if (g_bestSolution[i]) {
            printf("[sortIdx=%d origIdx=%d w=%d v=%d] ",
                   i, items[i].origIndex, items[i].weight, items[i].value);
            totalWeight += items[i].weight;
        }
    }
    printf("}\n");
    printf("Total Weight : %d / %d\n", totalWeight, W);

    /* Analytical peak memory:
     * Item array + recursive call stack (max depth n, each frame ~ sizeof(Node)).
     * Cast to double BEFORE multiplication to prevent 32-bit overflow. */
    double mem_mb = ((double)n * sizeof(Item) + (double)n * sizeof(Node)) / 1048576.0;

    printf("TIME_SEC:%.6f\n", elapsed_sec);
    printf("MEMORY_MB:%.6f\n", mem_mb);

    free(g_bestSolution);
    free(g_curSolution);
    free(items);
    return EXIT_SUCCESS;
}