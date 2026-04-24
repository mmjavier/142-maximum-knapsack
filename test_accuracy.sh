#!/usr/bin/env bash

echo "====================================="
echo " Compiling the 5 Algorithms..."
echo "====================================="
gcc -O3 -Wall -o brute_force 01_brute_force.c
gcc -O3 -Wall -o dp_2d 02_dp_2d.c
gcc -O3 -Wall -o dp_1d 03_dp_1d.c
gcc -O3 -Wall -o greedy 04_greedy.c
gcc -O3 -Wall -o branch_bound 05_branch_bound.c

echo "Compilation Done!"
echo ""
echo "====================================="
echo " Running Tests for Capacity 10, N=4"
echo "====================================="

echo "--- 1. Brute Force ---"
./brute_force test_input.txt
echo ""

echo "--- 2. DP 2D ---"
./dp_2d test_input.txt
echo ""

echo "--- 3. DP 1D ---"
./dp_1d test_input.txt
echo ""

echo "--- 4. Greedy ---"
./greedy test_input.txt
echo ""

echo "--- 5. Branch and Bound ---"
./branch_bound test_input.txt
echo ""

echo "Done! Compare the results above to check for correct answers."
