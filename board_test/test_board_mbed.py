"""Module to test that MBED and board simulate SpiNNaker"""

import pytest
import time
from fixtures import mbed, board, log
from common import (board_assert_equal, spinn_2_to_7, SpiNNMode, 
                    board_assert_isinstance)
from controller import RESPONSES
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket

@pytest.mark.dev("mbed")
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

    # Give a brief period of time to forward to the MBED
    time.sleep(0.1)

    # Retrieve packets from MBED
    (_, _, rx_pkt) = mbed.get_spinn()

    # Check results are identical
    assert rx_pkt
    for rxp in rx_pkt:
        board_assert_isinstance(rxp, SpiNNPacket)
    log.info("Got packet data: {}".format([hex(x) for x in rx_pkt[0].data]))
    log.info("Calculated data: {}".format([hex(x) for x in pkt.data]))
    board_assert_equal(pkt.data, rx_pkt[0].data)


@pytest.mark.dev("mbed")
@pytest.mark.parametrize("packets", [2, 3, 10, 15, 20])
def test_many_packets(mbed, board, log, packets):
    """Tests the time taken for many packets to be sent is small"""

    # Clear the MBED and tell it to wait
    mbed.get_spinn()
    mbed.wait()

    # Work out x limits for 20 packets
    x_min = 12
    x_step = 6
    x_max = 100 + (packets) * x_step

    y_max = 90
    y_step = 3
    y_min = y_max - (packets) * y_step

    # Form list of test packets and expected results
    dvs_data = [DVSPacket(x, y, y%2) for x, y in zip(
        range(x_min, x_max, x_step),
        range(y_max, y_min, -y_step))]
    exp_data = [spinn_2_to_7(x, SpiNNMode.SPINN_MODE_128) for x in dvs_data]

    for dvs_pkt in dvs_data:
        board_assert_equal(board.use_dvs(dvs_pkt), RESPONSES["success"])

    # Trigger the MBED to start the speed test
    mbed.trigger()

    # Give period of time to forward to the MBED
    time.sleep(packets * 0.05)

    # Retrieve packets
    (duration, count, rx_data) = mbed.get_spinn()

    # Check results are identical
    assert rx_data
    assert len(rx_data) == len(exp_data)
    assert len(rx_data) == count
    for exp, rxp in zip(rx_data, exp_data):
        assert exp.data == rxp.data

    # Check duration is reasonable - packets times 15ms
    assert duration < len(exp_data) * 15000

    log.info("Test for %d packets took %lfs with %lfs per packet"
             % (len(exp_data), duration/1000000.0, duration/(1000000.0*packets)))
