"""Configure test system for pytest"""

import pytest

# Import all fixtures from fixtures.py
from fixtures import log, board

def pytest_addoption(parser):
    """Adds options to pytest command line"""
    parser.addoption("-D", action="store", metavar="NAME",
                     help="only run tests with device name NAME.")

def pytest_configure(config):
    """Configures pytest with config fixture"""
    config.addinivalue_line(
        "markers",
        "dev(name): mark test to run only on named device")

def pytest_runtest_setup(item):
    """Modify test run per test"""
    devmarker = item.get_marker("dev")
    if devmarker is not None:
        devname = devmarker.args[0]
        devopt = item.config.getoption("-D")
        if devname != devopt:
            pytest.skip("test requires device {}".format(devname))
        if devname.startswith("not ") and devname.split(" ")[1] == devopt:
            pytest.skip("test cannot run with device {} in use".format(devname))
