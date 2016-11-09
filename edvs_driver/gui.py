"""Methods for retrieving information from eDVS and displaying to canvas"""

import logging
from loggers import init_loggers


def main():
    init_loggers()
    _log = logging.getLogger('eDVS_Driver')
    _log.info("Attempting to initialise eDVS")


if __name__ == '__main__':
    main()
