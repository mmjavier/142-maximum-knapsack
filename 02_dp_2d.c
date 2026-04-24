/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * 
    * Maximum Knapsack - Standard Tabulation (2D DP)
    * ===============================================
    * Complexity : O(n*W) time, O(n*W) space
    * Base Dynamic Programming Implementation (Tabulation)
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
/*  2-D DP table generator  (mirrors genMat from the subset-sum lab)   */
/* ------------------------------------------------------------------ */
 
int **genTable(int n, int W) {
    /* Allocate (n+1) x (W+1) table initialised to 0 */
    int **dp = (int **)malloc(sizeof(int *) * (n + 1));
    if (!dp) return NULL;
 
    for (int i = 0; i <= n; i++) {
        dp[i] = (int *)calloc(W + 1, sizeof(int));
        if (!dp[i]) {
            /* Allocation failed — free what we have and signal */
            for (int k = 0; k < i; k++) free(dp[k]);
            free(dp);
            return NULL;
        }
    }
    return dp;
}
 
void freeTable(int **dp, int n) {
    for (int i = 0; i <= n; i++) free(dp[i]);
    free(dp);
}
 
/* ------------------------------------------------------------------ */
/*  Fill the DP table                                                   */
/* ------------------------------------------------------------------ */
 
/*
 * dp[i][j] = maximum value using items 0..i-1 with capacity j
 *
 * Recurrence:
 *   dp[0][j] = 0  for all j
 *   dp[i][j] = dp[i-1][j]                             if w[i] > j
 *            = max(dp[i-1][j], dp[i-1][j-w[i]]+v[i]) otherwise
 */
void computeDP(int **dp, Item *items, int n, int W) {
    /* Row 0 is already zero from calloc */
    for (int i = 1; i <= n; i++) {
        int wi = items[i - 1].weight;
        int vi = items[i - 1].value;
        for (int j = 0; j <= W; j++) {
            if (wi > j) {
                dp[i][j] = dp[i - 1][j];
            } else {
                int include = dp[i - 1][j - wi] + vi;
                int exclude = dp[i - 1][j];
                dp[i][j] = (include > exclude) ? include : exclude;
            }
        }
    }
}
 
/* ------------------------------------------------------------------ */
/*  Backtrack to recover the chosen items                              */
/* ------------------------------------------------------------------ */
 
void backtrack(int **dp, Item *items, int n, int W) {
    printf("Items chosen (0-indexed): { ");
    int j = W;
    for (int i = n; i > 0; i--) {
        if (dp[i][j] != dp[i - 1][j]) {
            /* Item i-1 was included */
            printf("[w=%d v=%d] ", items[i - 1].weight, items[i - 1].value);
            j -= items[i - 1].weight;
        }
    }
    printf("}\n");
}
 
/* ------------------------------------------------------------------ */
/*  Print a fragment of the DP table (for small inputs)               */
/* ------------------------------------------------------------------ */
 
void printTable(int **dp, Item *items, int n, int W) {
    int maxCols = (W < 20) ? W : 20;
    int maxRows = (n < 10) ? n : 10;
 
    printf("\nDP Table (rows=items 0..%d, cols=capacity 0..%d):\n",
           maxRows, maxCols);
    printf("     ");
    for (int j = 0; j <= maxCols; j++) printf("%4d", j);
    printf("\n");
 
    for (int i = 0; i <= maxRows; i++) {
        if (i == 0) printf("  -- ");
        else        printf("w=%2d ", items[i - 1].weight);
        for (int j = 0; j <= maxCols; j++) printf("%4d", dp[i][j]);
        printf("\n");
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
    if (!items) { perror("malloc"); fclose(fp); return EXIT_FAILURE; }
 
    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d %d", &items[i].weight, &items[i].value);
    }
    fclose(fp);
 
    printf("Capacity W = %d, Items n = %d\n", W, n);
 
    /* Estimate memory before allocating */
    long long tableBytes = (long long)(n + 1) * (W + 1) * sizeof(int);
    printf("Table size estimate: %lld bytes (%.2f MB)\n\n",
           tableBytes, tableBytes / (1024.0 * 1024.0));
 
    int **dp = genTable(n, W);
    if (!dp) {
        fprintf(stderr,
            "FATAL: malloc failed for %lld-byte DP table. "
            "Memory Wall reached at n=%d, W=%d.\n",
            tableBytes, n, W);
        free(items);
        return EXIT_FAILURE;
    }
 
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
 
    computeDP(dp, items, n, W);
 
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed_ms = (ts_end.tv_sec - ts_start.tv_sec) * 1000.0
                      + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e6;
 
    printf("=== 2D DP Result ===\n");
    printf("Max Value   : %d\n", dp[n][W]);
 
    /* Print table only for small instances */
    if (n <= 10 && W <= 20) {
        printTable(dp, items, n, W);
    }
 
    backtrack(dp, items, n, W);
    printf("TIME_MS:%.4f\n", elapsed_ms);
 
    freeTable(dp, n);
    free(items);
    return EXIT_SUCCESS;
}