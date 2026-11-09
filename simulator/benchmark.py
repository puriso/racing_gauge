"""同じ60Hz入力で変更前後のSDL描画処理FPSを比較する。"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tarfile
import tempfile

from record import SCENARIOS

ROOT = Path(__file__).resolve().parent.parent


def render_function(source):
    # 本番の描画モジュールと画面遷移だけを差し替え、入力と計測ドライバーは共通にする。
    matches = list(re.finditer(r'^auto renderFrame\(.*?^\}\n', source, re.MULTILINE | re.DOTALL))
    if len(matches) != 1:
        raise ValueError('renderFrameを一意に取得できないため比較を中止します')
    return matches[0]


def build_baseline(directory, baseline_sha):
    archive = directory / 'baseline.tar'
    subprocess.run(['git', 'archive', '--format=tar', f'--output={archive}', baseline_sha], cwd=ROOT, check=True)
    checkout = directory / 'baseline'
    checkout.mkdir()
    with tarfile.open(archive) as source:
        source.extractall(checkout, filter='data')
    baseline_path = checkout / 'simulator' / 'main.cpp'
    baseline_source = baseline_path.read_text()
    current_source = (ROOT / 'simulator/main.cpp').read_text()
    before, after = render_function(baseline_source), render_function(current_source)
    baseline_path.write_text(current_source[:after.start()] + before.group() + current_source[after.end():])
    for name in ('benchmark.hpp', 'platformio.ini', 'build.py'):
        shutil.copyfile(ROOT / 'simulator' / name, checkout / 'simulator' / name)
    dependencies = ROOT / 'simulator/.pio/libdeps/desktop'
    if dependencies.is_dir():
        shutil.copytree(dependencies, checkout / 'simulator/.pio/libdeps/desktop', symlinks=True)
    build(checkout)
    return checkout


def build(checkout):
    subprocess.run([sys.executable, '-m', 'platformio', 'run', '-d', str(checkout / 'simulator'), '-e', 'desktop'], check=True)


def run_once(checkout, scenario, output):
    program = checkout / 'simulator/.pio/build/desktop' / ('program.exe' if os.name == 'nt' else 'program')
    subprocess.run([str(program), '--scenario', scenario, '--seconds', str(SCENARIOS[scenario]['seconds']),
                    '--benchmark', str(output)],
                   env={**os.environ, 'SDL_VIDEODRIVER': 'dummy', 'SDL_RENDER_DRIVER': 'software'},
                   stdout=subprocess.DEVNULL, check=True, timeout=120)
    result = json.loads(output.read_text())
    expected = SCENARIOS[scenario]['seconds'] * 60 + 1
    if (result['scenario'] != scenario or result['trace_frames'] != expected
            or result['warmup_frames'] != 120 or result['measured_frames'] != expected - 120
            or result['render_ns'] <= 0):
        raise ValueError(f'計測フレーム数・時間が不正です: {result}')
    return result


def aggregate(runs):
    frames = sum(run['measured_frames'] for run in runs)
    elapsed = sum(run['render_ns'] for run in runs)
    rates = [run['measured_frames'] * 1_000_000_000 / run['render_ns'] for run in runs]
    return {'measured_frames': frames, 'render_ns': elapsed,
            'average_fps': frames * 1_000_000_000 / elapsed,
            'mean_frame_ms': elapsed / frames / 1_000_000,
            'median_run_fps': statistics.median(rates), 'min_run_fps': min(rates), 'max_run_fps': max(rates),
            'runs': runs}


def compare(before, after):
    before_summary, after_summary = aggregate(before), aggregate(after)
    return {'before': before_summary, 'after': after_summary,
            'change_percent': (after_summary['average_fps'] / before_summary['average_fps'] - 1) * 100}


def host_metadata():
    compiler = subprocess.check_output([os.environ.get('CXX', 'c++'), '--version'], text=True).splitlines()[0]
    cpu = platform.processor() or platform.machine()
    if sys.platform == 'darwin':
        cpu = subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip()
    elif Path('/proc/cpuinfo').is_file():
        model = re.search(r'^model name\s*:\s*(.+)$', Path('/proc/cpuinfo').read_text(), re.MULTILINE)
        if model:
            cpu = model[1]
    return {'system': platform.system(), 'machine': platform.machine(), 'cpu': cpu,
            'logical_cpus': os.cpu_count(), 'compiler': compiler,
            'runner_os': os.environ.get('RUNNER_OS'),
            'sdl_version': subprocess.check_output(['pkg-config', '--modversion', 'sdl2'], text=True).strip()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-ref', default='origin/main')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scenario', choices=['all', *SCENARIOS], default='all')
    parser.add_argument('--repetitions', type=int, default=5)
    args = parser.parse_args()
    if not 1 <= args.repetitions <= 20 or args.output.exists():
        parser.error('繰り返し回数は1〜20、出力先は未作成のファイルを指定してください')
    baseline_sha = subprocess.check_output(['git', 'rev-parse', '--verify', f'{args.baseline_ref}^{{commit}}'], cwd=ROOT, text=True).strip()
    checkout_sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    driver = (ROOT / 'simulator/main.cpp').read_bytes() + (ROOT / 'simulator/benchmark.hpp').read_bytes()
    identical = subprocess.run(['git', 'diff', '--quiet', baseline_sha, '--', 'src', 'include',
                               'simulator/rendering.cpp', 'simulator/compat.hpp', 'simulator/include'], cwd=ROOT).returncode == 0
    scenarios = list(SCENARIOS) if args.scenario == 'all' else [args.scenario]
    results = {name: {'before': [], 'after': []} for name in scenarios}
    build(ROOT)
    with tempfile.TemporaryDirectory(prefix='gauge-fps-') as temporary:
        scratch = Path(temporary)
        baseline = build_baseline(scratch, baseline_sha)
        for repetition in range(args.repetitions):
            for index, name in enumerate(scenarios):
                # 実行順を交互にして、常に後から計測する版だけが有利になることを避ける。
                order = [('before', baseline), ('after', ROOT)]
                if (repetition + index) % 2:
                    order.reverse()
                for variant, checkout in order:
                    raw = run_once(checkout, name, scratch / f'{name}-{repetition}-{variant}.json')
                    results[name][variant].append(raw)
                hashes = {run['trace_hash'] for runs in results[name].values() for run in runs}
                if len(hashes) != 1:
                    raise ValueError(f'変更前後の入力が一致しないため比較を中止します: {name}')
                print(f'{repetition + 1}/{args.repetitions} {name}: 同一入力の計測完了', flush=True)
    rows = []
    for name in scenarios:
        runs = results[name]
        rows.append({'name': name, 'title': SCENARIOS[name]['title'],
                     'trace_hash': runs['before'][0]['trace_hash'], **compare(runs['before'], runs['after'])})
    combined = {}
    for variant in ('before', 'after'):
        combined[variant] = [
            {'measured_frames': sum(results[name][variant][i]['measured_frames'] for name in scenarios),
             'render_ns': sum(results[name][variant][i]['render_ns'] for name in scenarios)}
            for i in range(args.repetitions)]
    report = {'schema_version': 1, 'metric': 'sdl_render_processing_fps',
              'baseline_sha': baseline_sha, 'baseline_ref': args.baseline_ref,
              'current_sha': os.environ.get('SIM_BENCH_CURRENT_SHA', checkout_sha), 'checkout_sha': checkout_sha,
              'driver_sha256': hashlib.sha256(driver).hexdigest(),
              'render_sources_identical': identical, 'repetitions': args.repetitions, 'warmup_frames': 120,
              'host': host_metadata(), 'scenarios': rows, 'overall': compare(combined['before'], combined['after'])}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    overall = report['overall']
    print(f"SDL処理FPS: 変更前 {overall['before']['average_fps']:.2f} / 変更後 {overall['after']['average_fps']:.2f} "
          f"({overall['change_percent']:+.2f}%)")
    print('実機FPSは未測定。描画処理時間のみを実時間計測し、録画・画像保存・動画変換・待機は含めません。')


if __name__ == '__main__':
    main()
