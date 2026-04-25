/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * 
    * Maximum Knapsack - Space-Optimized DP (1D / Rolling Array)
    * ===========================================================
    * Complexity : O(n*W) time, O(W) space
    * Space-optimized DP Implementation
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
#include <time.h>
 
/* ------------------------------------------------------------------ */
/*  Data                                                                */
/* ------------------------------------------------------------------ */
 
typedef struct {
    int weight;
    int value;
} Item;
 
/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */
 
static inline int imax(int a, int b) { return a > b ? a : b; }
 
/* ------------------------------------------------------------------ */
/*  1-D DP solver                                                       */
/* ------------------------------------------------------------------ */
 
/*
 * dp[j] = best value achievable with capacity exactly j.
 *
 * For each item we sweep j from W down to weight[i].
 * Sweeping backwards ensures each item is only used once:
 *   dp[j] = max(dp[j], dp[j - w[i]] + v[i])
 *
 * To recover the solution we store a "keep" matrix (n x W+1) — only
 * one bit per cell, so it is much smaller than the full 2-D table.
 */
void solveKnapsack1D(Item *items, int n, int W) {
    /* Allocate 1-D DP array */
    int *dp = (int *)calloc(W + 1, sizeof(int));
    if (!dp) { perror("calloc dp"); return; }
 
    /*
     * For backtracking we need to know which items were included.
     * Store a compact boolean matrix: keep[i][j] == 1 means item i
     * was taken when capacity was j.
     * Memory: n*(W+1) bytes vs n*(W+1)*sizeof(int) for the full table.
     */
    char **keep = (char **)malloc(sizeof(char *) * n);
    if (!keep) { perror("malloc keep"); free(dp); return; }
    for (int i = 0; i < n; i++) {
        keep[i] = (char *)calloc(W + 1, sizeof(char));
        if (!keep[i]) {
            perror("calloc keep[i]");
            for (int k = 0; k < i; k++) free(keep[k]);
            free(keep); free(dp);
            return;
        }
    }
 
    /* Fill */
    for (int i = 0; i < n; i++) {
        int wi = items[i].weight;
        int vi = items[i].value;
        /* Sweep RIGHT TO LEFT — critical for 0/1 constraint */
        for (int j = W; j >= wi; j--) {
            int withItem = dp[j - wi] + vi;
            if (withItem > dp[j]) {
                dp[j]      = withItem;
                keep[i][j] = 1;
            }
        }
    }
 
    int maxValue = dp[W];
 
    /* Backtrack using keep matrix */
    printf("=== 1D Space-Optimized DP Result ===\n");
    printf("Max Value    : %d\n", maxValue);
    printf("Memory used  : %lu bytes for dp[] + %d bytes for keep[][]\n",
           (unsigned long)(W + 1) * sizeof(int), n * (W + 1));
 
    printf("Items chosen (0-indexed): { ");
    int j = W;
    for (int i = n - 1; i >= 0; i--) {
        if (keep[i][j]) {
            printf("[w=%d v=%d] ", items[i].weight, items[i].value);
            j -= items[i].weight;
        }
    }
    printf("}\n");
 
    /* Cleanup */
    for (int i = 0; i < n; i++) free(keep[i]);
    free(keep);
    free(dp);
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
    if (!items) { perror("malloc"); fclose(fp); return EXIT_FAILURE; }
 
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d %d", &items[i].weight, &items[i].value);
    }
    fclose(fp);
 
    printf("Capacity W = %d, Items n = %d\n\n", W, n);
 
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
 
    solveKnapsack1D(items, n, W);
 
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed_sec = (ts_end.tv_sec - ts_start.tv_sec)
                       + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    /* Analytical peak memory:
     * Single 1D dp array of (W+1) integers.
     * (The keep[][] matrix is a backtracking aid, not the algorithm's
     *  core space — the theoretical 1D-DP space is one array.)
     * Cast to double BEFORE multiplication to prevent 32-bit overflow. */
    double mem_mb = ((double)(W + 1) * sizeof(int)) / 1048576.0;

    printf("TIME_SEC:%.6f\n", elapsed_sec);
    printf("MEMORY_MB:%.6f\n", mem_mb);
 
    free(items);
    return EXIT_SUCCESS;
}