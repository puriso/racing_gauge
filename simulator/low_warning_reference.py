"""本番描画から独立した、低油圧イベント・表示保持・直近履歴の期待値。"""

from numeric import float32


class LowWarningReference:
    def __init__(self):
        self.start = None
        self.qualified = False
        self.last_warning = None
        self.peak = self.minimum = 0.0
        self.direction = 'Right'
        self.event = dict(g=0.0, pressure_bar=0.0, duration_s=0.0, direction='Right')
        self.state = 'idle'

    def step(self, now, g, pressure, direction):
        if g > 1 and pressure <= 3:
            if self.start is None:
                self.start = now
                self.qualified = False
                self.peak, self.minimum, self.direction = g, pressure, direction
            self.peak = max(self.peak, g)
            self.minimum = min(self.minimum, pressure)
            if ((now - self.start) & 0xffffffff) >= 500:
                self.qualified = True
                self.last_warning = now
        else:
            if self.start is not None and self.qualified:
                self.event = dict(g=self.peak, pressure_bar=self.minimum,
                                  duration_s=float32(float32((now - self.start) & 0xffffffff) / 1000),
                                  direction=self.direction)
            self.start = None
            self.qualified = False
        visible = self.last_warning is not None and ((now - self.last_warning) & 0xffffffff) < 3000
        if not visible:
            self.last_warning = None
        self.state = ('qualified' if self.qualified else 'pending') if self.start is not None else ('held' if visible else 'idle')
        return visible
