"""Controller to control the interface board, to be used for testing the board"""

# Import statements
import logging
import serial
from serial.tools import list_ports
import time
from binascii import hexlify
import math

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
}
RESPONSES = {
    "bad_cmd": "Not recognised",
}
ECHO_ON = True

class Controller(object):
    """Controller class that supports with statements for connecting to board"""

    def __init__(self):
        """Set up object attributes including loggers"""
        self.ser = None
        self.log = logging.getLogger("Controller")
        self.expected = 0

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
            tx_msg = bytes(msg + '\r', 'utf-8')
            self.log.debug("<<< {} : \'{}\'".format(hexlify(tx_msg), msg))
            self.ser.write(tx_msg)

            # Track how many bytes we're expecting to be echoed back
            if ECHO_ON:
                self.expected = self.expected + len(tx_msg)

    def _read(self, length):
        """Helper method to read number of bytes and log"""
        if self.ser != None:
            rx_msg = self.ser.read(self.expected + length)
            self.log.debug(">>> {} : {}".format(hexlify(rx_msg), rx_msg))
            msg = rx_msg[self.expected:]
            self.expected = 0
            return msg.decode('utf-8')
        return ""

    def open(self, port):
        """Connects to the given port at default baud rate"""
        # Open specified port
        self.ser = serial.Serial(port, baudrate=BAUD_RATE, timeout=0.001)

        # Flush any existing bytes in the buffer
        self._read(1000)

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
        _id = self._read(len(BOARD_ID) + 1)
        if len(_id) > 0:
            _id = _id[:-1]
        return _id

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
            # Read one extra for the carriage return
            rx_msg = self._read(tx_len + 1)
            if len(rx_msg) > 0:
                rx_msg = rx_msg[:-1]
            buf += rx_msg

        return buf

    def reset(self):
        """Resets the board. Returns False if command not recognised"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        self._write(COMMANDS["reset"])
        resp = self._read(len(RESPONSES["bad_cmd"]) + 1)
        if len(resp) > 0:
            resp = resp[:-1]
        return resp != RESPONSES["bad_cmd"]

    def forward_dvs(self, timeout_ms):
        """Request forwarding for timeout_ms, or 0 for permanently on"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        tx_msg = COMMANDS["dvs_forward"]
        tx_msg += chr(timeout_ms >> 8)
        tx_msg += chr(timeout_ms & 0xFF)
        self._write(tx_msg)
        resp = self._read(len(RESPONSES["bad_cmd"]) + 1)
        if len(resp) > 0:
            resp = resp[:-1]
        return resp != RESPONSES["bad_cmd"]

    def reset_dvs(self):
        """Cancels forwarding for DVS packets"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""
        self._write(COMMANDS["dvs_reset"])
        resp = self._read(len(RESPONSES["bad_cmd"]) + 1)
        if len(resp) > 0:
            resp = resp[:-1]
        return resp != RESPONSES["bad_cmd"]

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
