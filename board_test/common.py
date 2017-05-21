"""File containing common methods across tests"""

import time
from enum import Enum
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket

WAIT_TIME = 1.5

SYMBOL_TABLE = [0x11, 0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
                0x41, 0x42, 0x44, 0x48, 0x03, 0x06, 0x0C, 0x09, 0x60]
# Virtual address is 0x0200
CHIP_ADDRESS = [0, 2, 0, 0]
CHIP_SYMBOLS = [SYMBOL_TABLE[x] for x in CHIP_ADDRESS]

class SpiNNMode(Enum):
    """Enum values representing encoding values"""
    SPINN_MODE_128 = 0
    SPINN_MODE_64 = 1
    SPINN_MODE_32 = 2
    SPINN_MODE_16 = 3


def board_assert(assertion):
    """Helper method to wait for WDT to reset board in case of failure"""
    if not assertion:
        time.sleep(WAIT_TIME)
    assert assertion

def board_assert_equal(left, right):
    """Helper method to wait for WDT to reset board in case of failure"""
    if left != right:
        time.sleep(WAIT_TIME)
    assert left == right

def board_assert_not_equal(left, right):
    """Helper method to wait for WDT to reset board in case of failure"""
    if left == right:
        time.sleep(WAIT_TIME)
    assert left != right

def board_assert_not_none(assertion):
    """Helper method to wait for WDT to reset board in case of failure"""
    if assertion:
        time.sleep(WAIT_TIME)
    assert assertion is not None

def board_assert_ge(left, right):
    """Assert that left is greater than or equal to the right"""
    if left < right:
        time.sleep(WAIT_TIME)
    assert left >= right

def board_assert_le(left, right):
    """Assert that left is less than or equal to the right"""
    if left > right:
        time.sleep(WAIT_TIME)
    assert left <= right

def board_assert_isinstance(obj, classinfo):
    """Assert that obj is instance of classinfo"""
    if not isinstance(obj, classinfo):
        time.sleep(WAIT_TIME)
    assert isinstance(obj, classinfo)

def spinn_2_to_7(pkt, mode):
    """Converts DVS Packet data to SpiNN encoding using given mode"""

    buf = []
    data = 0x8000 + ((pkt.pol & 0x1) << 14)

    # Switch which mode is used
    if mode == SpiNNMode.SPINN_MODE_128:
        data += ((pkt.y & 0x7F) << 7) + (pkt.x & 0x7F)
    elif mode == SpiNNMode.SPINN_MODE_64:
        data += ((pkt.y & 0x7E) << 5) + ((pkt.x & 0x7E) >> 1)
    elif mode == SpiNNMode.SPINN_MODE_32:
        data += ((pkt.y & 0x7C) << 3) + ((pkt.x & 0x7C) >> 2)
    elif mode == SpiNNMode.SPINN_MODE_16:
        data += ((pkt.y & 0x78) << 1) + ((pkt.x & 0x78) >> 3)

    print(bin(data)[2:].zfill(16))
        
    # Calculate parity
    xor_all = (CHIP_ADDRESS[0] ^ CHIP_ADDRESS[1] ^ CHIP_ADDRESS[2] ^
               CHIP_ADDRESS[3] ^ ((data & 0xFF00) >> 8) ^ (data & 0xFF))
    odd_parity = (1 ^ ((xor_all & 0x80) >> 7) ^ ((xor_all & 0x40) >> 6) ^
                  ((xor_all & 0x20) >> 5) ^ ((xor_all & 0x10) >> 4) ^
                  ((xor_all & 0x08) >> 3) ^ ((xor_all & 0x04) >> 2) ^
                  ((xor_all & 0x02) >> 1) ^ (xor_all & 0x01))
    
    # Header
    buf += [SYMBOL_TABLE[odd_parity], SYMBOL_TABLE[0]]
    # Data
    buf += [SYMBOL_TABLE[data & 0x000F],
            SYMBOL_TABLE[(data & 0x00F0) >> 4],
            SYMBOL_TABLE[(data & 0x0F00) >> 8],
            SYMBOL_TABLE[(data & 0xF000) >> 12]]
    # Chip Address
    for addr in reversed(CHIP_SYMBOLS):
        buf += [addr]
    # EOP
    buf += [SYMBOL_TABLE[-1]]

    return SpiNNPacket(buf)
