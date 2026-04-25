/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * Maximum Knapsack - Greedy (Density-Based / Fractional Knapsack Heuristic)
    * Complexity : O(n log n) time, O(n) space
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

int cmpDensityDesc(const void *a, const void *b) {
    const Item *ia = (const Item *)a;
    const Item *ib = (const Item *)b;
    if (ib->density > ia->density) return  1;
    if (ib->density < ia->density) return -1;
    return 0;
}

void greedyKnapsack(Item *items, int n, int W) {
    qsort(items, n, sizeof(Item), cmpDensityDesc);

    printf("Items sorted by density (v/w) descending:\n");
    printf("  idx  w   v   density\n");
    for (int i = 0; i < n; i++) {
        printf("  [%2d] %-4d %-4d %.4f\n",
               items[i].origIndex, items[i].weight,
               items[i].value, items[i].density);
    }
    printf("\n");

    int totalValue  = 0;
    int totalWeight = 0;

    printf("=== Greedy Result ===\n");
    printf("Items chosen (original index): { ");
    for (int i = 0; i < n; i++) {
        if (totalWeight + items[i].weight <= W) {
            totalWeight += items[i].weight;
            totalValue  += items[i].value;
            printf("[idx=%d w=%d v=%d] ",
                   items[i].origIndex, items[i].weight, items[i].value);
        }
    }
    printf("}\n");

    printf("Total Value  : %d\n", totalValue);
    printf("Max Value    : %d\n", totalValue);
    printf("Total Weight : %d / %d\n", totalWeight, W);
    printf("\nNOTE: Greedy is NOT guaranteed to be optimal for 0/1 knapsack.\n");
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
    if (!items) { perror("malloc"); fclose(fp); return EXIT_FAILURE; }

    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d %d", &items[i].weight, &items[i].value);
        items[i].origIndex = i;
        items[i].density   = (items[i].weight > 0)
                             ? (double)items[i].value / items[i].weight : 0.0;
    }
    fclose(fp);

    printf("Capacity W = %d, Items n = %d\n\n", W, n);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    greedyKnapsack(items, n, W);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    double elapsed_sec = (ts_end.tv_sec - ts_start.tv_sec)
                       + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    /* Analytical peak memory: only the item array.
     * Cast to double BEFORE multiplication to prevent 32-bit overflow. */
    double mem_mb = ((double)n * sizeof(Item)) / 1048576.0;

    printf("TIME_SEC:%.6f\n", elapsed_sec);
    printf("MEMORY_MB:%.6f\n", mem_mb);

    free(items);
    return EXIT_SUCCESS;
}