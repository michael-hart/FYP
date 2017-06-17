"""Tests forwarding of DVS packets to PC"""

import pytest
from fixtures import board, log
from common import (board_assert_equal, board_assert_ge, board_assert_le,
                    board_assert_isinstance)
from controller import RESPONSES
from dvs_packet import DVSPacket

def test_dvs_fwd_permanent_on(board):
    """Tests that turning forwarding on permanently works"""
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])

def test_dvs_fwd_reset(board):
    """Tests that resetting the forwarding works"""
    board_assert_equal(board.reset_dvs(), RESPONSES["success"])

@pytest.mark.dev("not edvs")
def test_dvs_fwd_on_reset(board):
    """Tests that turning on and then resetting forwarding works"""
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])
    board_assert_equal(board.reset_dvs(), RESPONSES["success"])

def test_dvs_fwd_temp(board):
    """Tests that setting temporary forwarding is accepted"""
    board_assert_equal(board.forward_dvs(1000), RESPONSES["success"])

@pytest.mark.dev("edvs")
def test_dvs_packets_received(board):
    """Tests that board receives actual DVS packets"""

    # Set new timeout to allow time for actual DVS to supply packets
    tmp_timeout = board.ser.timeout
    board.ser.timeout = 1

    # Retrieve and check packet
    board_assert_equal(board.forward_dvs(1000), RESPONSES["success"])
    pkt = board.get_dvs()
    board_assert_isinstance(pkt, DVSPacket)
    board_assert_le(pkt.x, 127)
    board_assert_le(pkt.y, 127)
    board_assert_le(pkt.pol, 1)

    # Reset timeout to previous
    board.ser.timeout = tmp_timeout

@pytest.mark.dev("not edvs")
def test_dvs_use_pkt(board):
    """Tests that sending simulated packet is sent back"""

    # Turn on forwarding to check that any packet comes back
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])
    # Send a test packet
    test_pkt = DVSPacket(10, 30, 1)
    board_assert_equal(board.use_dvs(test_pkt), RESPONSES["success"])
    pkt = board.get_dvs()
    board_assert_isinstance(pkt, DVSPacket)
    board_assert_equal(pkt.x, test_pkt.x)
    board_assert_equal(pkt.y, test_pkt.y)
    board_assert_equal(pkt.pol, test_pkt.pol)
