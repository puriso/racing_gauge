"""本番の SDL 描画を録画し、CI でダウンロードできる目視確認用レポートを作る。"""

import argparse
import hashlib
import html
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone

import imageio_ffmpeg
from PIL import Image, ImageDraw

from adc_steps import validate_adc_steps
from adc_bus_reference import validate_communication
from annotations import SIZE, annotated_frame
from warning_review import REVIEWS, run_spot_checks, review_spots, validate_review
from fault_cases import CASES, validate_fault_inputs
from reference import add_expectations, check_expectations
from timing import FRAME_RATE, frame_at_or_after, frame_time_ms

ROOT = Path(__file__).resolve().parent
SCENARIOS = {
    "negative-maxima": {
        "title": "負温度の最高値・未取得・異常保持・0℃越え", "group": "reproduction",
        "description": "最初の2秒は温度異常でMAX未取得。負温度の初回取得、より高い負温度への更新、再冷却・異常時の保持を詳細画面でも確認。19秒から正温度へ移り、0℃を越えた最高値更新も追う",
        "seconds": 24, "snapshots": [0, 1.5, 3, 5, 7.8, 8.5, 11, 13.5, 16.5, 18, 20, 22.5, 24],
    },
    "menu-monitoring": {
        "title": "詳細画面中のLOW・最高値とR復帰", "group": "reproduction",
        "description": "3秒にR開始、4〜12秒は詳細画面。6〜9秒のLOWと8〜11秒の温度・油圧ピークを表示中も記録。高Gのまま戻ってR再表示、17秒の200ms切断後も最高値を保持する",
        "seconds": 28, "snapshots": [3.2, 4, 6, 7, 9.8, 11, 12, 12.2, 17, 17.25, 18.2, 28],
    },
    "fault-monitoring": {
        "title": "通信エラー・信号断線と正常な0barの区別", "group": "reproduction",
        "description": "2G中に200msのADC切断を注入し、ERRと全チャンネル無効化・最高値保持・LOW誤検知なしを確認。12〜15秒は正常な0barでLOW、15秒に200℃超、16秒に油圧0Vを入力し、ERRからの復帰を比較",
        "seconds": 20, "snapshots": [3, 4, 4.2, 5.2, 8, 12, 13, 15.6, 16, 18.6, 20],
    },
    "warning-history": {
        "title": "LOW発生履歴W・保持・画面復帰・リセット・再発", "group": "pressure",
        "description": "LOW成立で左下Wが点灯。LOW消灯後もWを保持。8秒の詳細画面で履歴を確認し、10秒にゲージへ戻るとWを消去。再発で再点灯する",
        "seconds": 20, "snapshots": [0, 2.625, 7, 8, 10, 12, 14.625, 19],
    },
    "warning-repeat": {
        "title": "PR #380・保持中の再発", "group": "reproduction",
        "description": "最初のLOW点灯→復帰・保持→再発。再発を判定する500msの間もLOWが消えず、次の履歴が別イベントとして残ることを等速で追う修正後の再現動画",
        "seconds": 12, "snapshots": [0, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 8.5, 12],
    },
    "warning-pending": {
        "title": "PR #380・保持期限をまたぐ再発判定", "group": "reproduction",
        "description": "旧LOWの保持期限直前に再発。旧表示の消灯後も次の開始時刻を維持し、所定の継続判定で次のLOWが点灯する流れを追う",
        "seconds": 12, "snapshots": [0, 2, 3, 3.5, 6, 6.25, 6.5, 6.75, 7, 7.5, 8, 11, 12],
    },
    "warning-brief": {
        "title": "PR #380・短い再発と警告履歴", "group": "reproduction",
        "description": "最初の警告・履歴を作った後に250msだけ再発。LOWの保持が途切れず、直近の有効なG・最低油圧・継続時間が短い低下で上書きされないことを確認",
        "seconds": 12, "snapshots": [0, 2, 3, 3.5, 4, 4.25, 4.5, 5, 6.5, 8, 12],
    },
    "adc-startup": {
        "title": "ADC起動時未接続・1秒間隔の再接続と復帰", "group": "raw",
        "description": "先頭はsetup直後のADS1015初期化失敗画面。初回loopで通常描画へ移り、5秒まで未接続、再接続・正常取得まで確認。初期画面は0秒ボタンと静止画像で確認",
        "seconds": 16, "snapshots": [0, 0.041, 1.1, 4, 5, 5.1, 6, 8, 16],
    },
    "adc-communication": {
        "title": "ADC通信異常・タイムアウト・再接続と復帰", "group": "raw",
        "description": "動作中の切断、変換完了しない状態、設定書込・状態読取・結果読取・本取得の失敗を順に注入。本番adc_readerの5ms期限・1秒再接続と、温度バッファ初期化・正常復帰を確認",
        "seconds": 56, "snapshots": [0, 3, 4, 7, 8, 11, 12, 15, 16, 19, 20, 23, 27, 28, 31, 35, 36, 39, 43, 44, 47, 48, 51, 55, 56],
    },
    "sensor-disconnect": {
        "title": "油温・水温・油圧の信号断線と復帰", "group": "raw",
        "description": "油温・水温の断線相当（5V入力）、油圧信号なし相当（0V）、3本同時、復帰を確認。油圧の断線電圧は配線次第のため、0Vに落ちた条件を模擬",
        "seconds": 40, "snapshots": [0, 4, 4.6, 8, 10, 12, 12.6, 16, 18, 20, 21, 24, 26, 28, 29, 32, 36, 40],
    },
    "temperature-high": {
        "title": "温度異常・200℃以上と復帰", "group": "raw",
        "description": "生ADC25→24（約197.57→200.30℃）を油温・水温へ順に入力し、ADC1（約583.29℃）も確認。現行コードの199℃判定・平均化・復帰の挙動を表示",
        "seconds": 40, "snapshots": [0, 4.6, 7, 8, 8.6, 10, 12, 15, 16.6, 19, 20.6, 22, 24, 27, 28.6, 30, 32, 36, 40],
    },
    "temperature-low": {
        "title": "PDF00903Sの動作下限・-20℃前後と復帰", "group": "raw",
        "description": "PDF00903Sの仕様上の動作下限-20℃を基準に、生ADC1429→1430（約-19.93→-20.04℃）を順に入力。範囲内・範囲外・同時入力・復帰を確認。換算温度はリポジトリのNTCモデルによる参考値",
        "seconds": 40, "snapshots": [0, 4.6, 7, 8, 8.6, 10, 12, 15, 16.6, 19, 20.6, 22, 24, 27, 28.6, 30, 32, 36, 40],
    },
    "adc-steps": {
        "title": "生ADCを1カウントずつ増減", "group": "raw",
        "description": "CH0・CH1・CH2を温度から逆算せず直接0→2047→0へ毎フレーム1カウントずつ増減。専用の保持処理は設けず、本番の取得周期・変換・平均化をそのまま確認",
        "seconds": 173, "snapshots": [0, 2, 2.041, 3, 6, 12, 25, 41, 52, 68, 70, 87.291, 87.333, 120, 160, 172.583, 173],
    },
    "circuit": {
        "title": "サーキット・2.0Gと急な油圧低下", "group": "pressure",
        "description": "通常は3〜6barを連続往復。瞬間的な2.0Gと0.8barへの急落、継続低下、LOW点灯・保持・回復を確認",
        "seconds": 36, "snapshots": [0, 4, 4.1, 8, 9.5, 10.5, 14, 18.1, 23.5, 28, 36],
    },
    "racing": {
        "title": "Rモード・進入から3分後の解除まで", "group": "racing",
        "description": "50msの2.0Gでは非進入。1.0G継続で進入し、G解除後も3分保持。再進入と詳細画面での解除も等速で確認",
        "seconds": 196, "snapshots": [0, 2, 2.1, 4.2, 5, 184, 184.3, 187.3, 190, 192, 196],
    },
    "ambient": {
        "title": "照度・昼→夕方→夜→昼と詳細画面", "group": "ambient",
        "description": "受光100→12→2→100lux。8秒周期・6点中央値による調光、夜間の詳細画面での最大輝度と復帰を確認",
        "seconds": 136, "snapshots": [0, 16, 24, 48, 64, 88, 96, 98, 104, 120, 136],
    },
    "raw-faults": {
        "title": "生ADCの0・上限・過電圧と復帰", "group": "raw",
        "description": "油温CH0=0、水温CH1=2047、油圧CH2=1633、全ADC=0を順に入力。変換・取得周期・異常判定・回復を本番処理で確認",
        "seconds": 24, "snapshots": [0, 0.6, 4, 4.6, 6.6, 8.6, 12, 13, 16.6, 18.6, 24],
    },
    "ramp": {
        "title": "温度の上昇→保持→下降", "group": "temperature",
        "description": "油温-10→170→-10℃・水温-10→130→-10℃。上昇後、そのまま下降。油圧は3〜6barを連続往復",
        "seconds": 200, "snapshots": [0, 2, 50, 92, 100, 145, 190, 200],
    },
    "cooldown": {
        "title": "高温からの降温・赤表示の解除", "group": "temperature",
        "description": "油温170→-10℃・水温130→-10℃を90秒で下降。バーの消去・赤色からの復帰・低温端を確認",
        "seconds": 100, "snapshots": [0, 2, 25, 50, 75, 92, 100],
    },
    "pressure-sweep": {
        "title": "油圧0→10→0bar", "group": "pressure",
        "description": "専用の全レンジ確認。0→10→0barを往復し、上昇・下降時の弧と8barの赤色境界を確認",
        "seconds": 20, "snapshots": [0, 4, 8, 10, 12, 16, 20],
    },
    "warnings": {
        "title": "LOW境界・短時間低下と継続低下", "group": "pressure",
        "description": "3〜6barの変動中、3.1/3.0/2.9barへ各250ms低下。2.9barを2秒保持しLOW点灯と回復を確認",
        "seconds": 40, "snapshots": [0, 8.1, 12.1, 16.2, 23.5, 24.2, 27.5, 30, 40],
    },
    "startup": {
        "title": "始動直後70℃→低温→暖機", "group": "operation",
        "description": "最初の2秒は70℃相当のADCを保持し、IMU補正後の取得を確認。その後-10℃相当へ戻して暖機",
        "seconds": 30, "snapshots": [0, 1, 1.5, 2, 3, 10, 30],
    },
    "warm-start": {
        "title": "70℃からの暖機", "group": "operation",
        "description": "油温・水温とも70℃から始動し、そのままゆっくり上昇",
        "seconds": 30, "snapshots": [0, 0.5, 1, 10, 20, 30],
    },
    "menu": {
        "title": "詳細画面と通常表示への復帰", "group": "operation",
        "description": "油温・水温がゆっくり上昇。8〜13秒は詳細画面、通常画面への復帰を確認",
        "seconds": 20, "snapshots": [0, 4, 8, 12, 16, 20],
    },
}


def running_pressure(seconds):
    phase = (seconds % 4) / 2
    return 3 + 3 * (phase if phase <= 1 else 2 - phase)


def validate_inputs(metadata):
    validate_communication(metadata)
    if metadata['scenario'] in ('adc-startup', 'adc-communication', 'menu-monitoring', 'fault-monitoring', 'negative-maxima'):
        return
    if metadata["scenario"] in CASES:
        validate_fault_inputs(metadata)
        return
    if metadata["scenario"] == "adc-steps":
        validate_adc_steps(metadata["frames"])
        return
    # 目標はADC波形を生成するためだけの値。実際の取得値は別途独立モデルと照合する。
    frames = [{**frame, **frame['target']} for frame in metadata["frames"]]
    scenario = metadata["scenario"]
    for index, frame in enumerate(frames):
        for key, low, high in (("oil_c", -10, 170), ("water_c", -10, 130), ("pressure_bar", 0, 10)):
            if not low <= frame[key] <= high:
                raise RuntimeError(f"入力が検証範囲外です: {scenario}, {key}, {frame}")
        if not 0 <= frame["g"] <= 2:
            raise RuntimeError(f"G入力が0〜2.0Gの範囲外です: {frame}")
        if not 0 <= frame["lux"] <= 100:
            raise RuntimeError(f"照度入力が0〜100luxの範囲外です: {frame}")
        seconds = index / FRAME_RATE
        ordinary = scenario not in ("pressure-sweep", "warnings", "circuit", "warning-history", *REVIEWS) and seconds >= 1
        if ordinary and not math.isclose(frame["pressure_bar"], running_pressure(seconds), abs_tol=0.0001):
            raise RuntimeError(f"油圧が3〜6barで連続変動していません: {frame}")
    if scenario == "ramp":
        for key, start, peak in (("oil_c", -10, 170), ("water_c", -10, 130)):
            values = [frame[key] for frame in frames]
            if (values[0] != start or values[-1] != start or max(values) != peak
                    or values[:100 * FRAME_RATE + 1] != sorted(values[:100 * FRAME_RATE + 1])
                    or values[100 * FRAME_RATE:] != sorted(values[100 * FRAME_RATE:], reverse=True)):
                raise RuntimeError(f"上昇後、そのまま下降する温度入力が不正です: {key}")
    if scenario == "cooldown":
        for key, start in (("oil_c", 170), ("water_c", 130)):
            values = [frame[key] for frame in frames]
            if values[0] != start or values[-1] != -10 or values != sorted(values, reverse=True):
                raise RuntimeError(f"降温シナリオの入力が不正です: {key}")
    if scenario == "pressure-sweep":
        values = [frame["pressure_bar"] for frame in frames]
        if (values[0] != 0 or values[-1] != 0 or max(values) != 10
                or values[:10 * FRAME_RATE + 1] != sorted(values[:10 * FRAME_RATE + 1])
                or values[10 * FRAME_RATE:] != sorted(values[10 * FRAME_RATE:], reverse=True)):
            raise RuntimeError("油圧の上昇・下降入力が不正です")
    if scenario == "warning-history":
        return
    if scenario == "warnings":
        at = lambda time_ms: frames[frame_at_or_after(time_ms)]
        for time, pressure in ((8100, 3.1), (12100, 3.0), (16200, 2.9), (23000, 2.9), (30000, 6.0)):
            if not math.isclose(at(time)["pressure_bar"], pressure, abs_tol=0.0001):
                raise RuntimeError(f"油圧の境界値・回復入力が不正です: {time}")
    if scenario in ("circuit", "racing"):
        at = lambda time_ms: frames[frame_at_or_after(time_ms)]
        expected = ({4100: False, 8200: True, 14000: True, 36000: True} if scenario == "circuit" else
                    {2100: False, 4200: True, 5000: True, 184000: True, 184300: False,
                     187300: True, 190000: False, 192000: False, 196000: False})
        for time, racing in expected.items():
            if at(time)["racing"] != racing:
                raise RuntimeError(f"Rモードの判定・保持・解除が不正です: {at(time)}")
        if scenario == "circuit":
            for time in (4000, 8000, 18000, 22000):
                if at(time)["g"] != 2 or not math.isclose(at(time)["pressure_bar"], 0.8, abs_tol=0.000001):
                    raise RuntimeError(f"2.0Gと油圧の同時急落を再現できていません: {at(time)}")
    if scenario in ("startup", "warm-start"):
        if frames[0]["oil_c"] != 70 or frames[0]["water_c"] != 70:
            raise RuntimeError("始動時の70℃入力がありません")
    if scenario == 'raw-faults':
        at = lambda time_ms: metadata['frames'][frame_at_or_after(time_ms)]
        for time, channel, count in [(4000, 0, 0), (8000, 1, 2047), (12000, 2, 1633)]:
            if at(time)['raw']['adc'][channel] != count:
                raise RuntimeError(f'生ADCの異常刺激が不正です: {at(time)}')
        if (at(4600)['sensor']['oil_c'] != 200 or at(8600)['sensor']['water_c'] != 200
                or not at(12000)['sensor']['pressure_fault'] or at(14000)['sensor']['pressure_fault']
                or at(16000)['raw']['adc'] != [0, 0, 0, 0]):
            raise RuntimeError('ADCからの温度異常・油圧過電圧・解除を検出できていません')
    if scenario == 'circuit':
        at = lambda time_ms: metadata['frames'][frame_at_or_after(time_ms)]
        for time, direction in [(4000, 'Front'), (8000, 'Right'), (18100, 'FL'), (22000, 'Left')]:
            if at(time)['g_direction'] != direction or not math.isclose(at(time)['g'], 2, abs_tol=0.00001):
                raise RuntimeError(f'IMU各軸からのG・方向計算が不正です: {at(time)}')
        if at(14500)['g'] != 0 or at(14500)['raw']['imu'][2] != 3:
            raise RuntimeError('縦軸の加速度だけで水平Gを検出しています')
    if scenario in ("ambient", "racing"):
        checkpoints = ({0: 60, 16000: 255, 48000: 200, 88000: 60, 96000: 255, 98000: 60, 120000: 255}
                       if scenario == "ambient" else {0: 60, 4200: 255, 184300: 60, 187300: 255, 190000: 255, 192000: 60})
        at = lambda time_ms: frames[frame_at_or_after(time_ms)]
        for time, brightness in checkpoints.items():
            if at(time)["actual"]["brightness_pwm"] != brightness:
                raise RuntimeError(f"調光・R・詳細画面の輝度遷移が不正です: {at(time)}")
    if scenario == "startup":
        if frames[2 * FRAME_RATE]["oil_c"] != -10 or frames[2 * FRAME_RATE]["water_c"] != -10:
            raise RuntimeError("始動時70℃から低温へ戻る入力がありません")
        acquired = metadata['frames'][FRAME_RATE * 3 // 2]['sensor']
        if not all(69 <= acquired[key] <= 71 for key in ('oil_c', 'water_c')):
            raise RuntimeError('24FPSでのIMU補正後に70℃相当のADCを取得できていません')
    # 始動直後の70℃→低温の変化以外、温度はゆっくり上昇させる。
    for before, after in zip(frames, frames[1:]):
        for key in ("oil_c", "water_c"):
            if scenario == "startup" and after["time_ms"] == 2000:
                continue
            if abs(after[key] - before[key]) > 2.2 * (after["time_ms"] - before["time_ms"]) / 1000 + 0.0001:
                raise RuntimeError(f"温度が急変しています: {scenario}, {after}")


def validate_monitoring(metadata):
    name, frames = metadata['scenario'], metadata['frames']
    at = lambda seconds: frames[round(seconds * FRAME_RATE)]
    if name == 'negative-maxima':
        if at(0)['actual']['maxima'] != [None, None, None] or at(1.5)['actual']['maxima'][:2] != [None, None]:
            raise RuntimeError('温度の未取得・起動時異常を0度として記録しています')
        if not all(value < 0 for value in at(3)['actual']['maxima'][:2]):
            raise RuntimeError('初回の負温度を最高値へ記録できていません')
        first, warmer = at(5)['actual']['maxima'][:2], at(8.5)['actual']['maxima'][:2]
        if not all(before < after < 0 for before, after in zip(first, warmer)):
            raise RuntimeError('負温度同士の最高値更新が不正です')
        if at(13.5)['sensor']['status'][:2] != [3, 3]:
            raise RuntimeError('正常取得後の温度異常を再現できていません')
        if not all(at(second)['actual']['maxima'][:2] == warmer for second in (11, 13.5, 18)):
            raise RuntimeError('再冷却・温度異常で負の最高値を失っています')
        if not all(value > 0 for value in at(23)['actual']['maxima'][:2]):
            raise RuntimeError('0度越えの最高値更新が不正です')
    if name == 'menu-monitoring':
        if not all(frame['menu'] and not frame['racing'] for frame in frames[4 * FRAME_RATE:12 * FRAME_RATE]):
            raise RuntimeError('詳細画面中にRモードが再開しています')
        if not at(7)['actual']['warning_history'] or at(11)['actual']['low_event']['duration_s'] < 2.5:
            raise RuntimeError('詳細画面中のLOWを履歴へ記録できていません')
        if not all(value > minimum for value, minimum in zip(at(11)['actual']['maxima'], [140, 118, 7.9])):
            raise RuntimeError('詳細画面中の温度・油圧ピークを記録できていません')
        if not at(12.25)['r_visible'] or at(12)['actual']['warning_history']:
            raise RuntimeError('高Gからの画面復帰でR再表示・W消去が不正です')
        if at(17)['sensor']['status'] != [2, 2, 2] or at(17)['actual']['maxima'] != at(16)['actual']['maxima']:
            raise RuntimeError('短い通信異常の即時反映・最高値保持が不正です')
        if at(19)['sensor']['status'] != [1, 1, 1]:
            raise RuntimeError('通信復帰後の正常取得が不足しています')
    if name == 'fault-monitoring':
        if at(4)['sensor']['status'] != [2, 2, 2] or any(frame['actual']['warning_history'] for frame in frames[:11 * FRAME_RATE]):
            raise RuntimeError('通信異常を0barとしてLOWを誤検知しています')
        if at(14)['sensor']['status'][2] != 1 or not at(14)['low_visible']:
            raise RuntimeError('正常な0barのLOWが成立していません')
        if at(16)['sensor']['status'] != [3, 3, 3] or at(19)['sensor']['status'] != [1, 1, 1]:
            raise RuntimeError('信号異常・復帰の区別が不正です')


def warning_visible(image):
    # 通常の赤い目盛りを避け、LOW ボックス中央の赤背景を検出する。
    return sum(red > 180 and green < 80 and blue < 80
               for red, green, blue in image.crop((40, 130, 103, 159)).getdata()) > 100


def setup_error_visible(image):
    # 2行の赤い起動メッセージと、その下にゲージが未描画であることを確認する。
    def red_count(box):
        return sum(red > 180 and green < 80 and blue < 80
                   for red, green, blue in image.crop(box).getdata())
    return (red_count((0, 0, 300, 16)) > 30 and red_count((0, 16, 300, 32)) > 30
            and image.crop((0, 40, 320, 240)).getbbox() is None)


def history_visible(image):
    return any(red > 180 and green < 80 and blue < 80
               for red, green, blue in image.crop((14, 224, 22, 232)).getdata())


def racing_visible(image):
    return any(red > 180 and green < 80 and blue < 80
               for red, green, blue in image.crop((0, 224, 12, 237)).getdata())


def convert_and_check(directory):
    metadata = json.loads((directory / "inputs.json").read_text())
    if metadata.get('schema_version') != 2 or metadata['fps'] != FRAME_RATE:
        raise RuntimeError('24FPSの生ADC・IMU入力を記録したschema_version=2の録画が必要です')
    frames = metadata["frames"]
    config = SCENARIOS[metadata["scenario"]]
    sources = sorted(directory.glob("frame-*.ppm"))
    if len(frames) != config["seconds"] * FRAME_RATE + 1 or len(sources) != len(frames):
        raise RuntimeError(f"録画が不完全です: {directory}")
    if [frame["time_ms"] for frame in frames] != [frame_time_ms(index) for index in range(len(frames))]:
        raise RuntimeError("録画時刻が24FPSの刻みと一致しません")
    add_expectations(directory, metadata)
    validate_inputs(metadata)
    seen = set()
    snapshots = [frame_at_or_after(round(seconds * 1000)) for seconds in config["snapshots"]]
    sheet = Image.new("RGB", (320 * 3, 264 * math.ceil(len(snapshots) / 3)), "#252525")
    labels = ImageDraw.Draw(sheet)
    for index, source in enumerate(sources):
        with Image.open(source) as image:
            image.load()
            if image.size != (320, 240) or image.getbbox() is None:
                raise RuntimeError(f"空白またはサイズ不正のフレーム: {source}")
            seen.add(hashlib.sha256(image.tobytes()).digest())
            image.save(source.with_suffix(".png"))
            frame = frames[index]
            frame["setup_error_visible"] = setup_error_visible(image)
            frame["low_visible"] = warning_visible(image)
            frame["r_visible"] = racing_visible(image)
            frame["w_visible"] = history_visible(image)
            check_expectations(frame)
            if not frame["menu"] and frame["r_visible"] != frame["racing"]:
                raise RuntimeError(f"Rモードと実際のR表示が一致しません: {frame}")
            # 130℃を超えた油温バーが右側の数値領域へはみ出さないことを確認する。
            if metadata["scenario"] in ("ramp", "cooldown") and metadata["sensors"]["oil_temp"]:
                if any(red > 180 and green < 80 and blue < 80
                       for red, green, blue in image.crop((231, 16, 237, 34)).getdata()):
                    raise RuntimeError(f"油温バーが数値領域へはみ出しています: {frames[index]}")
                checkpoints = ({0: False, 92000: True, 100000: True, 190000: False} if metadata["scenario"] == "ramp"
                               else {0: False, 2000: True, 50000: False, 100000: False})
                red_bar = any(red > 180 and green < 80 and blue < 80
                              for red, green, blue in image.crop((20, 16, 230, 34)).getdata())
                if frame["time_ms"] in checkpoints and red_bar != checkpoints[frame["time_ms"]]:
                    raise RuntimeError(f"昇温・降温時の油温バーの赤色と解除が不正です: {frame}")
            if metadata["scenario"] == "warnings" and metadata["sensors"]["pressure"]:
                visible = warning_visible(image)
                time = frames[index]["time_ms"]
                if time < 22000 and visible:
                    raise RuntimeError(f"短い油圧低下でLOWが点灯しています: {time}")
                if time in (23500, frame_time_ms(frame_at_or_after(24200))) and not visible:
                    raise RuntimeError(f"継続する低油圧・解除後の保持でLOWが点灯していません: {time}")
                if time >= 27500 and visible:
                    raise RuntimeError(f"油圧回復後もLOWが残っています: {time}")
            if metadata["scenario"] == "circuit" and metadata["sensors"]["pressure"]:
                expected = {4100: False, 9500: True, 10500: True, 14000: False,
                            18100: False, 23500: True, 26000: True, 28000: False}
                expected = {frame_time_ms(frame_at_or_after(time)): state for time, state in expected.items()}
                time = frame["time_ms"]
                if time in expected and frame["low_visible"] != expected[time]:
                    raise RuntimeError(f"急落・復帰時のLOW表示が不正です: {frame}")
            if index in snapshots:
                tile = snapshots.index(index)
                x, y = (tile % 3) * 320, (tile // 3) * 264
                sheet.paste(image, (x, y))
                labels.text((x + 6, y + 243), f'{frames[index]["time_ms"] / 1000:.3f}s', fill="white")
        source.unlink()
    if metadata["scenario"] == "warning-history":
        for seconds, menu, history, visible in [(7, False, True, True), (8, True, True, False),
                                                (10, False, False, False), (12, False, False, False),
                                                (19, False, True, True)]:
            frame = frames[seconds * FRAME_RATE]
            if (frame['menu'], frame['actual']['warning_history'], frame['w_visible']) != (menu, history, visible):
                raise RuntimeError(f'詳細画面から戻る際のWリセットが不正です: {frame}')
    validate_monitoring(metadata)
    validate_review(metadata["scenario"], frames)
    if len(seen) < 20:
        raise RuntimeError("描画がほとんど変化していません")
    sheet.save(directory / "contact-sheet.png")
    (directory / "inputs.json").write_text(json.dumps(metadata, ensure_ascii=False, separators=(',', ':')), encoding="utf-8")
    return metadata


def encode(directory):
    metadata = json.loads((directory / "inputs.json").read_text())
    command = [imageio_ffmpeg.get_ffmpeg_exe(), "-nostdin", "-loglevel", "error", "-f", "rawvideo",
               "-pixel_format", "rgb24", "-video_size", f'{SIZE[0]}x{SIZE[1]}', "-framerate", str(FRAME_RATE),
               "-i", "pipe:0", "-c:v", "libx264", "-crf", "18", "-pix_fmt", "yuv420p",
               "-movflags", "+faststart", str(directory / "recording.mp4")]
    # メーターの原画像は変更せず、動画への入力時だけ外側の比較欄を合成する。
    with subprocess.Popen(command, stdin=subprocess.PIPE) as encoder:
        try:
            for index, sample in enumerate(metadata['frames']):
                with Image.open(directory / f'frame-{index:04d}.png') as source:
                    frame = annotated_frame(source, sample, metadata['scenario'], metadata['sensor_settings'])
                    if index == 0:
                        frame.save(directory / 'poster.png')
                    encoder.stdin.write(frame.tobytes())
        finally:
            encoder.stdin.close()
        if encoder.wait(timeout=120):
            raise RuntimeError(f'動画の生成に失敗しました: {directory}')
    # エンコード後のファイルもデコードし、欠損した動画を成果物にしない。
    subprocess.run(
        [imageio_ffmpeg.get_ffmpeg_exe(), "-nostdin", "-v", "error", "-xerror", "-i",
         str(directory / "recording.mp4"), "-f", "null", "-"],
        check=True, timeout=60,
    )


def write_report(output, scenarios):
    columns = ['time_ms', 'setup_error', 'g', 'lux', 'low_visible', 'r_visible', 'w_visible', 'adc', 'imu', 'pressure_bar',
               'low_event', 'expected_low_event', 'warning_state', 'adc_fault', 'adc_connected', 'adc_begin_calls', 'adc_wait_ms', 'adc_reads', 'adc_last',
               'case_label', 'input_temperature_c', 'input_temperature_abnormal', 'actual_oil_c', 'actual_water_c',
               'expected_pressure_bar', 'expected_oil_c', 'expected_water_c', 'brightness_mode',
               'brightness_pwm', 'lux_sample', 'lux_median', 'expected_brightness_pwm', 'sensor_status', 'maxima', 'expected_maxima']
    data = []
    for name in scenarios:
        config = SCENARIOS[name]
        metadata = json.loads((output / name / "inputs.json").read_text())
        # 列名を共有して数値行にし、24FPSの全区間でもHTMLを直開きできる量に収める。
        frames = []
        for frame in metadata['frames']:
            values = {key: frame[key] for key in ('time_ms', 'setup_error', 'g', 'lux', 'low_visible', 'r_visible', 'w_visible')}
            values.update(adc_fault=frame['raw']['adc_fault'])
            values.update({key: frame['sensor'][key] for key in ('adc_connected', 'adc_begin_calls', 'adc_wait_ms', 'adc_reads', 'adc_last')})
            values.update(adc=frame['raw']['adc'], imu=frame['raw']['imu'],
                          pressure_bar=round(frame['sensor']['pressure_bar'], 4))
            values.update({f'expected_{key}': frame['expected'][key] for key in ('oil_c', 'water_c', 'pressure_bar')})
            values.update({key: frame['actual'][key] for key in ('brightness_mode', 'brightness_pwm', 'lux_sample', 'lux_median')})
            values['low_event'] = [round(frame['actual']['low_event'][key], 4) for key in ('g', 'pressure_bar', 'duration_s')] + [frame['actual']['low_event']['direction']]
            values['expected_low_event'] = [round(frame['expected']['low_event'][key], 4) for key in ('g', 'pressure_bar', 'duration_s')] + [frame['expected']['low_event']['direction']]
            values['warning_state'] = frame['expected']['warning_state']
            values.update(case_label=frame.get('case_label', ''), input_temperature_c=frame.get('input_temperature_c'),
                          input_temperature_abnormal=frame.get('input_temperature_abnormal'),
                          actual_oil_c=frame['actual']['oil_c'], actual_water_c=frame['actual']['water_c'])
            values.update(sensor_status=frame['sensor']['status'], maxima=frame['actual']['maxima'], expected_maxima=frame['expected']['maxima'])
            values['expected_brightness_pwm'] = frame['expected']['brightness_pwm']
            frames.append([values[key] for key in columns])
        cues = [frame_at_or_after(round(second * 1000)) for second in config['snapshots']]
        data.append({"name": name, **config, 'snapshots': cues, 'fps': FRAME_RATE, 'columns': columns, "frames": frames, "sensor_spec": metadata.get("temperature_sensor_spec"), "startup_error_title": metadata["startup_error_title"], "startup_error_hint": metadata["startup_error_hint"], "review": REVIEWS.get(name), "review_spots": review_spots(name, metadata["frames"])})
    template = (ROOT / "report.html").read_text()
    payload = json.dumps(data, ensure_ascii=False, separators=(',', ':')).replace("<", "\\u003c")
    (output / "index.html").write_text(template.replace("@@SCENARIOS@@", payload).replace("@@SPOT_CHECKS@@", (output / "spot-checks.json").read_text().replace("<", "\\u003c")), encoding="utf-8")


def export_preview(output, scenarios):
    # 公開用には動画と代表画像だけを含め、全PNGはActionsの成果物へ残す。
    preview = output / "public"
    preview.mkdir()
    shutil.copyfile(output / "index.html", preview / "index.html")
    shutil.copyfile(output / "spot-checks.json", preview / "spot-checks.json")
    for scenario in scenarios:
        destination = preview / scenario
        destination.mkdir()
        for name in ("recording.mp4", "contact-sheet.png", "poster.png", "inputs.json"):
            shutil.copyfile(output / scenario / name, destination / name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=["all", *SCENARIOS], default="all")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    output = args.output or ROOT / "out" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
    output = output.resolve()
    # 既存の成果物やユーザーのファイルを上書きしない。
    output.mkdir(parents=True, exist_ok=False)
    if not args.skip_build:
        subprocess.run([sys.executable, "-m", "platformio", "run", "-d", str(ROOT), "-e", "desktop"], check=True)
    program = ROOT / ".pio" / "build" / "desktop" / ("program.exe" if os.name == "nt" else "program")
    scenarios = list(SCENARIOS) if args.scenario == "all" else [args.scenario]
    run_spot_checks(output)
    for scenario in scenarios:
        directory = output / scenario
        subprocess.run(
            [str(program), "--scenario", scenario, "--seconds", str(SCENARIOS[scenario]["seconds"]), "--capture", str(directory)],
            env={**os.environ, "SDL_VIDEODRIVER": "dummy", "SDL_RENDER_DRIVER": "software"},
            check=True, timeout=180,
        )
        convert_and_check(directory)
        encode(directory)
    write_report(output, scenarios)
    export_preview(output, scenarios)
    print(f"録画とレポート: {output / 'index.html'}")


if __name__ == "__main__":
    main()
