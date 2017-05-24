"""Module to test that the downscaling of the DVS is correct """

import pytest
from common import board_assert_equal, board_assert_isinstance, SpiNNMode
from fixtures import board
from dvs_packet import DVSPacket
from controller import RESPONSES

def helper_check_downscale(board, pkt_list, exp):
    """Helper method to be used by remaining tests"""
    # Request one more than expected to be sure that there aren't extras
    request_packets = 1
    if exp is not None:
        request_packets += len(exp)

    # Forward all packets to PC
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])

    response_list = []

    # Send all packets
    for pkt in pkt_list:
        response_list += [board.use_dvs(pkt)]

    # Retrieve further packets
    for _ in range(request_packets):
        response_list += board._read()

    # DVS packets are mixed with error codes, so need to extract them

    # Filter out error codes, asserting there is no error
    for response in RESPONSES:
        if response != RESPONSES["success"]:
            assert response not in response_list
        response_list = [resp for resp in response_list if resp != response]

    # Convert remaining list to DVSPackets
    rx_pkt_list = [DVSPacket(ord(resp[0]), ord(resp[1]), ord(resp[2])) 
                   for resp in response_list]

    # Compare to expected
    assert len(rx_pkt_list) == len(exp)
    for pkt, rx_pkt in zip(exp, rx_pkt_list):
        board_assert_isinstance(rx_pkt, DVSPacket)
        board_assert_equal(rx_pkt.x, pkt.x)
        board_assert_equal(rx_pkt.y, pkt.y)
        board_assert_equal(rx_pkt.pol, pkt.pol)


def test_res_128(board):
    """Test that resolution 128x128 is unchanged"""

    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_128.value),
                       RESPONSES["success"])

    # Forward all packets to PC
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])

    # Send several 128x128 DVS Packets
    for param in range(10, 120, 20):
        pkt = DVSPacket(param, param, param%2)
        board_assert_equal(board.use_dvs(pkt), RESPONSES["success"])
        rx_pkt = board.get_dvs()
        board_assert_isinstance(rx_pkt, DVSPacket)
        board_assert_equal(rx_pkt.x, pkt.x)
        board_assert_equal(rx_pkt.y, pkt.y)
        board_assert_equal(rx_pkt.pol, pkt.pol)


@pytest.mark.parametrize("pkt_list,exp", [
    # One packet not enough
    ([DVSPacket(0, 0, 1)], None),

    # Two positive packets is enough
    # ([DVSPacket(0, 0, 1), DVSPacket(1, 1, 1)], [DVSPacket(0, 0, 1)]),
    
    # # 2 positive, 2 negative gives 1 positive, 1 negative
    # ([DVSPacket(0, 0, 1), DVSPacket(1, 1, 1), 
    #   DVSPacket(0, 1, 0), DVSPacket(1, 0, 0)], 
    #  [DVSPacket(0, 0, 1), DVSPacket(0, 0, 0)]),

    # # Triple negative is 1 negative, last ignored
    # ([DVSPacket(0, 1, 0), DVSPacket(1, 1, 0), DVSPacket(1, 0, 0)],
    #  [DVSPacket(0, 0, 0)]),

    # # Repeat all above tests with offset of x=46, y=92
    # ([DVSPacket(46, 92, 1)], None),
    # ([DVSPacket(46, 92, 1), DVSPacket(47, 93, 1)], [DVSPacket(46, 92, 1)]),
    # ([DVSPacket(46, 92, 1), DVSPacket(47, 93, 1), 
    #   DVSPacket(46, 93, 0), DVSPacket(47, 92, 0)], 
    #  [DVSPacket(46, 92, 1), DVSPacket(46, 92, 0)]),
    # ([DVSPacket(46, 93, 0), DVSPacket(47, 93, 0), DVSPacket(47, 93, 0)],
    #  [DVSPacket(46, 92, 0)]),
    ])
def test_res_64(board, pkt_list, exp):
    """Tests that resolution 64x64 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_64.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)


@pytest.mark.parametrize("pkt_list,exp", [])
def test_res_32(board, pkt_list, exp):
    """Tests that resolution 32x32 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_32.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)


@pytest.mark.parametrize("pkt_list,exp", [])
def test_res_16(board, pkt_list, exp):
    """Tests that resolution 16x16 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_16.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)
