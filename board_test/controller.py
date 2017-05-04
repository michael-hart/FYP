"""Controller to control the interface board, to be used for testing the board"""

# Import statements
import logging
import serial
from serial.tools import list_ports
import time
from binascii import hexlify
import math

# Constant definitions
BAUD_RATE = 115200
BOARD_ID = "Interface"
COMMANDS = {
        "get_id": "id  ",
        "echo": "echo",
}

class Controller(object):
    """Controller class that supports with statements for connecting to board"""

    def __init__(self):
        """Set up object attributes including loggers"""
        self.ser = None
        self.log = logging.getLogger("Controller")

    def get_responding(self):
        """Checks all connected Windows COM ports for responding device"""
        responding = []
        for port in list_ports.comports():
            self.open(port.device)

            # Write get ID and wait 10ms for response
            self._write(COMMANDS["get_id"])
            _id = self._read(len(BOARD_ID))

            if _id == BOARD_ID:
                responding += [port.device]
                self.log.debug("Responding device found on port " + port.device)

            self.ser.close()
            self.ser = None

        return responding

    def _write(self, msg):
        """Helper method to write bytes and log"""
        if self.ser != None:
            tx_msg = bytes(msg, 'utf-8')
            self.log.debug("<<< {} : \'{}\'".format(hexlify(tx_msg), msg))
            self.ser.write(tx_msg)

    def _read(self, length):
        """Helper method to read number of bytes and log"""
        if self.ser != None:
            rx_msg = self.ser.read(length)
            self.log.debug(">>> {} : {}".format(hexlify(rx_msg), rx_msg))
            return rx_msg.decode('utf-8')
        return ""

    def open(self, port):
        """Connects to the given port at default baud rate"""
        # Open specified port
        self.ser = serial.Serial(port, baudrate=BAUD_RATE, timeout=5)

        # Print out any existing bytes in the buffer
        self._read(1000)

        self.log.debug("Opened serial port " + port)

    def echo(self, msg):
        """Requests that the board echoes back the given bytes"""

        if self.ser == None:
            self.log.error("No serial device connected!")
            return ""

        # Write the command ID and the number of bytes to echo
        buf = ""
        for i in range(math.ceil(len(msg)/256)):
            tx_msg = COMMANDS["echo"]
            tx_len = min(256, len(msg) - i*256)
            tx_msg += chr(tx_len)
            tx_msg += msg[i*256:i*256+tx_len]
            self._write(tx_msg)
            rx_msg = self._read(tx_len)
            buf += rx_msg

        return buf

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if self.ser != None:
            self.ser.close()
        if not value:
            self.log.error("Received exception: {}".format(value))
        self.log.info("Controller closing down...")
        return True


if __name__ == '__main__':
    # Do some stuff that actually tests whether stuff is working
    from loggers import init_loggers

    init_loggers()
    logger = logging.getLogger("Main")
    logger.setLevel(logging.DEBUG)
    ports = [x.device for x in list_ports.comports()]
    logger.info("COM ports are: " + str(ports))

    with Controller() as c:
        # responding = c.get_responding()
        # print(responding)
        # if len(responding) == 0:
        #     logger.error("No responding COM port found")
        #     raise Exception("No responding COM port found")
        # c.open(responding[0])
        c.open("COM4")
        assert c.echo("Test String") == "Test String"
 
