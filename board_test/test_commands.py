"""File used to test if simple PC->Board commands are working"""

import pytest
from serial.tools import list_ports
from controller import BOARD_ID, RESPONSES
from fixtures import board
from common import board_assert, board_assert_equal

def test_comports():
    """Tests if there are open COM ports available"""
    assert len([x.device for x in list_ports.comports()]) > 0

def test_id(board):
    """Tests if open controller responds to ID request"""
    board_assert_equal(board.get_id(), BOARD_ID)


@pytest.mark.parametrize("test_str", [
    "a0",
    "a"*34,
    "a"*38,
    "a"*8 + "b"*8 + "c"*8 + "d"*8 + "e"*8 + "f"*8,
    "g"*200,
    '\r',
    '\r'*5,
    ])
def test_echo(board, test_str):
    """Tests echo functionality of board"""
    board_assert_equal(board.echo(test_str), test_str)

def test_bad_cmd(board, log):
    """Tests whether a non-existent command gives an error message"""
    board._write("bad_cmd")

    # Log error code
    resp_msg = board._read()
    if resp_msg in RESPONSES.values():
        log.info("Response received: " + resp_msg)

    board_assert_equal(resp_msg, RESPONSES["bad_cmd"])

def test_reset(board):
    """Tests whether the reset command is accepted"""
    reset_result = board.reset()
    # If any result is retrieved, reset has failed
    board_assert(reset_result not in RESPONSES.values())
