"""外部応答と時刻から、ADC再接続・ダミー取得・通信失敗を独立計算する。"""


class AdcBusReference:
    def __init__(self, reads, last):
        self.reads, self.last = reads, last
        self.connected = False
        self.started = False
        self.attempts = 0
        self.last_attempt = 0
        self.now = self.wait = self.fault = 0

    def begin(self, now):
        self.attempts += 1
        self.last_attempt = now
        self.connected = self.fault != 1

    def frame(self, now, fault):
        self.now, self.fault, self.wait = now, fault, 0
        if not self.started:
            self.begin(now)
            self.started = True

    def read(self, adc, channel):
        now = self.now + self.wait
        if not self.connected:
            if now - self.last_attempt < 1000:
                return None
            self.begin(now)
            if not self.connected:
                return None
        if self.fault:
            if self.fault == 2:
                self.wait += 5
            elif self.fault == 6:
                self.reads[channel] += 1
                self.last[channel] = adc[channel]
            self.connected = False
            self.last_attempt = self.now + self.wait
            return None
        self.reads[channel] += 2
        self.last[channel] = adc[channel]
        return adc[channel]


def fault_input(scenario, now):
    if scenario == 'adc-startup':
        return 1 if now < 5000 else 0
    if scenario != 'adc-communication' or now >= 48000 or (now // 4000) % 2 == 0:
        return 0
    return now // 8000 + 1


def validate_communication(metadata):
    name, frames = metadata['scenario'], metadata['frames']
    for frame in frames:
        if frame['raw']['adc_fault'] != fault_input(name, frame['time_ms']):
            raise ValueError('ADC外部応答の入力がシナリオと一致しません')
        if bool(frame.get('setup_error')) != (name == 'adc-startup' and frame['time_ms'] == 0):
            raise ValueError('初回エラー画面はsetup直後の先頭フレームだけで確認します')
    if name not in ('adc-startup', 'adc-communication'):
        return
    faults = {frame['raw']['adc_fault'] for frame in frames}
    if faults != ({0, 1} if name == 'adc-startup' else set(range(7))):
        raise ValueError('ADC通信異常と復帰のケースが不足しています')
    if name == 'adc-communication':
        for second in (7, 15, 23, 31, 39, 47):
            frame = frames[second * 24]
            if frame['sensor']['pressure_bar'] != 0 or frame['sensor']['oil_c'] != 200 or frame['sensor']['water_c'] != 200:
                raise ValueError('ADC通信失敗を本番のセンサー異常として取得できていません')
        for second in (11, 19, 27, 35, 43, 55):
            frame = frames[second * 24]
            if not 90 < frame['sensor']['oil_c'] < 110 or not 90 < frame['sensor']['water_c'] < 100:
                raise ValueError('ADC復帰後の正常温度へ再初期化されていません')
        if not any(frame['sensor']['adc_wait_ms'] == 5 for frame in frames):
            raise ValueError('変換タイムアウトの5ms待機を通っていません')
    elif frames[4 * 24]['sensor']['oil_c'] != 200 or frames[8 * 24]['sensor']['oil_c'] < 90:
        raise ValueError('起動時未接続からの復帰が不足しています')
