"""Contains driver for eDVS 4337 hardware used in final year project"""

import binascii
import logging
import serial
import time
from Queue import Queue
from serial.threaded import LineReader, ReaderThread

# Define constants
DVS_BAUD_RATE = 115200


class DVSReader(LineReader):
    """Threaded Protocol instance to read and display data from serial port"""

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger("DVSReader")
        super(DVSReader, self).__init__(*args, **kwargs)
        self.TERMINATOR = b'\n'

    def connection_made(self, transport):
        super(DVSReader, self).connection_made(transport)
        self._log.info("Opened port successfully")

    def handle_line(self, data):
        self._log.debug(">>> %s :: RAW %s" % (data, binascii.hexlify(data.encode('utf-8'))))

    def connection_lost(self, exc):
        if exc:
            self._log.exception(exc)
        self._log.info("Port closed")

    def led_set(self, led_state):
        """Sets LED state to the given number"""
        assert type(led_state) == int
        assert 0 <= led_state <= 3
        self.write_line("!L{}".format(led_state))


class DVSReaderThread(ReaderThread):

    def __init__(self, *args, **kwargs):
        """Override initialise to create standard serial object"""
        self._log = logging.getLogger("DVSReaderThread")

        # Get list of valid COM ports
        valid_ports = self.list_ports()
        # Raise exception if too few ports exist
        if len(valid_ports) < 2:
            raise DVSException("Too few COM ports")
        ser = serial.Serial(valid_ports[1], baudrate=DVS_BAUD_RATE, parity=serial.PARITY_NONE,
                            stopbits=serial.STOPBITS_ONE, rtscts=True)

        super(DVSReaderThread, self).__init__(ser, *args, **kwargs)

    def list_ports(self):
        """List the valid COM ports; currently Windows only"""
        # Get valid COM ports in list
        valid = []
        for i in range(256):
            try:
                port = "COM{}".format(i)
                s = serial.Serial(port)
                s.close()
                self._log.debug("Valid COM port found: %s", port)
                valid += [port]
            except (OSError, serial.SerialException):
                pass
        return valid


if __name__ == '__main__':
    from loggers import init_loggers
    init_loggers()
    logger = logging.getLogger("Main")
    with DVSReaderThread(DVSReader) as dvs:
        logger.info("LED Off")
        dvs.led_set(0)
        time.sleep(3)
        logger.info("LED On")
        dvs.led_set(1)
        time.sleep(3)
        logger.info("LED Blinking")
        dvs.led_set(2)
