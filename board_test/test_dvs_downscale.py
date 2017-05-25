"""Module to test that the downscaling of the DVS is correct """

import pytest
from common import board_assert_equal, board_assert_isinstance, SpiNNMode
from fixtures import board
from dvs_packet import DVSPacket
from controller import RESPONSES

# Generate the test arrays for 64x64, 32x32, 16x16

TOO_FEW_64 = [DVSPacket(0, 0, 1)]
JUST_ENOUGH_64 = [DVSPacket(0, 0, 1), DVSPacket(1, 1, 1)]
EQUAL_64 = [DVSPacket(0, 0, 1), DVSPacket(1, 1, 1), 
            DVSPacket(0, 1, 0), DVSPacket(1, 0, 0)]
MANY_NEG_64 = [DVSPacket(0, 1, 0), DVSPacket(1, 1, 0), DVSPacket(1, 0, 0)]

JUST_ENOUGH_32 = [DVSPacket(0, 0, 1), DVSPacket(3, 0, 1), DVSPacket(1, 1, 1),
                  DVSPacket(2, 1, 1), DVSPacket(1, 2, 1), DVSPacket(2, 2, 1),
                  DVSPacket(3, 0, 1), DVSPacket(3, 3, 1)]

TOO_FEW_32 = [DVSPacket(1, 0, 0), DVSPacket(2, 0, 1), DVSPacket(0, 1, 1),
              DVSPacket(2, 2, 0), DVSPacket(3, 2, 1), DVSPacket(1, 3, 0)]

EQUAL_32 = [DVSPacket(idx % 4, idx // 4, (idx & 0x1)) for idx in range(16)]
ALL_NEG_32 = [DVSPacket(idx % 4, idx // 4, 0) for idx in range(16)]
MANY_NEG_32 = ALL_NEG_32[1:7] + ALL_NEG_32[9:12] + ALL_NEG_32[13:15]

TOO_FEW_16 = [DVSPacket(2, 0, 1), DVSPacket(7, 0, 0), DVSPacket(5, 2, 0),
              DVSPacket(1, 3, 1), DVSPacket(3, 4, 0), DVSPacket(6, 4, 0),
              DVSPacket(1, 6, 1), DVSPacket(4, 6, 1)]

JUST_ENOUGH_16 = [DVSPacket(0, 0, 1), DVSPacket(3, 0, 1), DVSPacket(4, 0, 1),
                  DVSPacket(7, 0, 1), DVSPacket(1, 1, 1), DVSPacket(3, 1, 1),
                  DVSPacket(4, 1, 1), DVSPacket(6, 1, 1), DVSPacket(2, 2, 1),
                  DVSPacket(5, 2, 1), DVSPacket(0, 3, 1), DVSPacket(1, 3, 1),
                  DVSPacket(3, 3, 1), DVSPacket(4, 3, 1), DVSPacket(6, 3, 1),
                  DVSPacket(7, 3, 1), DVSPacket(0, 4, 1), DVSPacket(1, 4, 1),
                  DVSPacket(3, 4, 1), DVSPacket(4, 4, 1), DVSPacket(6, 4, 1),
                  DVSPacket(7, 4, 1), DVSPacket(2, 5, 1), DVSPacket(5, 5, 1),
                  DVSPacket(1, 6, 1), DVSPacket(3, 6, 1), DVSPacket(4, 6, 1),
                  DVSPacket(6, 6, 1), DVSPacket(0, 7, 1), DVSPacket(3, 7, 1),
                  DVSPacket(4, 7, 1), DVSPacket(7, 7, 1)]

EQUAL_16 = [DVSPacket(idx % 8, idx // 8, (idx & 0x1)) for idx in range(64)]
ALL_NEG_16 = [DVSPacket(idx % 8, idx // 8, 0) for idx in range(64)]
MANY_NEG_16 = ALL_NEG_16[:6] + ALL_NEG_16[8:22] + ALL_NEG_16[23:29] + \
              ALL_NEG_16[33:49] + ALL_NEG_16[56:]


def dvs_offset(pkt_list, x, y, _mod):
    base_x = x - (x % _mod)
    base_y = y - (y % _mod)
    return [DVSPacket(pkt.x + base_x, pkt.y + base_y, pkt.pol)
            for pkt in pkt_list]

def helper_check_downscale(board, pkt_list, exp):
    """Helper method to be used by remaining tests"""
    # Request one more than expected to be sure that there aren't extras
    request_packets = 1
    if exp is not None:
        request_packets += len(exp)

    # As we are expecting to time out on reads a lot, set the timeout lower
    tmp_timeout = board.ser.timeout
    board.ser.timeout = 0.2

    # Forward all packets to PC
    board_assert_equal(board.forward_dvs(0), RESPONSES["success"])

    response_list = []
    rx_pkt_list = []

    # Send all packets
    for pkt in pkt_list:
        board_assert_equal(board.use_dvs(pkt), RESPONSES["success"])
        # Check for any output from dvs
        rx_pkt = board.get_dvs()
        if rx_pkt:
            rx_pkt_list += [rx_pkt]

    # Compare to expected
    if exp is None:
        assert rx_pkt_list == []
    else:
        assert rx_pkt_list
        for rx_pkt in rx_pkt_list:
            print("{}, {}, {}".format(rx_pkt.x, rx_pkt.y, rx_pkt.pol))
        assert len(rx_pkt_list) == len(exp)
        for pkt, rx_pkt in zip(exp, rx_pkt_list):
            board_assert_isinstance(rx_pkt, DVSPacket)
            board_assert_equal(rx_pkt.x, pkt.x)
            board_assert_equal(rx_pkt.y, pkt.y)
            board_assert_equal(rx_pkt.pol, pkt.pol)

    # Reset board timeout
    board.ser.timeout = tmp_timeout


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
    (TOO_FEW_64, None),

    # Two positive packets is enough
    (JUST_ENOUGH_64, [DVSPacket(0, 0, 1)]),
    
    # 2 positive, 2 negative gives 1 positive, 1 negative
    (EQUAL_64, [DVSPacket(0, 0, 1), DVSPacket(0, 0, 0)]),

    # # Triple negative is 1 negative, last ignored
    (MANY_NEG_64, [DVSPacket(0, 0, 0)]),

    # # Repeat all above tests with dvs_offset of x=47, y=93
    (dvs_offset(TOO_FEW_64, 47, 93, 2), None),
    (dvs_offset(JUST_ENOUGH_64, 47, 93, 2), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 2)),
    (dvs_offset(EQUAL_64, 47, 93, 2), 
     dvs_offset([DVSPacket(0, 0, 1), DVSPacket(0, 0, 0)], 47, 93, 2)),
    (dvs_offset(MANY_NEG_64, 47, 93, 2), 
     dvs_offset([DVSPacket(0, 0, 0)], 47, 93, 2)),
])
def test_res_64(board, pkt_list, exp):
    """Tests that resolution 64x64 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_64.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)


@pytest.mark.parametrize("pkt_list,exp", [
    (TOO_FEW_32, None),
    (JUST_ENOUGH_32, [DVSPacket(0, 0, 1)]),
    (EQUAL_32, [DVSPacket(0, 0, 0)]),
    (MANY_NEG_32, [DVSPacket(0, 0, 0)]),

    (dvs_offset(TOO_FEW_32, 47, 93, 4), None),
    (dvs_offset(JUST_ENOUGH_32, 47, 93, 4), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 4)),
    (dvs_offset(EQUAL_32, 47, 93, 4), 
     dvs_offset([DVSPacket(0, 0, 0)], 47, 93, 4)),
    (dvs_offset(MANY_NEG_32, 47, 93, 4), 
     dvs_offset([DVSPacket(0, 0, 0)], 47, 93, 4)),
])
def test_res_32(board, pkt_list, exp):
    """Tests that resolution 32x32 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_32.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)


@pytest.mark.parametrize("pkt_list,exp", [
    (TOO_FEW_16, None),
    (JUST_ENOUGH_16, [DVSPacket(0, 0, 1)]),
    (EQUAL_16, [DVSPacket(0, 0, 0)]),
    (MANY_NEG_16, [DVSPacket(0, 0, 0)]),

    (dvs_offset(TOO_FEW_16, 47, 93, 8), None),
    (dvs_offset(JUST_ENOUGH_16, 47, 93, 8), 
     dvs_offset([DVSPacket(0, 0, 1)], 47, 93, 8)),
    (dvs_offset(EQUAL_16, 47, 93, 8), 
     dvs_offset([DVSPacket(0, 0, 0)], 47, 93, 8)),
    (dvs_offset(MANY_NEG_16, 47, 93, 8), 
     dvs_offset([DVSPacket(0, 0, 0)], 47, 93, 8)),
])
def test_res_16(board, pkt_list, exp):
    """Tests that resolution 16x16 only outputs packets in specific cases"""
    board_assert_equal(board.set_mode_spinn(SpiNNMode.SPINN_MODE_16.value),
                       RESPONSES["success"])
    helper_check_downscale(board, pkt_list, exp)
