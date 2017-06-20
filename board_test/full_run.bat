@echo off
echo Beginning test run - no peripherals
py.test -vv --html=reports/test_results_none.html
echo Attach eDVS, then press reset
pause
py.test -vv --html=reports/test_results_edvs.html -D edvs -k dvs_packets
echo Detach eDVS, ensure switches for MBED are ON, press reset
pause
py.test -vv --html=reports/test_results_mbed.html -D mbed test_mbed_only.py test_board_mbed.py -k "not single_tx and not many_tx"
echo Press STM reset button
pause
py.test -vv --html=reports/test_results_mbed_single_tx.html -D mbed -k single_tx
echo Press STM reset button
pause
py.test -vv --html=reports/test_results_mbed_many_tx.html -D mbed -k many_tx
echo Test run finished
pause
