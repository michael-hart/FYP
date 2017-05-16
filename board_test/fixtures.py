"""File containing fixtures for use in tests"""

import logging
import time
import pytest
from controller import Controller

@pytest.fixture(scope='module', autouse=True)
def log():
    """Auto-used fixture to set up and return loggers"""
    from loggers import init_loggers

    init_loggers()
    logger = logging.getLogger("Test")
    logger.setLevel(logging.DEBUG)

@pytest.yield_fixture
def board():
    """Returns an open controller connection to the board"""
    with Controller() as con:
        responding = con.get_responding()
        con.open(responding[0])
        yield con
        con.reset()
        # Allow board time to reset
        time.sleep(0.1)
