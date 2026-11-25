#!/usr/bin/env python3
"""S31 release gate: run every host suite except the documented exclusions.

中文：发布前的 S31 主机测试关口。在板级测试视图中运行全部 Python/shell 主机测试，
跳过 tools/s31_suite_manifest.json 中注明原因的项；需在仓库根运行的测试单独执行。
任何未列入清单的失败都会使退出码非零；已修好但仍在清单中的项会被提示。
English: build the S31 board test view, run every Python/shell host suite except the
documented exclusions in tools/s31_suite_manifest.json, and run repository-root tests
separately. Any unlisted failure exits non-zero; listed suites that now pass are reported.
"""
from pathlib import Path
import argparse
import concurrent.futures
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOT_TESTS = ('firmware_layout_host_test.py', 'release_full_only_host_test.py', 'firmware_editions_host_test.py')


def run(cmd, cwd, log, timeout):
    try:
        with open(log, 'w') as out:
            return subprocess.run(cmd, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, timeout=timeout).returncode == 0
    except subprocess.TimeoutExpired:
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--view', type=Path, default=ROOT / 'build/s31-suites')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--no-regen', action='store_true', help='Reuse an existing test view')
    parser.add_argument('--check-listed', action='store_true', help='Also run excluded suites to find fixed ones')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'tools/s31_suite_manifest.json').read_text())['excluded']
    view = args.view.resolve()
    if not args.no_regen:
        shutil.rmtree(view, ignore_errors=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/board_source_view.py'), '--board', 'metalio47',
                        '--tests', '--output', str(view)], check=True, stdout=subprocess.DEVNULL)
    # 中文：部分测试在 build/ 下建临时目录。/ English: some suites create scratch folders under build/.
    (view / 'build').mkdir(parents=True, exist_ok=True)
    logs = view / 'build/suite-logs'
    logs.mkdir(parents=True, exist_ok=True)
    python_tests = sorted({p.name for p in (view / 'tools').glob('*host_test.py')} |
                          {p.name for p in (view / 'tools').glob('test_*.py')})
    # 中文：排除本关口自身的 shell 包装，否则会递归调用。/ English: exclude this gate's own shell wrapper, which would recurse.
    shell_tests = sorted(p.name for p in (view / 'tools').glob('run_*.sh') if p.name != 'run_s31_release_suites.sh')
    results = {}

    def selected(name):
        return name not in ROOT_TESTS and (args.check_listed or name not in manifest)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        jobs = {pool.submit(run, [sys.executable, 'tools/' + n], view, logs / (n + '.log'), 900): n
                for n in python_tests if selected(n)}
        for job in concurrent.futures.as_completed(jobs):
            results[jobs[job]] = job.result()
    # 中文：shell 汇总脚本共享 build/ 输出，顺序执行。/ English: shell runners share build/ outputs; run them in order.
    for n in shell_tests:
        if selected(n):
            results[n] = run(['bash', 'tools/' + n], view, logs / (n + '.log'), 1800)
    for n in ROOT_TESTS:
        results[n] = run([sys.executable, 'tools/' + n], ROOT, logs / (n + '.log'), 900)

    unexpected = sorted(n for n, ok in results.items() if not ok and n not in manifest)
    fixed = sorted(n for n, ok in results.items() if ok and n in manifest and manifest[n]['category'] != 'repo_root')
    skipped = sorted(n for n in (*python_tests, *shell_tests) if n not in results)
    for n in sorted(results):
        print(('PASS ' if results[n] else 'FAIL ') + n)
    for n in skipped:
        print('SKIP %s (%s: %s)' % (n, manifest[n]['category'], manifest[n]['reason']))
    passed = sum(results.values())
    print(f'\nS31 suites: {passed} passed, {len(unexpected)} unexpected failures, {len(skipped)} excluded; logs in {logs}')
    if fixed:
        print('Listed as excluded but now passing (remove from tools/s31_suite_manifest.json): ' + ', '.join(fixed))
    if unexpected:
        print('UNEXPECTED FAILURES: ' + ', '.join(unexpected))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
