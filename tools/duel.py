#!/usr/bin/env python3
import argparse, json, math, subprocess
from concurrent.futures import ThreadPoolExecutor

BIN = './build/tetris_bot'

def game(wa, wb, seed_a, seed_b, pieces, nodes, nodes_b=0, shape_a=None, shape_b=None):
    cmd = [BIN, '--versus', wb, '--seed', str(seed_a), '--seed2', str(seed_b),
           '--pieces', str(pieces), '--json']
    if wa:
        cmd += ['--weights', wa]
    if nodes:
        cmd += ['--nodes', str(nodes)]
    if nodes_b:
        cmd += ['--nodes2', str(nodes_b)]
    if shape_a:
        cmd += ['--width', str(shape_a[0]), '--depth', str(shape_a[1])]
    if shape_b:
        cmd += ['--width2', str(shape_b[0]), '--depth2', str(shape_b[1])]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    return json.loads(out.strip().splitlines()[-1])

def parse_shape(spec):
    if not spec:
        return None
    w, d = spec.split(',')
    return int(w), int(d)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--a', default='', help='candidate A weights ("" = compiled defaults)')
    ap.add_argument('--b', default='', help='candidate B weights')
    ap.add_argument('--pairs', type=int, default=50)
    ap.add_argument('--seed0', type=int, default=40000)
    ap.add_argument('--pieces', type=int, default=2000)
    ap.add_argument('--nodes', type=int, default=6400)
    ap.add_argument('--nodes-b', type=int, default=0,
                    help='opponent node budget (candidate keeps --nodes): asymmetric depth duel')
    ap.add_argument('--shape-a', default='', help='candidate A beam "width,depth"')
    ap.add_argument('--shape-b', default='', help='candidate B beam "width,depth"')
    ap.add_argument('--workers', type=int, default=10)
    args = ap.parse_args()

    jobs = []
    for i in range(args.pairs):
        s1, s2 = args.seed0 + 2 * i, args.seed0 + 2 * i + 1
        jobs.append((args.a, args.b, s1, s2, False))
        jobs.append((args.b, args.a, s1, s2, True))

    def run(j):
        wa, wb, s1, s2, swapped = j

        na = args.nodes if not swapped else (args.nodes_b or args.nodes)
        nb = (args.nodes_b or args.nodes) if not swapped else args.nodes

        sa = parse_shape(args.shape_a if not swapped else args.shape_b)
        sb = parse_shape(args.shape_b if not swapped else args.shape_a)
        r = game(wa, wb, s1, s2, args.pieces, na, nb if nb != na else 0, sa, sb)
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
