"""Module to test that MBED and board simulate SpiNNaker"""

import time
from fixtures import mbed, board, log
from common import (board_assert_equal, spinn_2_to_7, SpiNNMode, 
                    board_assert_isinstance)
from controller import RESPONSES
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket

def test_single_packet(mbed, board, log):
    """Test that DVS packet sent to board is sent to MBED correctly"""

    # Trigger the MBED to ensure state is correct
    mbed.trigger()

    # Ensure board is in correct mode
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_128.value),
                       RESPONSES["success"])

    # Send a DVS packet to the board
    dvs_pkt = DVSPacket(10, 30, 1)
    board_assert_equal(board.use_dvs(dvs_pkt), RESPONSES["success"])
    pkt = spinn_2_to_7(dvs_pkt, SpiNNMode.SPINN_MODE_128)

    # TODO: remove this, it is purely for debugging
    log.info("Packet contents expected as follows:")
    for byte in pkt.data:
        log.info(bin(byte)[2:].zfill(8))

    # Give a brief period of time to forward to the MBED
    time.sleep(0.1)
    # time.sleep(100)

    # Retrieve packets from MBED
    (_, rx_pkt) = mbed.get_spinn()

    # Check results are identical
    assert rx_pkt
    for rxp in rx_pkt:
        board_assert_isinstance(rxp, SpiNNPacket)
    log.info("Got packet data: {}".format([hex(x) for x in rx_pkt[0].data]))
    log.info("Calculated data: {}".format([hex(x) for x in pkt.data]))
    board_assert_equal(pkt.data, rx_pkt[0].data)
