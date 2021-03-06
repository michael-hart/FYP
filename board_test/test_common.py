"""Module to unit test common methods"""

import pytest
from common import spinn_2_to_7, SpiNNMode
from dvs_packet import DVSPacket

@pytest.mark.parametrize("dvs_pkt,mode,result", [
    (DVSPacket(10, 30, 1), SpiNNMode.SPINN_MODE_128, 
     [0x11, 0x11, 0x44, 0x11, 0x09, 0x03, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(10, 30, 1), SpiNNMode.SPINN_MODE_64, 
     [0x11, 0x11, 0x22, 0x03, 0x18, 0x03, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(10, 30, 1), SpiNNMode.SPINN_MODE_32, 
     [0x11, 0x11, 0x14, 0x0C, 0x11, 0x03, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(10, 30, 1), SpiNNMode.SPINN_MODE_16, 
     [0x12, 0x11, 0x12, 0x18, 0x11, 0x03, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(54, 99, 0), SpiNNMode.SPINN_MODE_128, 
     [0x12, 0x11, 0x24, 0x48, 0x12, 0x48, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(54, 99, 0), SpiNNMode.SPINN_MODE_64, 
     [0x11, 0x11, 0x48, 0x22, 0x03, 0x41, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(54, 99, 0), SpiNNMode.SPINN_MODE_32, 
     [0x11, 0x11, 0x06, 0x11, 0x18, 0x41, 0x11, 0x11, 0x14, 0x11, 0x60]),
    (DVSPacket(54, 99, 0), SpiNNMode.SPINN_MODE_16, 
     [0x12, 0x11, 0x24, 0x03, 0x11, 0x41, 0x11, 0x11, 0x14, 0x11, 0x60]),
    ])
def test_encode_dvs(dvs_pkt, mode, result):
    """Tests that encoding a DVS packet matches the hand encoded version"""
    assert spinn_2_to_7(dvs_pkt, mode).data == result
