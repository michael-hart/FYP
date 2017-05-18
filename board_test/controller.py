"""Controller to control the interface board, to be used for testing the board"""

# Import statements
from binascii import hexlify
import math
import struct
import logging
import serial
from serial.tools import list_ports
from dvs_packet import DVSPacket

# Constant definitions
BAUD_RATE = 500000
DEST_BUF_SIZE = 40
BOARD_ID = "Interface"
COMMANDS = {
    "get_id": "id  ",
    "echo": "echo",
    "reset": "rset",
    "dvs_forward": "fdvs",
    "dvs_reset": "rdvs",
    "dvs_use": "udvs",
    "spinn_forward": "fspn",
    "spinn_reset": "rspn",
    "spinn_set_mode": "mspn",
}
RESPONSES = {
    "success": "000 Success",
    "bad_cmd": "001 Not recognised",
    "bad_len": "002 Wrong length",
    "bad_param": "003 Bad parameter",
}
ECHO_ON = True

class Controller(object):
    """Controller class that supports with statements for connecting to board"""

    def __init__(self):
        """Set up object attributes including loggers"""
        self.ser = None
        self.log = logging.getLogger("Controller")
        self.expected = 0
        self.returns = 0
        self.echo_returns = 0

    def get_responding(self):
        """Checks all connected Windows COM ports for responding device"""
        responding = []
        for port in list_ports.comports()[1:]:
            self.open(port.device)

            if self.get_id() == BOARD_ID:
                responding += [port.device]
                self.log.debug("Responding device found on port " + port.device)

            self.ser.close()
            self.ser = None

        return responding

    def _write(self, msg):
        """Helper method to write bytes and log"""
        if self.ser != None:
            # Add carriage return to signify end of command

            tx_msg = bytes([ord(x) for x in msg + '\r'])
            self.log.debug("<<< {} : \'{}\'".format(hexlify(tx_msg), msg))
            self.ser.write(tx_msg)

            # Track how many packets we're expecting to be echoed back
            if ECHO_ON:
                self.expected += 1
                # Track how many carriage returns were just sent
                self.returns = msg.count('\r')

            # Special case where we have requested echo
            if msg.startswith(COMMANDS["echo"]):
                self.echo_returns += msg.count('\r')

    def _read(self, echo_expected=False):
        """Helper method to read packet"""
        buf = ""
        raw = 0
        char = 0

        # Handle echoed bytes by recursively discarding packets
        if self.expected > 0:
            self.expected -= 1
            self._read()

        if self.ser != None:
            while char != '\r':
                raw = self.ser.read(1)
                if len(raw) > 0:
                    char = chr(struct.unpack("<B", raw)[0])
                else:
                    # If string is empty, we timed out, so return
                    break
                buf += char

                if echo_expected:
                    if self.echo_returns > 0 and char == '\r':
                        self.echo_returns -= 1
                        char = 0
                        continue
                    else:
                        self.echo_expected = False

                if self.returns > 0 and char == '\r':
                    char = 0
                    self.returns -= 1

            log_buf = bytes([ord(x) for x in buf])
            self.log.debug(">>> {} : {}".format(hexlify(log_buf), log_buf))

            if len(buf) > 0:
                return buf[:-1]
        return ""

    def open(self, port):
        """Connects to the given port at default baud rate"""
        # Open specified port
        self.ser = serial.Serial(port, baudrate=BAUD_RATE, timeout=0.001)

        # Flush any existing bytes in the buffer
        flushed = self.ser.read(1000)
        self.log.debug(">>> {} : {}".format(hexlify(flushed), flushed))

        # Set a higher timeout after buffer flush
        self.ser.timeout = 0.1

        self.log.debug("Opened serial port " + port)

    def get_id(self):
        """Retrieves currently open device ID"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""

        # Write get ID and get expected length of ID
        self._write(COMMANDS["get_id"])
        resp_msg = self._read()
        if resp_msg in RESPONSES.values():
            self.log.info("Response received: " + resp_msg)
        return self._read()

    def echo(self, msg):
        """Requests that the board echoes back the given bytes"""

        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""

        # Write the command ID and the number of bytes to echo
        buf = ""
        # data size is buffer size minus command, length, and \r
        data_size = DEST_BUF_SIZE - 7
        for i in range(math.ceil(len(msg)/data_size)):
            tx_msg = COMMANDS["echo"]
            tx_len = min(data_size, len(msg) - i*data_size)
            tx_msg += chr(tx_len)
            tx_msg += msg[i*data_size:i*data_size+tx_len]
            self._write(tx_msg)

            # Log error code
            resp_msg = self._read()
            if resp_msg in RESPONSES.values():
                self.log.info("Response received: " + resp_msg)

            # Read echoed packet
            rx_msg = self._read(echo_expected=True)
            buf += rx_msg

        return buf

    def reset(self):
        """Resets the board. Returns False if command not recognised"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        self._write(COMMANDS["reset"])

        resp_msg = self._read()
        if resp_msg in RESPONSES.values():
            self.log.info("Response received: " + resp_msg)

        return resp_msg

    def forward_dvs(self, timeout_ms):
        """Request forwarding for timeout_ms, or 0 for permanently on"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        tx_msg = COMMANDS["dvs_forward"]
        tx_msg += chr((timeout_ms & 0xFF00) >> 8)
        tx_msg += chr(timeout_ms & 0xFF)
        self._write(tx_msg)

        # Log error code
        resp_msg = self._read()
        if resp_msg in RESPONSES.values():
            self.log.info("Response received: " + resp_msg)

        return resp_msg

    def reset_dvs(self):
        """Cancels forwarding for DVS packets"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        self._write(COMMANDS["dvs_reset"])

        # Log error code
        resp_msg = self._read()
        if resp_msg in RESPONSES.values():
            self.log.info("Response received: " + resp_msg)

        return resp_msg

    def get_dvs(self):
        """Attempts to retrieve a DVS packet if it exists"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""

        self.log.info("Attempting to read 3 bytes for 1 packet")

        # Retrieve packet or time out
        pkt = self._read()
        if len(pkt) >= 3:
            return DVSPacket(pkt[0], pkt[1], pkt[2])
        else:
            return None

    def use_dvs(self, pkt):
        """Sends given packet as simulated DVS message"""

        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""

        # Put packet data into buffer and send
        buf = [pkt.x, pkt.y, pkt.pol]
        str_buf = COMMANDS["dvs_use"] + ''.join([chr(x) for x in buf])
        self._write(str_buf)

        # Log error code
        resp_msg = self._read()
        if resp_msg in RESPONSES.values():
            self.log.info("Response received: " + resp_msg)

        return resp_msg


    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if self.ser is not None:
            self.ser.close()
        if value != None:
            self.log.error("Received exception: {}".format(value))
        self.log.info("Controller closing down...")
        return True

def main():
    """Simple tests to see if controller is working"""
    from loggers import init_loggers

    init_loggers()
    logger = logging.getLogger("Main")
    logger.setLevel(logging.DEBUG)
    ports = [x.device for x in list_ports.comports()]
    logger.info("COM ports are: " + str(ports))

    with Controller() as con:
        responding = con.get_responding()
        logger.info("Responding: " + str(responding))
        if len(responding) == 0:
            logger.error("No responding COM port found")
            raise Exception("No responding COM port found")
        con.open(responding[0])
        assert con.echo("a3") == "a3"
        assert con.echo("Test String") == "Test String"

if __name__ == '__main__':
    main()
