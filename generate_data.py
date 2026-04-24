"""
generate_data.py  —  Knapsack Benchmark Dataset Generator
==========================================================
Generates .txt input files for three benchmark configurations:

  Config 1 (N-Scaling)     : Fixed W=1000,  varying n
  Config 2 (W-Scaling)     : Fixed n=100,   varying W
  Config 3 (Greedy Traps)  : Correlated items + classic 3-item trap

Output format (every file):
  Line 1      : n W
  Lines 2..n+1: weight value

Usage:
  python generate_data.py [--seed SEED] [--outdir DIR]

Defaults: seed=42, outdir=./datasets
"""

import os
import random
import argparse


# ──────────────────────────────────────────────────────────────────────────────
#  Helpers
# ──────────────────────────────────────────────────────────────────────────────

def write_dataset(path: str, n: int, W: int, items: list[tuple[int, int]]) -> None:
    """Write a single dataset file.

    Args:
        path  : destination file path
        n     : number of items (must equal len(items))
        W     : knapsack capacity
        items : list of (weight, value) tuples
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(f"{W}\n")
        f.write(f"{n}\n")
        for w, v in items:
            f.write(f"{w} {v}\n")
    print(f"  Written: {path}  (n={n}, W={W})")


def random_items(n: int, rng: random.Random) -> list[tuple[int, int]]:
    """Generate n items with weight in [1,100], value in [10,500]."""
    return [(rng.randint(1, 100), rng.randint(10, 500)) for _ in range(n)]


def correlated_items(n: int, rng: random.Random) -> list[tuple[int, int]]:
    """Generate n correlated items: value = weight + 10."""
    return [(w := rng.randint(1, 100), w + 10) for _ in range(n)]


# ──────────────────────────────────────────────────────────────────────────────
#  Config 1 — N-Scaling  (fixed W=1000, vary n)
# ──────────────────────────────────────────────────────────────────────────────

def generate_n_scaling(outdir: str, rng: random.Random) -> None:
    print("\n[Config 1] N-Scaling  (W=1000, varying n)")
    W = 1000
    ns = [10, 15, 20, 25, 30, 35, 40, 50, 100, 1000]

    for n in ns:
        items = random_items(n, rng)
        path  = os.path.join(outdir, "config1_n_scaling", f"n{n:04d}_W{W}.txt")
        write_dataset(path, n, W, items)


# ──────────────────────────────────────────────────────────────────────────────
#  Config 2 — W-Scaling  (fixed n=100, vary W)
# ──────────────────────────────────────────────────────────────────────────────

def generate_w_scaling(outdir: str, rng: random.Random) -> None:
    print("\n[Config 2] W-Scaling  (n=100, varying W)")
    n  = 100
    Ws = [1_000, 10_000, 100_000, 1_000_000, 50_000_000]

    # Generate items once so the item set is the same across W values —
    # this isolates the effect of W on runtime rather than confounding
    # it with different item populations.
    items = random_items(n, rng)

    for W in Ws:
        path = os.path.join(outdir, "config2_w_scaling", f"n{n:04d}_W{W}.txt")
        write_dataset(path, n, W, items)


# ──────────────────────────────────────────────────────────────────────────────
#  Config 3 — Greedy Traps / Correlated
# ──────────────────────────────────────────────────────────────────────────────

def generate_greedy_traps(outdir: str, rng: random.Random) -> None:
    print("\n[Config 3] Greedy Traps / Correlated")
    subdir = os.path.join(outdir, "config3_greedy_traps")

    # 3a: Large correlated instance — value = weight + 10
    #     Greedy (by v/w density) will rank nearly all items equally,
    #     making it hard to pick the right subset.
    n, W = 1000, 1000
    items = correlated_items(n, rng)
    path  = os.path.join(subdir, f"correlated_n{n}_W{W}.txt")
    write_dataset(path, n, W, items)

    # 3b: Classic 3-item Greedy Trap
    #     Optimal: take both (25,30) items → value 60, weight 50
    #     Greedy:  density of (50,50)=1.0 > (25,30)=1.2  ← actually greedy
    #              picks the two 25/30 items here, so let's use the canonical
    #              trap where a single heavy item looks best by density:
    #
    #     W=10, items: (10, 10), (5, 6), (5, 6)
    #       Greedy density: (5,6)=1.2 → takes two (5,6) → value=12  ✓ optimal
    #
    #     Classic textbook trap (W=50):
    #       Items: [w=50 v=50], [w=25 v=30], [w=25 v=30]
    #       Density: 50/50=1.0,  30/25=1.2,  30/25=1.2
    #       Greedy (desc density): takes [25,30] then [25,30] → value=60 ✓
    #       ...greedy is actually optimal here.
    #
    #     A *true* greedy trap for 0/1 knapsack (W=10):
    #       Items: [w=6 v=6], [w=5 v=5], [w=5 v=5]
    #       Density: 6/6=1.0, 5/5=1.0, 5/5=1.0 — ties, order-dependent
    #
    #     Definitive textbook trap (W=10):
    #       Items: [w=6 v=7], [w=5 v=5], [w=5 v=5]
    #       Density: 7/6≈1.167 > 5/5=1.0
    #       Greedy: takes [6,7] (fits), then [5,5] won't fit → value=7
    #       Optimal: take [5,5]+[5,5] → value=10
    #
    #     For the paper's W=50 version use items that force the trap clearly:
    #       Items: [w=50 v=56], [w=25 v=30], [w=25 v=30]
    #       Density: 56/50=1.12, 30/25=1.20, 30/25=1.20
    #       Greedy: takes [25,30] → [25,30] → value=60 ✓ optimal (no trap)
    #
    #     Best documented trap at W=50:
    #       Items: [w=30 v=40], [w=25 v=30], [w=25 v=30]
    #       Density: 40/30≈1.33 > 30/25=1.20
    #       Greedy: takes [30,40] (fits, rem=20), [25,30] won't fit, [25,30] won't fit → value=40
    #       Optimal: takes [25,30]+[25,30] → value=60  ← GAP = 20
    n_trap, W_trap = 3, 50
    trap_items = [(30, 40), (25, 30), (25, 30)]
    path = os.path.join(subdir, "greedy_trap_classic_n3_W50.txt")
    write_dataset(path, n_trap, W_trap, trap_items)

    # 3c: Scaled greedy trap — harder version with more items
    #     Each "trap group": one dense-but-large item vs two smaller items
    #     that together are more valuable.  Repeated 10 times.
    trap_group = [(30, 40), (25, 30), (25, 30)]  # same pattern
    n_scaled   = len(trap_group) * 10             # 30 items
    W_scaled   = 50 * 10                          # 500
    scaled_items = trap_group * 10
    path = os.path.join(subdir, f"greedy_trap_scaled_n{n_scaled}_W{W_scaled}.txt")
    write_dataset(path, n_scaled, W_scaled, scaled_items)


# ──────────────────────────────────────────────────────────────────────────────
#  Entry point
# ──────────────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate knapsack benchmark datasets."
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducibility (default: 42)"
    )
    parser.add_argument(
        "--outdir",
        type=str,
        default="datasets",
        help="Output directory for dataset files (default: ./datasets)"
    )
    args = parser.parse_args()

    rng = random.Random(args.seed)
    print(f"Seed     : {args.seed}")
    print(f"Output   : {os.path.abspath(args.outdir)}")

    generate_n_scaling(args.outdir, rng)
    generate_w_scaling(args.outdir, rng)
    generate_greedy_traps(args.outdir, rng)

    print("\nDone. All datasets written.")


if __name__ == "__main__":
    main()