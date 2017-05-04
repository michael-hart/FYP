"""File contains common methods for initialising logging"""

from datetime import datetime
import os
import logging


def init_loggers():
    """Sets up default logging behaviour for project"""

    # Create standard format for all outputs
    formatter = logging.Formatter(
                    '%(levelname)s::%(asctime)s::%(name)s %(message)s'
                )

    # Create stream handler for console printing
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(formatter)

    # Create file handler for log file
    try:
        os.makedirs('logs')
    except (TypeError, WindowsError):
        pass
    fh = logging.FileHandler(
            os.path.join(
                'logs',
                'edvs_driver_log_{}.txt'.format(
                    datetime.now().strftime('%Y-%m-%d_%H-%M-%S_%f'))))
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(formatter)

    # Add handlers to root logger
    root_log = logging.getLogger()
    root_log.addHandler(ch)
    root_log.addHandler(fh)
    root_log.setLevel(logging.DEBUG)
