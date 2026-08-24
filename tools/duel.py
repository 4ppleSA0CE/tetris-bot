#!/usr/bin/env python3
"""Paired duel runner: candidate A vs candidate B over mirrored seed pairs.

Each pair plays two games with roles swapped (A takes B's seat and seeds), so seed luck and
the first-mover slot cancel. Deterministic with --nodes; draws score half.

  python3 tools/duel.py --a "" --b "wastedT=-50" --pairs 50 --pieces 2000 --nodes 5200 --workers 10
"""
import argparse, json, math, subprocess
from concurrent.futures import ThreadPoolExecutor

BIN = './build/tetris_bot'


def game(wa, wb, seed_a, seed_b, pieces, nodes):
    cmd = [BIN, '--versus', wb, '--seed', str(seed_a), '--seed2', str(seed_b),
           '--pieces', str(pieces), '--json']
    if wa:
        cmd += ['--weights', wa]
    if nodes:
        cmd += ['--nodes', str(nodes)]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    return json.loads(out.strip().splitlines()[-1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--a', default='', help='candidate A weights ("" = compiled defaults)')
    ap.add_argument('--b', default='', help='candidate B weights')
    ap.add_argument('--pairs', type=int, default=50)
    ap.add_argument('--seed0', type=int, default=40000)
    ap.add_argument('--pieces', type=int, default=2000)
    ap.add_argument('--nodes', type=int, default=5200)
    ap.add_argument('--workers', type=int, default=10)
    args = ap.parse_args()

    jobs = []
    for i in range(args.pairs):
        s1, s2 = args.seed0 + 2 * i, args.seed0 + 2 * i + 1
        jobs.append((args.a, args.b, s1, s2, False))   # A in seat A
        jobs.append((args.b, args.a, s1, s2, True))    # roles swapped, same seats/seeds

    def run(j):
        wa, wb, s1, s2, swapped = j
        r = game(wa, wb, s1, s2, args.pieces, args.nodes)
        w = r['winner']
        if w == 'draw':
            return 'D', r
        a_won = (w == 'A') != swapped
        return ('W' if a_won else 'L'), r

    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        results = list(ex.map(run, jobs))

    W = sum(1 for t, _ in results if t == 'W')
    L = sum(1 for t, _ in results if t == 'L')
    D = sum(1 for t, _ in results if t == 'D')
    n = W + L + D
    score = (W + D / 2) / n
    elo = float('inf') if score in (0.0, 1.0) else 400 * math.log10(score / (1 - score))
    los = 0.5 * (1 + math.erf((W - L) / math.sqrt(2 * (W + L)))) if W + L else 0.5
    rounds = sum(r['rounds'] for _, r in results) / n
    print(f"A: {W}W {L}L {D}D of {n}  score {score:.3f}  elo {elo:+.0f}  "
          f"LOS {los:.3f}  avg rounds {rounds:.0f}")


if __name__ == '__main__':
    main()
