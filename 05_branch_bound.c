/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * 
    * Maximum Knapsack - Branch and Bound (Best-First with Fractional Bound)
    * =======================================================================
    * Complexity : Worst-case O(2^n), but pruning makes it far faster in
    *              practice — often better than DP on large, sparse instances.
    * Space      : O(n) recursion stack + O(n log n) for sorting.
    * Combination of Speed of Greedy (bounding function) with the correctness
    * of Brute Force.
    *          
    * Algorithm:
    *   1. Sort items by density descending.
    *   2. Recursively explore include/exclude branches.
    *   3. At each node compute an UPPER BOUND via the fractional knapsack
    *      relaxation on the remaining items.
    *   4. Prune any branch whose upper bound ≤ current best value.
    *
    * Input file format (txt):
    *   Line 1 : W   (knapsack capacity)
    *   Line 2 : n   (number of items)
    *   Lines 3…n+2 : weight_i  value_i
    *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Data                                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    int    weight;
    int    value;
    double density;
    int    origIndex;
} Item;

/* ------------------------------------------------------------------ */
/*  Global state shared by the recursion                               */
/* ------------------------------------------------------------------ */

static Item   *g_items;
static int     g_n;
static int     g_W;

static int     g_bestValue;
static int    *g_bestSolution;   /* boolean array, size n */
static int    *g_curSolution;    /* boolean array, size n */

static long long g_nodeCount;    /* for benchmarking */

/* ------------------------------------------------------------------ */
/*  Comparator — descending density                                     */
/* ------------------------------------------------------------------ */

int cmpDensityDesc(const void *a, const void *b) {
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;
    if (ib->density > ia->density) return  1;
    if (ib->density < ia->density) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Upper-bound function (fractional knapsack relaxation)              */
/* ------------------------------------------------------------------ */

/*
 * Given that we have already committed weight curWeight and value curValue
 * using items 0..level-1, compute the best possible value we could
 * achieve by filling the remaining capacity with fractional items
 * starting at index `level`.
 *
 * Items must already be sorted by density.
 */
double upperBound(int level, int curWeight, int curValue) {
    double bound    = curValue;
    int    remCap   = g_W - curWeight;

    for (int i = level; i < g_n; i++) {
        if (remCap <= 0) break;

        if (g_items[i].weight <= remCap) {
            bound  += g_items[i].value;
            remCap -= g_items[i].weight;
        } else {
            /* Take a fraction of this item */
            bound += (double)g_items[i].value * remCap / g_items[i].weight;
            remCap = 0;
        }
    }
    return bound;
}

/* ------------------------------------------------------------------ */
/*  Recursive Branch and Bound                                          */
/* ------------------------------------------------------------------ */

void bnb(int level, int curWeight, int curValue) {
    g_nodeCount++;

    /* Leaf node — update best */
    if (level == g_n) {
        if (curValue > g_bestValue) {
            g_bestValue = curValue;
            memcpy(g_bestSolution, g_curSolution, sizeof(int) * g_n);
        }
        return;
    }

    /* ---------- INCLUDE branch ---------- */
    if (curWeight + g_items[level].weight <= g_W) {
        g_curSolution[level] = 1;
        int newWeight = curWeight + g_items[level].weight;
        int newValue  = curValue  + g_items[level].value;

        /* Update best eagerly (complete feasible solution so far) */
        if (newValue > g_bestValue) {
            g_bestValue = newValue;
            memcpy(g_bestSolution, g_curSolution, sizeof(int) * g_n);
        }

        /* Recurse only if upper bound is promising */
        if (upperBound(level + 1, newWeight, newValue) > g_bestValue) {
            bnb(level + 1, newWeight, newValue);
        }
        g_curSolution[level] = 0;
    }

    /* ---------- EXCLUDE branch ---------- */
    if (upperBound(level + 1, curWeight, curValue) > g_bestValue) {
        bnb(level + 1, curWeight, curValue);
    }
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    FILE *fp;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

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
                             ? (double)items[i].value / items[i].weight
                             : 0.0;
    }
    fclose(fp);

    printf("Capacity W = %d, Items n = %d\n\n", W, n);

    /* Sort by density — required for the bounding function */
    qsort(items, n, sizeof(Item), cmpDensityDesc);

    /* Initialise globals */
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

    /* Run */
    bnb(0, 0, 0);

    /* Report */
    printf("=== Branch and Bound Result ===\n");
    printf("Max Value    : %d\n", g_bestValue);
    printf("Nodes visited: %lld  (vs 2^%d = %lld worst-case)\n",
           g_nodeCount, n, (1LL << (n < 62 ? n : 62)));

    int totalWeight = 0;
    printf("Items chosen (sorted-index / original-index):\n{ ");
    for (int i = 0; i < n; i++) {
        if (g_bestSolution[i]) {
            printf("[sortIdx=%d origIdx=%d w=%d v=%d] ",
                   i,
                   items[i].origIndex,
                   items[i].weight,
                   items[i].value);
            totalWeight += items[i].weight;
        }
    }
    printf("}\n");
    printf("Total Weight : %d / %d\n", totalWeight, W);

    free(g_bestSolution);
    free(g_curSolution);
    free(items);
    return EXIT_SUCCESS;
}
