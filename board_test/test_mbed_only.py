"""File used to test if simple PC->MBED commands are working"""

from serial.tools import list_ports
from fixtures import mbed
from mbed_controller import MBED_ID

def test_comports():
    """Tests if there are open COM ports available"""
    assert [x.device for x in list_ports.comports()]

def test_id(mbed):
    """Tests if open MBED controller responds to ID request"""
    assert mbed.get_id() == MBED_ID

def test_empty_spinn(mbed):
    """Tests that no DVS packets sent to board means no SpiNNaker results"""

    # Note that test is flawed as no bytes returned gives the same result
    # as only a carriage return returned. This merely checks that there is
    # nothing in the buffer waiting to be sent
    packets = mbed.get_spinn()
    assert not packets
