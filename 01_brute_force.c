/*
 * @subject: CMSC 142 - Design and Analysis of Algorithms
 * @uthor: Myko Jefferson M. Javier and Justin Dayne Bryant Pena
 * @code-desc:
    * 
    * Maximum Knapsack - Brute Force (Bitmask Enumeration)
    * =====================================================
    * Complexity : O(2^n) time, O(n) space
    * Baseline Algorithm
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
    int weight;
    int value;
} Item;

/* ------------------------------------------------------------------ */
/*  Helper – print chosen subset                                        */
/* ------------------------------------------------------------------ */

void printSubset(Item *items, int n, unsigned long long mask) {
    printf("Items chosen (0-indexed): { ");
    for (int i = 0; i < n; i++) {
        if (mask & (1ULL << i)) {
            printf("[w=%d v=%d] ", items[i].weight, items[i].value);
        }
    }
    printf("}\n");
}

/* ------------------------------------------------------------------ */
/*  Brute-force solver                                                  */
/* ------------------------------------------------------------------ */

void bruteForce(Item *items, int n, int W) {
    int         bestValue  = 0;
    int         bestWeight = 0;
    unsigned long long bestMask  = 0;

    unsigned long long total = 1ULL << n;   /* 2^n subsets */

    for (unsigned long long mask = 0; mask < total; mask++) {
        int curWeight = 0;
        int curValue  = 0;

        for (int i = 0; i < n; i++) {
            if (mask & (1ULL << i)) {
                curWeight += items[i].weight;
                curValue  += items[i].value;
            }
        }

        if (curWeight <= W && curValue > bestValue) {
            bestValue  = curValue;
            bestWeight = curWeight;
            bestMask   = mask;
        }
    }

    printf("=== Brute Force Result ===\n");
    printf("Max Value  : %d\n", bestValue);
    printf("Total Weight: %d / %d\n", bestWeight, W);
    printSubset(items, n, bestMask);
    printf("Subsets checked: %llu\n", total);
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
    if (!fp) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int W, n;
    fscanf(fp, "%d", &W);
    fscanf(fp, "%d", &n);

    if (n > 63) {
        fprintf(stderr,
            "Warning: n=%d exceeds 63. Bitmask will overflow — "
            "run will take extremely long or produce wrong results.\n", n);
    }

    Item *items = (Item *)malloc(sizeof(Item) * n);
    if (!items) { perror("malloc"); fclose(fp); return EXIT_FAILURE; }

    for (int i = 0; i < n; i++) {
        fscanf(fp, "%d %d", &items[i].weight, &items[i].value);
    }
    fclose(fp);

    printf("Capacity W = %d, Items n = %d\n\n", W, n);

    bruteForce(items, n, W);

    free(items);
    return EXIT_SUCCESS;
}
