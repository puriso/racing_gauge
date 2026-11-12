"""筑波2000を想定した合成一周を、生入力と本番監視結果から検証する。"""
import math

from timing import FRAME_RATE


def validate_tsukuba(metadata):
    frames = metadata['frames']
    lap = [f for f in frames if 60000 <= f['time_ms'] <= 140000]
    at = lambda seconds: frames[round(seconds * FRAME_RATE)]
    oils = [f['target']['oil_c'] for f in lap]
    if not math.isclose(oils[0], 120, abs_tol=0.0001) or not math.isclose(oils[-1], 140, abs_tol=0.0001):
        raise RuntimeError('筑波一周の油温は120→140℃が必要です')
    if oils != sorted(oils) or any(b - a > 0.011 for a, b in zip(oils, oils[1:])):
        raise RuntimeError('油温が一周を通して徐々に上がっていません')
    if abs(at(60)['sensor']['oil_c'] - 120) > 1 or abs(at(142)['sensor']['oil_c'] - 140) > 1:
        raise RuntimeError('生ADCからの油温が120→140℃へ到達していません')
    if any(not f.get('track', {}).get('synthetic') for f in frames):
        raise RuntimeError('実測ではない合成データであることを明示してください')
    sections = list(dict.fromkeys(f['track']['section'] for f in lap))
    if sections != ['HOME STRAIGHT', 'TURN 1', 'S CURVES', 'HAIRPIN 1', 'DUNLOP',
                    '80R / 170R (MAX LOAD)', 'HAIRPIN 2', 'BACK STRAIGHT', 'FINAL / HOME']:
        raise RuntimeError('筑波2000の区間順序が不正です')
    peak = max(lap, key=lambda f: f['g'])
    if not 97000 <= peak['time_ms'] <= 107000 or not 1.99 <= peak['g'] <= 2.001 or peak['g_cardinal'] != 3:
        raise RuntimeError('最大2G・左方向のピークがダンロップ先にありません')
    for f in lap:
        target = f['target']
        if not 0 <= f['g'] <= 2.001 or not 2.39 <= target['pressure_bar'] <= 6.001:
            raise RuntimeError('筑波想定のG・油圧範囲を外れています')
        if abs(f['time_ms'] - 100000) > 1000:
            t = f['time_ms'] / 1000
            base = 5.8 - 0.008 * (target['oil_c'] - 120) + 0.15 * math.sin(t * 0.86) + 0.08 * math.sin(t * 2.99)
            expected = max(3.1, min(6, base - 0.5 * f['g']))
            if abs(expected - target['pressure_bar']) > 0.0003:
                raise RuntimeError('通常油圧がGに応じた小さな低下になっていません')
    # monitorの並びは油温・水温・油圧。
    below = [f for f in lap if f['actual']['monitor'][2] <= 3 and f['g'] > 1]
    if not below or any(not 99000 <= f['time_ms'] <= 102000 for f in below):
        raise RuntimeError('ダンロップ先だけで実際の監視油圧がLOW閾値を割る必要があります')
    duration = (below[-1]['time_ms'] - below[0]['time_ms'] + 1000 / FRAME_RATE) / 1000
    qualified = any(f['actual']['warning_history'] for f in lap)
    event = at(168)['risk']
    sustained = metadata['scenario'] == 'tsukuba-low'
    if (sustained and (duration < 0.5 or not qualified or event['kind'] != 3)):
        raise RuntimeError('継続低下の比較ケースでLOWが成立していません')
    if not sustained and (duration >= 0.5 or qualified or event['kind'] != 2):
        raise RuntimeError(f'一瞬の閾値割れはLOW未成立・NEAR記録で確認します: {duration:.3f}s, {event}')
    if not 99000 <= event['at_ms'] <= 102000 or event['g_direction'] != 3:
        raise RuntimeError('最悪点をダンロップ先の左高速区間に記録できていません')
    if event['pre_ms'] != 60000 or event['post_ms'] != 60000 or event['age_ms'] < 60000:
        raise RuntimeError('前後60秒の完成グラフと何分前かを確認できません')
    if at(168)['screen'] != 1 or at(170)['screen'] != 2 or at(174)['risk']['at_ms'] != event['at_ms']:
        raise RuntimeError('走行後のグラフ・詳細・記録保持が不正です')
    print(f"{metadata['scenario']}: 80R max={peak['g']:.2f}G / pressure<=3bar {duration:.3f}s / LOW={qualified} / oil 120->140C")
