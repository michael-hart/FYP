"""Module containing controller class specific to MBED device"""

# Import statements
from binascii import hexlify
import struct
import logging
import serial
from serial.tools import list_ports
from spinn_packet import SpiNNPacket, SPINN_PACKET_SHORT

# Constant definitions
BAUD_RATE = 500000
MBED_ID = "MBED"
COMMANDS = {
    "get_id": "id  ",
    "get_spinn": "gspn",
    # TODO
}

class MBEDController(object):
    """Controller class that supports with statements for connecting with mbed"""

    def __init__(self):
        """Set up object attributes including loggers"""
        self.ser = None
        self.log = logging.getLogger("MBED")
        self.expected = 0

    def get_responding(self):
        """Checks all connected Windows COM ports for responsing devices"""
        responding = []
        for port in list_ports.comports()[1:]:
            self.open(port.device)

            if self.get_id() == MBED_ID:
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
            self.log.debug("<<< %s : \'%s\'", hexlify(tx_msg), msg)
            self.ser.write(tx_msg)

    def _read(self):
        """Helper method to read packet"""
        buf = ""
        raw = 0
        char = 0

        if self.ser != None:
            while char != '\r':
                raw = self.ser.read(1)
                if raw:
                    char = chr(struct.unpack("<B", raw)[0])
                else:
                    # If string is empty, we timed out, so return
                    break
                buf += char

            log_buf = bytes([ord(x) for x in buf])
            self.log.debug(">>> %s : %s", hexlify(log_buf), log_buf)

            if buf:
                return buf[:-1]
        return ""

    def open(self, port):
        """Connects to the given port at default baud rate"""
        # Open specified port
        self.ser = serial.Serial(port, baudrate=BAUD_RATE, timeout=0.001)

        # Flush any existing bytes in the buffer
        flushed = self.ser.read(1000)
        self.log.debug(">>> %s : %s", hexlify(flushed), flushed)

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
        return self._read()

    def get_spinn(self):
        """Returns list of SpiNNPackets retrieved from MBED"""
        if self.ser is None:
            self.log.error("No serial device connected!")
            return ""

        # Write get SpiNN packets command
        self._write(COMMANDS["get_spinn"])
        raw = self._read()

        # Remove first 4 bytes as this is the duration
        raw_duration = bytes([ord(x) for x in raw[:4]])
        duration = struct.unpack(">I", raw_duration)[0]
        raw = raw[4:]

        # Package bytes into SpiNNPackets
        packets = []
        while len(raw) >= SPINN_PACKET_SHORT:
            packets += SpiNNPacket(raw[:SPINN_PACKET_SHORT])
            raw = raw[SPINN_PACKET_SHORT:]

        return (duration, packets)

    def __enter__(self):
        return self

    def __exit__(self, _type, _value, traceback):
        if self.ser is not None:
            self.ser.close()
        if _value != None:
            self.log.error("Received exception: %s", _value)
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

    with MBEDController() as con:

        responding = con.get_responding()
        logger.info("Responding: " + str(responding))
        if not responding:
            logger.error("No responding COM port found")
            raise Exception("No responding COM port found")
        con.open(responding[0])
        assert con.get_id() == MBED_ID

if __name__ == '__main__':
    main()
