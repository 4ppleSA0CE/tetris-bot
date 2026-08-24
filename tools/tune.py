#!/usr/bin/env python3
"""Noisy cross-entropy weight tuner (Szita & Lorincz 2006; Thiery & Scherrer 2009).

Every candidate in a generation plays the same seeds (common random numbers), redrawn each
generation, in two regimes: solo and under --garbage. Fitness is lexicographic: fewer top-outs
first, then more attack. attackDealt stays pinned at its default (the unit anchor). The search
runs at the shipped time budget, or at --nodes N, a deterministic node count calibrated to it.

  python3 tools/tune.py --gens 30 --pop 60 --elite 8 --pieces 1500 --workers 8
"""
import argparse, json, os, random, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

BIN = './build/tetris_bot'
PINNED = {'attackDealt'}


def list_weights():
    out = subprocess.run([BIN, '--list-weights'], capture_output=True, text=True, check=True).stdout
    return {k: float(v) for k, v in (l.split('=') for l in out.split())}


def run(weights, seed, pieces, garbage, nodes=0):
    cmd = [BIN, '--json', '--seed', str(seed), '--pieces', str(pieces), '--weights', weights]
    if garbage:
        cmd += ['--garbage', garbage]
    if nodes:
        cmd += ['--nodes', str(nodes)]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    return json.loads(out.strip().splitlines()[-1])


def fmt(names, vec):
    return ','.join(f'{n}={v:.1f}' for n, v in zip(names, vec))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gens', type=int, default=30)
    ap.add_argument('--pop', type=int, default=60)
    ap.add_argument('--elite', type=int, default=8)
    ap.add_argument('--pieces', type=int, default=1500)
    ap.add_argument('--garbage', default='4/16')
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--nodes', type=int, default=0,
                    help='deterministic node budget per search, calibrated to the shipped 4.5 ms '
                         '(read "nodes" from a sequential --json run); 0 = wall clock')
    ap.add_argument('--seed0', type=int, default=20000)
    ap.add_argument('--sigma-scale', type=float, default=0.5, help='initial sigma = max(scale*|w|, 5)')
    ap.add_argument('--noise', type=float, default=0.05, help='constant variance floor: (noise*sigma0)^2')
    ap.add_argument('--init', default='', help='start mean, name=value,... (default: compiled)')
    ap.add_argument('--out', default='build/tune')
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    defaults = list_weights()
    if a.init:
        for tok in a.init.split(','):
            k, v = tok.split('=')
            defaults[k] = float(v)
    names = [n for n in defaults if n not in PINNED]
    mean = [defaults[n] for n in names]
    sigma0 = [max(a.sigma_scale * abs(m), 5.0) for m in mean]
    sigma = list(sigma0)
    floor = [(a.noise * s) ** 2 for s in sigma0]
    rng = random.Random(a.seed0)

    for gen in range(a.gens):
        pop = [[rng.gauss(m, s) for m, s in zip(mean, sigma)] for _ in range(a.pop)]
        pop[0] = list(mean)                     # the incumbent always competes
        seeds = [a.seed0 + gen * 10 + 1, a.seed0 + gen * 10 + 2]
        jobs = [(i, r) for i in range(a.pop) for r in (0, 1)]

        def job(ir):
            i, r = ir
            return run(fmt(names, pop[i]), seeds[r], a.pieces, a.garbage if r else '', a.nodes)

        with ThreadPoolExecutor(max_workers=a.workers) as ex:
            res = list(ex.map(job, jobs))
        fit = []
        for i in range(a.pop):
            solo, pres = res[2 * i], res[2 * i + 1]
            fit.append((solo['topouts'] + pres['topouts'], -(solo['attack'] + pres['attack']),
                        solo['attack'], pres['attack']))
        order = sorted(range(a.pop), key=lambda i: fit[i][:2])
        elite = order[:a.elite]
        mean = [sum(pop[i][k] for i in elite) / a.elite for k in range(len(names))]
        var = [sum((pop[i][k] - mean[k]) ** 2 for i in elite) / a.elite for k in range(len(names))]
        sigma = [(v + f) ** 0.5 for v, f in zip(var, floor)]

        best = order[0]
        print(f"gen {gen:02d} seeds {seeds} best top-outs {fit[best][0]} attack "
              f"{fit[best][2]}+{fit[best][3]}  elite-avg attack "
              f"{sum(-fit[i][1] for i in elite) / a.elite:.0f}  incumbent rank {order.index(0)}")
        print(f"  mean: {fmt(names, mean)}")
        sys.stdout.flush()
        with open(os.path.join(a.out, f'gen{gen:02d}.json'), 'w') as f:
            json.dump(dict(gen=gen, seeds=seeds, names=names, mean=mean, sigma=sigma,
                           best=dict(weights=pop[best], fitness=fit[best]),
                           elites=[dict(weights=pop[i], fitness=fit[i]) for i in elite]), f)
    print('final:', fmt(names, mean))


if __name__ == '__main__':
    main()
