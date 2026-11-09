"""期待値モデルでC++のfloatと同じ単精度丸めを行う。"""

import struct


def float32(value):
    return struct.unpack('f', struct.pack('f', value))[0]
