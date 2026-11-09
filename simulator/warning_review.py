"""PR #380の動画で追う流れと、数値テストで確認する境界を整理する。"""

import json
import subprocess
import sys
from pathlib import Path

BASELINE = '77d95d29eeda1ca9cf3f43620cb331691ff509d5'
REVIEWS = {
    'warning-repeat': dict(
        title='再発中もLOWが消えない',
        sequence=['最初の低油圧でLOWが点灯する', '復帰後の保持中に再発しても点滅・消灯しない', '再発したイベントが別の履歴として記録される'],
    ),
    'warning-pending': dict(
        title='保持期限をまたぐ次の判定',
        sequence=['前のLOWを保持している間に次の低油圧が始まる', '前の保持期限で一旦消灯する', '次の開始時刻を失わず、新しい警告が点灯する'],
    ),
    'warning-brief': dict(
        title='短い再発で履歴を上書きしない',
        sequence=['最初の警告と履歴を確認する', '250msの短い再発でも前のLOWの保持が途切れない', '短い再発が終了しても、直近の有効な履歴が変わらない'],
    ),
}
SPOTS = {
    'continuous': ('500ms成立・2999/3000ms保持境界', '動画＋厳密な数値', 'warning-repeat'),
    'interrupted': ('別々の短い低下を500msへ合算しない', '数値で確認', None),
    'repeated': ('保持中の再発を別イベントとして記録', '動画＋数値', 'warning-repeat'),
    'brief': ('短い再発のLOW保持と有効な履歴', '動画＋数値', 'warning-brief'),
    'initial_brief': ('最初の短い低下を履歴へ記録しない', '数値で確認', None),
    'pending': ('旧表示の消去時も次イベントの開始を維持', '動画＋数値', 'warning-pending'),
    'zero': ('開始0ms・499/500ms境界', '数値で確認', None),
    'hold_wrap': ('32ビット折り返しと2999/3000ms保持境界', '数値で確認', None),
    'debounce_wrap': ('32ビット折り返しと499/500ms成立境界', '数値で確認', None),
    'thresholds': ('G=1.0・油圧3.0/3.1bar境界', '数値で確認', None),
}


def run_spot_checks(output):
    root = Path(__file__).resolve().parent.parent
    script = root / 'test/low_warning/run_tests.py'
    after_path, before_path = output / 'after-spot-checks.json', output / 'before-spot-checks.json'
    subprocess.run([sys.executable, str(script), '--report', str(after_path)], check=True)
    # 旧実装の失敗は再現結果として保存し、コンパイル失敗や欠損は成功扱いにしない。
    subprocess.run([sys.executable, str(script), '--baseline-ref', BASELINE, '--report', str(before_path)], check=False)
    before, after = json.loads(before_path.read_text()), json.loads(after_path.read_text())
    before_cases = {case['name']: case for case in before['results']}
    if set(before_cases) != set(SPOTS) or {case['name'] for case in after['results']} != set(SPOTS):
        raise ValueError('数値テストの実行結果が不足しています')
    if not all(case['passed'] for case in after['results']):
        raise ValueError('修正後の数値テストが失敗しています')
    if any(before_cases[key]['passed'] for key in ('repeated', 'brief', 'pending', 'zero', 'hold_wrap')):
        raise ValueError('旧実装で期待した再現結果が得られていません')
    rows = []
    for case in after['results']:
        title, method, scenario = SPOTS[case['name']]
        rows.append(dict(name=case['name'], title=title, method=method, scenario=scenario,
                         before=before_cases[case['name']], after=case))
    result = dict(baseline_ref=BASELINE, clock_model='ESP32 uint32', clock_adaptation=before['clock_adaptation'],
                  before_original_sha256=before['source_original_sha256'], before_sha256=before['source_sha256'],
                  after_sha256=after['source_sha256'], results=rows)
    (output / 'spot-checks.json').write_text(json.dumps(result, ensure_ascii=False), encoding='utf-8')
    return result


def review_spots(name, frames):
    if name not in REVIEWS:
        return []
    def first(predicate, label):
        index = next(i for i, frame in enumerate(frames) if predicate(frame))
        return dict(index=index, label=label)
    spots = [first(lambda f: f['low_visible'], '最初のLOW点灯')]
    if name == 'warning-pending':
        spots += [first(lambda f: f['time_ms'] >= 6000 and f['expected']['warning_state'] == 'pending' and f['low_visible'], '再発判定中・旧LOW保持'),
                  first(lambda f: f['time_ms'] >= 6000 and f['expected']['warning_state'] == 'pending' and not f['low_visible'], '旧LOW消灯・次の判定は継続'),
                  first(lambda f: f['time_ms'] >= 6000 and f['expected']['warning_state'] == 'qualified', '次のLOW点灯')]
    else:
        spots += [first(lambda f: f['time_ms'] >= 4000, '再発開始・LOW保持'),
                  first(lambda f: f['time_ms'] >= (6000 if name == 'warning-repeat' else 4500), '再発後の履歴')]
    spots.append(first(lambda f: f['time_ms'] >= 8000 and not f['low_visible'], '保持終了後の消灯'))
    return spots


def validate_review(name, frames):
    if name not in REVIEWS:
        return
    # 画像から読み取ったLOWと、直近履歴を実際の本番描画・記録値で確認する。
    if name in ('warning-repeat', 'warning-brief'):
        end = 5500 if name == 'warning-repeat' else 4250
        if not all(frame['low_visible'] for frame in frames if 3500 <= frame['time_ms'] <= end):
            raise ValueError('保持中の再発でLOWが一瞬消えています')
    if name == 'warning-brief':
        original = frames[84]['actual']['low_event']
        if not all(frame['actual']['low_event'] == original for frame in frames[96:]):
            raise ValueError('短い再発で直近の有効な履歴が上書きされています')
    if name == 'warning-repeat' and frames[144]['actual']['low_event']['g'] < 1.4:
        raise ValueError('再発イベントが別の履歴として記録されていません')
    review_spots(name, frames)
