"""File containing common methods across tests"""

import time

WAIT_TIME = 1.5

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
