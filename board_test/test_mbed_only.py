"""File used to test if simple PC->MBED commands are working"""

from serial.tools import list_ports
import time
from fixtures import mbed, log
from mbed_controller import MBED_ID
from dvs_packet import DVSPacket
from spinn_packet import SpiNNPacket
from common import spinn_2_to_7, SpiNNMode


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
    (duration, packets) = mbed.get_spinn()
    assert duration == 0
    assert not packets

def test_wait(mbed):
    """Tests that telling the MBED to wait doesn't log values"""
    mbed.wait()

    # Send a simulated packet
    mbed.send_sim(spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128))

    # Check that nothing is recorded
    (_, packets) = mbed.get_spinn()
    assert not packets

def test_trigger(mbed):
    """Tests that triggering the MBED allows values to be captured"""
    mbed.trigger()

    # Send a simulated packet
    mbed.send_sim(spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128))

    # Check that packet is returned
    (_, packet) = mbed.get_spinn()
    assert packet


def test_wait_trigger(mbed):
    """Tests that triggering after a wait allows capture"""

    # Initial wait command
    mbed.wait()

    # Send a simulated packet
    mbed.send_sim(spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128))

    # Check that nothing is recorded
    (_, packets) = mbed.get_spinn()
    assert not packets

    # Trigger recording again
    mbed.trigger()

    # Send a simulated packet
    mbed.send_sim(spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128))

    # Check that packet is returned
    (_, packet) = mbed.get_spinn()
    assert packet

def test_read_clears(mbed):
    """Tests that reading a packet from the MBED then resetting works"""
    # Ensure MBED is empty
    _ = mbed.get_spinn()

    # Send a simulated packet
    mbed.send_sim(spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128))

    # Check that there is a returned value
    (_, packets) = mbed.get_spinn()
    assert packets

    # Read again and check that there is nothing
    (_, packets) = mbed.get_spinn()
    assert not packets    


def test_sim_spinn(mbed, log):
    """Tests that a single SpiNNaker packet is sent back correctly"""

    # Ensure we are triggered
    mbed.trigger()

    # Construct packet
    pkt = spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128)

    # Send simulated packet
    mbed.send_sim(pkt)

    # Retrieve packet and check
    (_, rx_pkt) = mbed.get_spinn()
    assert rx_pkt
    log.info("Got packet %s", rx_pkt[0].data)
    log.info("Expected   %s", pkt.data)
    assert rx_pkt[0].data == pkt.data

def test_sim_duration(mbed):
    """Tests that sending two packets roughly 100ms apart gives a duration"""
    
    # Ensure we are triggered
    mbed.trigger()

    # Construct packet
    pkt = spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128)

    # Send simulated packet twice
    mbed.send_sim(pkt)
    time.sleep(0.1)
    mbed.send_sim(pkt)

    # Retrieve packet and check
    (duration, _) = mbed.get_spinn()
    assert duration
    # Check duration is +/- 40ms
    assert 60000 <= duration <= 140000

def test_sim_many_packets(mbed):
    """Tests that sending many packets, each 10ms apart, gives a duration"""

    # Ensure we are triggered
    mbed.trigger()

    # Construct packets
    pkts = []
    pkts += [spinn_2_to_7(DVSPacket(10, 20, 1), SpiNNMode.SPINN_MODE_128)]
    pkts += [spinn_2_to_7(DVSPacket(43, 19, 1), SpiNNMode.SPINN_MODE_128)]
    pkts += [spinn_2_to_7(DVSPacket(131, 0, 1), SpiNNMode.SPINN_MODE_128)]
    pkts += [spinn_2_to_7(DVSPacket(85, 67, 1), SpiNNMode.SPINN_MODE_128)]
    pkts += [spinn_2_to_7(DVSPacket(127, 127, 1), SpiNNMode.SPINN_MODE_128)]

    for pkt in pkts:
        mbed.send_sim(pkt)
        time.sleep(0.01)

    # Retrieve all packets and check their contents and total duration
    (duration, rx_pkts) = mbed.get_spinn()

    assert duration
    # Check duration is +/- 40ms
    assert (len(pkts)-4) * 10000 <= duration <= (len(pkts)+4) * 10000

    assert rx_pkts
    for true_pkt, rx_pkt in zip(pkts, rx_pkts):
        assert true_pkt.data == rx_pkt.data
