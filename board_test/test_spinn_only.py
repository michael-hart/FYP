"""Module to test SpiNNaker code with no DVS connected"""

import pytest
from common import (board_assert_equal, board_assert_ge, board_assert_le,
                    board_assert_isinstance)
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

# TODO: Test that different modes affect outcome of packets
