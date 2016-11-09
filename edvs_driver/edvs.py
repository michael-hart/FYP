"""Contains driver for eDVS 4337 hardware used in final year project"""

import logging
import serial


class DVSException(Exception):
    """Custom exception for DVS Exceptions"""
    pass


class DVS:
    """DVS class is driver for eDVS device"""

    def __init__(self):
        """Initialise function for DVS class"""
        self._log = logging.getLogger("eDVS_driver")
        self._log.debug("Created DVS instance")


    def open(self, com):
        """Opens device on given COM port"""
        self._log.debug("Attempting to open COM port on %s", com)
        self.ctx = serial.Serial(com)
        self._log.debug("No exceptions; COM port open successfully")


    def open_default(self):
        """Opens second valid COM port in list"""
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

        # Raise exception if too few ports exist
        if len(valid) < 2:
            raise DVSException("Too few COM ports")

        # Open serial connection
        self._log.debug("Attempting to open COM port on %s", valid[1])
        self.ctx = serial.Serial(valid[1])
        self._log.debug("No exceptions; COM port opened successfully")

    def close(self):
        """Closes any currently open COM port"""
        if hasattr(self, ctx) and ctx is not None:
            ctx.close()
        self.ctx = None

if __name__=='__main__':
    from loggers import init_loggers
    init_loggers()
    dvs = DVS()
    dvs.open_default()

