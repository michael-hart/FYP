"""Module to test SpiNNaker code with no DVS connected"""

import pytest
from common import (board_assert_equal, board_assert_ge,
                    board_assert_isinstance, SpiNNMode, spinn_2_to_7)
from fixtures import board
from controller import RESPONSES
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket

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

@pytest.mark.parametrize("mode", [mode for mode in SpiNNMode])
@pytest.mark.parametrize("dvs_pkt", [
    DVSPacket(10, 20, 1), # Normal data
    DVSPacket(10, 20, 0), # Another polarity
    DVSPacket(0, 0, 0), # Min values
    DVSPacket(127, 127, 1), # Max values
    ])
def test_spinn_encode(board, dvs_pkt, mode, log):
    """Test that board encodes a packet the same as the test system"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.forward_spinn(0), RESPONSES["success"])

    # Set mode to the mode under test
    board_assert_equal(board.set_mode_spinn(mode.value), RESPONSES["success"])

    # For each packet, test all modes
    result = spinn_2_to_7(dvs_pkt, mode)

    # Send packet
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
