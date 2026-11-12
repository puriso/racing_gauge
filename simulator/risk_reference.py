"""生入力から独立計算した監視値を使う、危険点と前後1分の期待値。"""
import math
from numeric import float32 as f32


def rounded(value, scale):
    value = f32(f32(value) * scale)
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


class RiskReference:
    def __init__(self):
        self.now = 0
        self.previous = None
        self.tick = 0
        self.history = {}
        self.window = [[0, 0, 0, 0, 0, 0] for _ in range(1201)]
        self.kind = 0
        self.recorded = False
        self.at = self.anchor = 0
        self.point = (0, 0, None)
        self.direction = 0
        self.valid_since = self.near_since = self.drop_since = None

    @staticmethod
    def merge(bucket, pressure, g, oil, valid, oil_valid, low, direction):
        if valid:
            p = rounded(max(0, min(pressure, f32(655.35))), 100)
            if bucket[4] & 1:
                bucket[0], bucket[1] = min(bucket[0], p), max(bucket[1], p)
            else:
                bucket[0] = bucket[1] = p
            bucket[4] |= 1
        else:
            bucket[4] |= 16
        if math.isfinite(g) and g >= 0:
            value = rounded(min(g, f32(655.35)), 100)
            if not bucket[4] & 2 or value > bucket[2]:
                bucket[2], bucket[5] = value, direction
            bucket[4] |= 2
        if oil_valid:
            bucket[3] = rounded(max(-273, min(oil, 199)), 10)
            bucket[4] |= 4
        else:
            bucket[4] |= 32
        if low:
            bucket[4] |= 8

    def step(self, now, pressure, g, oil, valid, oil_valid, low, direction=0):
        if self.previous is not None:
            self.now += (now - self.previous) & 0xffffffff
        self.previous = now
        self.tick = self.now // 100
        self.history = {tick: value for tick, value in self.history.items() if tick >= self.tick - 1200}
        args = pressure, g, oil, valid, oil_valid, low, direction
        bucket = self.history.setdefault(self.tick, [0, 0, 0, 0, 0, 0])
        self.merge(bucket, *args)
        if self.recorded and 0 <= self.tick - self.anchor <= 600:
            self.merge(self.window[600 + self.tick - self.anchor], *args)
        kind = 0
        if not valid or not math.isfinite(g) or g < 0:
            self.valid_since = self.near_since = self.drop_since = None
        else:
            if self.valid_since is None:
                self.valid_since = self.now
            if low:
                kind = 3
            elif self.now - self.valid_since < 1000:
                self.near_since = self.drop_since = None
            else:
                near = g > 1 and pressure <= 3.5
                self.near_since = (self.now if self.near_since is None else self.near_since) if near else None
                recent = [self.history.get(tick) for tick in range(max(0, self.tick - 5), self.tick + 1)]
                baseline = f32(max(b[1] for b in recent) / f32(100)) if all(b and b[4] & 1 and not b[4] & 16 for b in recent) else 0
                drop = g >= f32(0.8) and f32(baseline - pressure) >= 1.5 and pressure <= f32(baseline * f32(0.7))
                self.drop_since = (self.now if self.drop_since is None else self.drop_since) if drop else None
                if self.near_since is not None and self.now - self.near_since >= 300:
                    kind = 2
                elif self.drop_since is not None and self.now - self.drop_since >= 150:
                    kind = 1
        band, selected = rounded(pressure, 20), rounded(self.point[0], 20)
        better = kind > self.kind or (kind == self.kind and (band < selected or
                                    (band == selected and g >= f32(self.point[1] + f32(0.1)))))
        overview = (not self.kind and self.valid_since is not None and self.now - self.valid_since >= 1000 and
                    (not self.recorded or rounded(g, 10) > rounded(self.point[1], 10) or
                     (rounded(g, 10) == rounded(self.point[1], 10) and band < selected)))
        if (kind and better) or overview:
            self.recorded = True
            self.kind, self.at, self.anchor = kind, self.now, self.tick
            self.point = pressure, g, oil if oil_valid else None
            self.direction = direction
            self.window = [self.history.get(self.anchor + offset, [0, 0, 0, 0, 0, 0]).copy()
                           if offset <= 0 else [0, 0, 0, 0, 0, 0] for offset in range(-600, 601)]
        return self.snapshot()

    def snapshot(self):
        return dict(kind=self.kind, recorded=self.recorded, at_ms=self.at, age_ms=self.now - self.at if self.recorded else 0,
                    pressure_bar=self.point[0], g=self.point[1], oil_c=self.point[2], g_direction=self.direction,
                    pre_ms=min(self.anchor, 600) * 100 if self.recorded else 0,
                    post_ms=min(self.now - self.at, 60000) if self.recorded else 0)
