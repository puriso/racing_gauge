"""筑波専用テスト: 60Hz取得・連続した周回・アクセル/油圧/油温の関係。"""
import math

from timing import FRAME_RATE, SAMPLE_RATE


def validate_tsukuba(metadata):
    frames = metadata['frames']
    at = lambda second: frames[round(second * FRAME_RATE)]
    if metadata.get('sample_hz') != SAMPLE_RATE:
        raise RuntimeError('筑波テストの取得周期は60Hzです')
    oils = [frame['target']['oil_c'] for frame in frames]
    if abs(oils[0] - 120) > 0.001 or abs(oils[-1] - 140) > 0.001 or oils != sorted(oils):
        raise RuntimeError('油温は全記録を通して120→140℃へ少しずつ上げます')
    if abs(at(180)['sensor']['oil_c'] - 140) > 1:
        raise RuntimeError('本番の温度取得で140℃まで到達していません')
    if any(not f.get('track', {}).get('synthetic') for f in frames):
        raise RuntimeError('合成データであることを全フレームに明示します')
    focus = [f for f in frames if 60000 <= f['time_ms'] < 140000]
    sections = list(dict.fromkeys(f['track']['section'] for f in focus))
    if sections != ['HOME STRAIGHT', 'TURN 1', 'S CURVES', 'HAIRPIN 1', 'DUNLOP',
                    '80R / 170R (MAX LOAD)', 'HAIRPIN 2', 'BACK STRAIGHT', 'FINAL / HOME']:
        raise RuntimeError('注目する一周の区間順序が不正です')
    peak = max(focus, key=lambda frame: frame['g'])
    if not 97000 <= peak['time_ms'] <= 102000 or not 1.95 <= peak['g'] <= 2.001 or peak['g_cardinal'] != 3:
        raise RuntimeError('ダンロップ先を最大約2G・左方向にします')

    columns = metadata['sample_columns']
    samples = [dict(zip(columns, row)) for row in metadata['raw_samples']]
    if len(samples) != 180 * SAMPLE_RATE + 1:
        raise RuntimeError('60Hzの生取得データが全区間分必要です')
    for second in range(2, 180):
        block = samples[second * SAMPLE_RATE:(second + 1) * SAMPLE_RATE]
        magnitudes = [math.hypot(s['ax'] - 0.125, s['ay'] + 0.0625) for s in block]
        if max(magnitudes) - min(magnitudes) < 0.003:
            raise RuntimeError(f'Gが長時間固定されています: {second}秒')
    # 減速の入り口で全開からオフへ変わり、直前の油圧から約2bar低下する。
    for second in [63, 78, 90, 99.6, 107, 127.5]:
        before = samples[round(second * SAMPLE_RATE)]['target_pressure_bar']
        after = samples[round((second + 0.1) * SAMPLE_RATE)]['target_pressure_bar']
        if not 1.8 <= before - after <= 2.3:
            raise RuntimeError(f'全開→オフの約2bar低下がありません: {second}秒 {before-after:.3f}bar')

    fast, slow = [], []
    for a, b in zip(frames, frames[1:]):
        dt = (b['sample_index'] - a['sample_index']) / SAMPLE_RATE
        rate = (b['target']['oil_c'] - a['target']['oil_c']) / dt
        throttle = (a['track']['throttle'] + b['track']['throttle']) / 2
        if throttle > 0.95:
            fast.append(rate)
        if throttle < 0.05:
            slow.append(rate)
    if not fast or not slow or sum(fast) / len(fast) < 2 * sum(slow) / len(slow):
        raise RuntimeError('アクセルを開けた区間で油温の上昇が速くなっていません')

    for frame in frames:
        if frame['g'] > 2.001:
            raise RuntimeError('Gが上限2を超えています')
        if not 99.7 <= frame['time_ms'] / 1000 <= 100.85:
            target, track = frame['target'], frame['track']
            t = frame['sample_index'] / SAMPLE_RATE
            expected = 4 + 2 * track['throttle'] - 0.15 * frame['g'] - 0.003 * (target['oil_c'] - 120) + 0.03 * math.sin(t * 1.4)
            if abs(expected - target['pressure_bar']) > 0.0005:
                raise RuntimeError('油圧がアクセル主体のモデルと一致しません')

    # LOW判定自体は、全取得の本番状態と独立モデルの照合で確認する。
    qualified = any(f['actual']['warning_history'] for f in frames)
    event = at(168)['risk']
    sustained = metadata['scenario'] == 'tsukuba-low'
    if sustained and (not qualified or event['kind'] != 3):
        raise RuntimeError('継続低下の比較ケースでLOWが成立していません')
    if not sustained and (qualified or event['kind'] not in (1, 2)):
        raise RuntimeError('短い低下はLOW未成立の予兆として記録します')
    if not 99000 <= event['at_ms'] <= 102000 or event['g_direction'] != 3:
        raise RuntimeError('ダンロップ先を代表の危険点に記録できません')
    if event['pre_ms'] != 60000 or event['post_ms'] != 60000 or event['age_ms'] < 60000:
        raise RuntimeError('前後60秒の完成グラフと何分前かを確認できません')
    if at(168)['screen'] != 1 or at(170)['screen'] != 2 or at(174)['risk']['at_ms'] != event['at_ms']:
        raise RuntimeError('走行後のグラフ・詳細・記録保持が不正です')
    print(f"{metadata['scenario']}: 60Hz continuous G / throttle-off -2bar / oil 120->140C with throttle / LOW={qualified}")
