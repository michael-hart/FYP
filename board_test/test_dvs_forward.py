"""Tests forwarding of DVS packets to PC"""

from fixtures import board, log
from common import (board_assert_equal, board_assert_ge, board_assert_le,
                    board_assert_isinstance)
from controller import RESPONSES
from dvs_packet import DVSPacket

def test_fwd_permanent_on(board):
    """Tests that turning forwarding on permanently works"""
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])

def test_fwd_reset(board):
    """Tests that resetting the forwarding works"""
    board_assert_equal(board.reset_dvs(), RESPONSES["success"])

def test_fwd_temp(board):
    """Tests that setting temporary forwarding is accepted"""
    board_assert_equal(board.forward_dvs(1000), RESPONSES["success"])

def test_fwd_packets(board):
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
