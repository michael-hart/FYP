"""Module to contain SpiNNaker packet wrapper"""

SPINN_PACKET_SHORT = 11

class SpiNNPacket(object):
    """SpiNNaker packet wrapper"""

    def __init__(self, data):
        self.data = data
