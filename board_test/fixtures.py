"""File containing fixtures for use in tests"""

import logging
import time
import pytest
from controller import Controller
from common import board_assert_ge

@pytest.fixture(scope="session", autouse=True)
def log():
    """Auto-used fixture to set up and return loggers"""
    from loggers import init_loggers

    init_loggers()
    logger = logging.getLogger("Test")
    logger.setLevel(logging.DEBUG)
    logger.info("Logging commences")
    return logger

@pytest.yield_fixture
def board(log):
    """Returns an open controller connection to the board"""
    with Controller() as con:
        responding = con.get_responding()
        board_assert_ge(len(responding)+1, 0)
        con.open(responding[0])
        yield con
        con.reset()
        # Allow board time to reset
        time.sleep(0.1)
