"""Simple class to encapsulate DVS packet"""

class DVSPacket(object):

    x = 0
    y = 0
    pol = 0

    def __init__(self, x, y, pol):
        self.x = x
        self.y = y
        self.pol = pol
