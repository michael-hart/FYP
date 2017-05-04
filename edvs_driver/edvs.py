"""Contains driver for eDVS 4337 hardware used in final year project"""

import binascii
from enum import Enum
from itertools import izip_longest
import logging
import os
import serial
import time
from Queue import Queue
from serial.threaded import LineReader, ReaderThread
import yaml


EDVS_CONFIG = os.path.join(os.path.split(__file__)[0], "edvs_config.yaml")


class EDVSState(Enum):
    COMMAND = 0
    EVENT = 1


def grouper(iterable, n, fillvalue=None):
    args = [iter(iterable)] * n
    return izip_longest(*args, fillvalue=fillvalue)


def decode_events(raw, evt_fmt):
    """Decodes raw given format num_bytes"""
    # Reduce list to correct size
    raw += '\n'
    # leftover_size = len(raw) % (evt_fmt + 2)
    # data = raw[:-leftover_size]
    # leftover = raw[-leftover_size:]
    leftover = []
    events = []
    index = 0
    while index < len(raw):
        group = [ord(i) for i in raw[index:index+2]]
        if group[0] & 0x80 == 0:
            index += 1
            continue
        y = group[0] & 0x7F
        x = group[1] & 0x7F
        polarity = (group[1] & 0x80) > 0
        events += [(x, y, polarity)]
        index += 2
    # Let each event simply be x,y co-ordinates with up=True or False
    # for group in grouper(raw, evt_fmt + 2, '0'):
    #     # Check that first byte starts with 1, else it is not event data
    #     if ord(group[0]) & 0x80 == 0:
    #         continue
    #     # Format is 1yyyyyyy.pxxxxxxx
    #     y = ord(group[0]) & 0x7F
    #     x = ord(group[1]) & 0x7F
    #     polarity = (ord(group[1]) & 0x80) > 0
    #     events += [(x, y, polarity)]

    return (events, leftover)


class DVSException(Exception):
    """Custom exception for DVS"""
    pass


class DVSReader(LineReader):
    """Threaded Protocol instance to read and display data from serial port"""

    listeners = []
    buf = ''
    state = EDVSState.COMMAND

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger("DVSReader")
        super(DVSReader, self).__init__(*args, **kwargs)
        self.edvs_config = yaml.load(file(EDVS_CONFIG))
        self.TERMINATOR = b'\n'

    def connection_made(self, transport):
        super(DVSReader, self).connection_made(transport)
        self._log.info("Opened port successfully")
        # Set some default params
        self.serial = transport.serial
        self.write_line("!U{}".format(self.edvs_config['echo']))
        self.set_baud_rate(self.edvs_config['baud'])
        self.set_event_bytes(self.edvs_config['event_bytes'])
        self.set_event_sending(False)

    def handle_line(self, data):
        self._log.debug(">>> %s :: RAW %s" %
                        (data, binascii.hexlify(data.encode('utf-8'))))
        if self.state == EDVSState.EVENT:
            # Attempt to decode as event data
            data = self.buf + data
            event_data, self.buf = decode_events(data,
                                                 self.edvs_config['event_bytes'])
            if event_data != []:
                self._log.debug("Events decoded: {}".format(event_data))
                for listener in self.listeners:
                    listener(event_data)

    def write_line(self, data):
        self._log.debug("<<< %s :: RAW %s" %
                        (data, binascii.hexlify(data.encode('utf-8'))))
        super(DVSReader, self).write_line(data)

    def connection_lost(self, exc):
        if exc:
            self._log.exception(exc)
        with open('edvs_config.yaml', 'w') as f:
            f.write("# Default configuration for EDVS\n")
            yaml.dump(self.edvs_config, f, default_flow_style=False)
        self._log.info("Port closed")

    def add_event_listener(self, listener):
        self.listeners += [listener]

    def led_set(self, led_state):
        """Sets LED state to the given number"""
        assert type(led_state) == int
        assert 0 <= led_state <= 3
        self.write_line("!L{}".format(led_state))

    def set_baud_rate(self, rate):
        """Sets baud rate to given rate"""
        assert type(rate) == int
        self._log.info("Setting baud rate to %d" % rate)
        self.write_line("!U={}".format(rate))
        self.edvs_config['baud'] = rate
        self.serial.baudrate = rate

    def set_event_sending(self, enable):
        """Sets whether to send event data"""
        if enable:
            self.write_line("E+")
            self.state = EDVSState.EVENT
        else:
            self.write_line("E-")
            self.state = EDVSState.COMMAND

    def set_event_bytes(self, _bytes):
        """Sets format for number of bytes"""
        assert type(_bytes) == int
        self._log.info("Setting event bytes format to {}".format(_bytes))
        self.write_line("!E{}".format(_bytes))
        self.edvs_config['event_bytes'] = _bytes


class DVSReaderThread(ReaderThread):

    def __init__(self, *args, **kwargs):
        """Override initialise to create standard serial object"""
        self._log = logging.getLogger("DVSReaderThread")

        # Get list of valid COM ports
        valid_ports = self.list_ports()
        # Raise exception if too few ports exist
        if len(valid_ports) < 2:
            raise DVSException("Too few COM ports")
        ser = serial.Serial(valid_ports[1], parity=serial.PARITY_NONE,
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
        request_baud = 115200
        if dvs.edvs_config['baud'] != request_baud:
            dvs.set_baud_rate(request_baud)
        dvs.set_event_sending(True)

        def event_listener(data):
            for event in data:
                print "x:{} y:{} polarity:{}".format(
                    event[0], event[1], event[2])
        dvs.add_event_listener(event_listener)
        print("Listening for ten seconds")
        time.sleep(10)
