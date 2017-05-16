"""Tests forwarding of DVS packets to PC"""

from fixtures import board, log
from common import board_assert_equal, board_assert_ge, board_assert_le


def test_fwd_permanent_on(board):
    """Tests that turning forwarding on permanently works"""
    board_assert_equal(board.forward_dvs(0), True)

def test_fwd_reset(board):
    """Tests that resetting the forwarding works"""
    board_assert_equal(board.reset_dvs(), True)

def test_fwd_temp(board):
    """Tests that setting temporary forwarding is accepted"""
    board_assert_equal(board.forward_dvs(1000), True)

def test_fwd_packets(board, log):
    """Tests that board receives actual DVS packets"""
    board_assert_equal(board.forward_dvs(1000), True)
    tmp_timeout = board.ser.timeout
    board.ser.timeout = 1
    log.info("Attempting to read 4 bytes for 1 packet")
    pkt = board._read(4)
    board_assert_ge(len(pkt), 4)
    board_assert_ge(pkt[0], 0)
    board_assert_ge(pkt[1], 0)
    board_assert_le(pkt[2], 1)
    board.ser.timeout = tmp_timeout
