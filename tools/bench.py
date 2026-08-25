#!/usr/bin/env python3
import argparse, json, statistics, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

BIN = './build/tetris_bot'

def run(weights, seed, pieces, garbage, nodes=0, bin_=None):
    cmd = [bin_ or BIN, '--json', '--seed', str(seed), '--pieces', str(pieces)]
    if nodes:
        cmd += ['--nodes', str(nodes)]
    if weights:
        cmd += ['--weights', weights]
    if garbage:
        cmd += ['--garbage', garbage]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    return json.loads(out.strip().splitlines()[-1])

def summarize(rows):
    app = [r['attack'] / r['pieces'] for r in rows]
    n = len(app)
    mean = statistics.fmean(app)
    ci = 1.96 * statistics.stdev(app) / n ** 0.5 if n > 1 else 0.0
    return dict(median=statistics.median(app), mean=mean, ci=ci,
                topouts=sum(r['topouts'] for r in rows), app=app)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--weights', default='', help='candidate weights, name=value,...')
    ap.add_argument('--baseline', default=None, help='baseline weights; "" = compiled defaults')
    ap.add_argument('--seeds', type=int, default=8)
    ap.add_argument('--seed0', type=int, default=1000)
    ap.add_argument('--pieces', type=int, default=3000)
    ap.add_argument('--garbage', default='')
    ap.add_argument('--workers', type=int, default=1, help='>1 shares cores; the anytime clock gets noisier')
    ap.add_argument('--nodes', type=int, default=0, help='deterministic node budget instead of the clock')
    ap.add_argument('--bin', default=BIN, help='candidate binary')
    ap.add_argument('--bin-base', default=None, help='baseline binary (default: same as --bin)')
    a = ap.parse_args()

    seeds = list(range(a.seed0, a.seed0 + a.seeds))

    jobs = []
    for s in seeds:
        jobs.append(('cand', s))
        if a.baseline is not None:
            jobs.append(('base', s))
    with ThreadPoolExecutor(max_workers=a.workers) as ex:
        res = dict(zip(jobs, ex.map(
            lambda j: run(a.weights if j[0] == 'cand' else a.baseline, j[1], a.pieces, a.garbage,
                          a.nodes, a.bin if j[0] == 'cand' else (a.bin_base or a.bin)), jobs)))

    print(f"{'seed':>6} {'app':>7} {'topout':>6} {'spin%':>6} {'b2b':>4} {'height':>6} {'p99':>5} {'nodes':>6}"
          + ('   |  base app  diff' if a.baseline is not None else ''))
    for s in seeds:
        c = res[('cand', s)]
        line = (f"{s:>6} {c['attack']/c['pieces']:7.4f} {c['topouts']:6d} "
                f"{100*c['spins']/c['pieces']:6.2f} {c['maxB2b']:4d} {c['avgHeight']:6.2f} {c['p99']:5.2f} {c['nodes']:6.0f}")
        if a.baseline is not None:
            b = res[('base', s)]
            line += f"   | {b['attack']/b['pieces']:9.4f} {c['attack']/c['pieces']-b['attack']/b['pieces']:+6.4f}"
        print(line)

    cs = summarize([res[('cand', s)] for s in seeds])
    print(f"cand: median {cs['median']:.4f}  mean {cs['mean']:.4f} +- {cs['ci']:.4f}  top-outs {cs['topouts']}")
    if a.baseline is not None:
        bs = summarize([res[('base', s)] for s in seeds])
        print(f"base: median {bs['median']:.4f}  mean {bs['mean']:.4f} +- {bs['ci']:.4f}  top-outs {bs['topouts']}")
        d = [x - y for x, y in zip(cs['app'], bs['app'])]
        dci = 1.96 * statistics.stdev(d) / len(d) ** 0.5 if len(d) > 1 else 0.0
        print(f"paired diff: {statistics.fmean(d):+.4f} +- {dci:.4f}  "
              f"top-outs {cs['topouts']} vs {bs['topouts']}")

if __name__ == '__main__':
    main()
