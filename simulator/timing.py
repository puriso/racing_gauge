"""24FPSの仮想時刻と、メディアの表示時刻を揃える。"""

FRAME_RATE = 24


def frame_time_ms(index):
    return index * 1000 // FRAME_RATE


def frame_at_or_after(time_ms):
    return (time_ms * FRAME_RATE + 999) // 1000


def gif_duration_ms(index):
    # GIFの10ms単位へ余りを持ち越し、24フレームの合計を1000msに保つ。
    return ((index + 1) * 100 // FRAME_RATE - index * 100 // FRAME_RATE) * 10
