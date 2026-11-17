"""本番のセンサー取得・LOW・危険履歴を使い、IMU異常と復帰を再現する。"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline-ref')
args = parser.parse_args()
library = root / '.pio/libdeps/m5stack-cores3/Adafruit ADS1X15'
if not (library / 'Adafruit_ADS1X15.h').is_file():
    raise SystemExit('先に pio pkg install -e m5stack-cores3 で依存ライブラリを取得してください。')

with tempfile.TemporaryDirectory(prefix='imu-sensor-') as directory:
    modules = root / 'src/modules'
    sensor_source = modules / 'sensor.cpp'
    if args.baseline_ref:
        sensor_source = Path(directory) / 'sensor.cpp'
        sensor_source.write_bytes(subprocess.check_output(
            ['git', 'show', f'{args.baseline_ref}:src/modules/sensor.cpp'], cwd=root))
    binary = Path(directory) / 'test_imu_sensor'
    subprocess.run([
        os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable',
        f'-I{root / "test/adc_reader/stubs"}', f'-I{library}', f'-I{root / "test/low_warning/stubs"}',
        f'-I{root / "include"}', f'-I{modules}', str(root / 'test/imu_sensor/test_imu_sensor.cpp'),
        str(sensor_source), str(modules / 'adc_reader.cpp'), str(modules / 'low_warning.cpp'),
        str(modules / 'risk_history.cpp'), '-o', str(binary),
    ], check=True)
    tests = subprocess.check_output([binary, '--list'], text=True).splitlines()
    failures = 0
    for test in tests:
        # 本番の静的な校正状態をリセットするため、ケースごとに別プロセスで実行する。
        result = subprocess.run([binary, test], check=False, capture_output=True, text=True, timeout=5)
        failures += result.returncode != 0
        print(f'{test}: {"PASS" if result.returncode == 0 else "FAIL"} {result.stderr.strip()}', flush=True)
    print(f'{len(tests) - failures}/{len(tests)} passed', flush=True)
    raise SystemExit(bool(failures))
