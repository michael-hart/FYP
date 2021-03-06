"""Module to test SpiNNaker code with no DVS connected"""

import pytest
from common import (board_assert_equal, board_assert_ge,
                    board_assert_isinstance, SpiNNMode, spinn_2_to_7,
                    motor_2_to_7)
from fixtures import board
from controller import RESPONSES
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket
from test_dvs_downscale import (JUST_ENOUGH_64, JUST_ENOUGH_32, JUST_ENOUGH_16,
                                dvs_offset)

def test_spinn_fwd_permanent_on(board):
    """Tests that turning forwarding on permanently works"""
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])

def test_spinn_fwd_reset(board):
    """Tetss that resetting the forwarding works"""
    board_assert_equal(board.reset_spinn(), RESPONSES["success"])

def test_spinn_fwd_on_reset(board):
    """Tests that turning on and then resetting forwarding works"""
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])
    board_assert_equal(board.reset_spinn(), RESPONSES["success"])

def test_spinn_fwd_temp(board):
    """Tests that setting temporary forwarding is accepted"""
    board_assert_equal(board.forward_spinn(1000), RESPONSES["success"])

def test_spinn_fwd_received(board):
    """Tests that sending a DVS packet and receiving some data works"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])
    # Send a test packet
    test_pkt = DVSPacket(10, 30, 1)
    board_assert_equal(board.use_dvs(test_pkt), RESPONSES["success"])

    pkt = board.get_spinn()
    board_assert_isinstance(pkt, SpiNNPacket)

@pytest.mark.parametrize("mode", range(4))
def test_spinn_set_mode_correct(board, mode):
    board_assert_equal(board.set_mode_spinn(mode), RESPONSES["success"])

@pytest.mark.parametrize("mode", [255, 4, 5])
def test_spinn_set_mode_incorrect(board, mode):
    board_assert_equal(board.set_mode_spinn(mode), RESPONSES["bad_param"])

@pytest.mark.parametrize("dvs_pkt", [
    DVSPacket(10, 20, 1), # Normal data
    DVSPacket(10, 20, 0), # Another polarity
    DVSPacket(0, 0, 0), # Min values
    DVSPacket(127, 127, 1), # Max values
    ])
def test_spinn_encode(board, dvs_pkt, log):
    """Test that board encodes a packet the same as the test system for mode 128"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])

    # Set mode to the mode under test
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_128.value), 
                       RESPONSES["success"])

    # For each packet, test all modes
    result = spinn_2_to_7(dvs_pkt, SpiNNMode.SPINN_MODE_128)

    # Send packet
    board_assert_equal(board.use_dvs(dvs_pkt), RESPONSES["success"])

    # Retrieve response
    pkt = board.get_spinn()
    board_assert_isinstance(pkt, SpiNNPacket)

    # Check data is correct
    log.info("Got packet data: {}".format([hex(x) for x in pkt.data]))
    log.info("Calculated data: {}".format([hex(x) for x in result.data]))
    board_assert_equal(pkt.data, result.data)

@pytest.mark.parametrize("pkt_list,exp,mode", [
    # Centred at 0, 0
    (JUST_ENOUGH_64, DVSPacket(0, 0, 1), SpiNNMode.SPINN_MODE_64),
    (JUST_ENOUGH_32, DVSPacket(0, 0, 1), SpiNNMode.SPINN_MODE_32),
    (JUST_ENOUGH_16, DVSPacket(0, 0, 1), SpiNNMode.SPINN_MODE_16),

    # Centred at x=47, y=93
    (dvs_offset(JUST_ENOUGH_64, 47, 93, 2), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 2)[0], SpiNNMode.SPINN_MODE_64),

    (dvs_offset(JUST_ENOUGH_32, 47, 93, 4), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 4)[0], SpiNNMode.SPINN_MODE_32),

    (dvs_offset(JUST_ENOUGH_16, 47, 93, 8), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 8)[0], SpiNNMode.SPINN_MODE_16),
])
def test_spinn_encode_downres(board, pkt_list, exp, mode, log):
    """Tests that encoding works for two of each mode"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])

    # Set mode to the mode under test
    board_assert_equal(board.set_mode_spinn(mode.value), RESPONSES["success"])

    # For each packet, test all modes
    result = spinn_2_to_7(exp, mode)

    # Send packets
    for dvs_pkt in pkt_list:
        board_assert_equal(board.use_dvs(dvs_pkt), RESPONSES["success"])

    # Retrieve response
    pkt = board.get_spinn()
    board_assert_isinstance(pkt, SpiNNPacket)

    # Check data is correct
    log.info("Got packet data: {}".format([hex(x) for x in pkt.data]))
    log.info("Calculated data: {}".format([hex(x) for x in result.data]))
    board_assert_equal(pkt.data, result.data)


def test_spinn_nocrash(board):
    """Test that sending a DVS packet with no forwarding does not crash board"""
    pkt = DVSPacket(10, 30, 1)
    # Send packet
    board_assert_equal(board.use_dvs(pkt), RESPONSES["success"])
    # Echo to check that the board replies
    board_assert_equal(board.echo("test"), "test")

def test_spinn_fwd_rx_permanent_on(board):
    """Tests that turning forwarding on permanently works"""
    board_assert_equal(board.set_spinn_rx_fwd(0), RESPONSES["success"])

def test_spinn_fwd_rx_reset(board):
    """Tetss that resetting the forwarding works"""
    board_assert_equal(board.reset_spinn_rx_fwd(), RESPONSES["success"])

def test_spinn_fwd_rx_on_reset(board):
    """Tests that turning on and then resetting forwarding works"""
    board_assert_equal(board.set_spinn_rx_fwd(0), RESPONSES["success"])
    board_assert_equal(board.reset_spinn_rx_fwd(), RESPONSES["success"])

def test_spinn_fwd_rx_temp(board):
    """Tests that setting temporary forwarding is accepted"""
    board_assert_equal(board.set_spinn_rx_fwd(1000), RESPONSES["success"])

def test_spinn_fwd_rx_received(board):
    """Tests that sending a DVS packet and receiving some data works"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.set_spinn_rx_fwd(0), RESPONSES["success"])
    # Send a test packet
    test_pkt = motor_2_to_7(100)
    board_assert_equal(board.use_spinn(test_pkt), RESPONSES["success"])

    speed = board.get_received_data()
    assert speed > 0
    assert speed == 100
