#include "mbed.h"

#define RX_BUF_SIZE (40)
#define BAUD_RATE (500000)

#define SPINN_BUF_SIZE (250)

/* PC Command definitions */
#define PC_CMD_ID         "id__"
#define PC_CMD_GET_SPINN  "gspn"
#define PC_CMD_WAIT       "wait"
#define PC_CMD_TRIGGER    "trig"
#define PC_CMD_SIM        "sim_"
#define PC_CMD_RESET      "rset"
#define PC_CMD_USE_SPINN  "uspn"
#define PC_CMD_TRIG_SPINN "tspn"

/* PC Response definitions */
#define PC_RESP_ID "MBED\r"

/* Serial definition */
Serial uart(p9, p10, 500000);

/* UART receive buffer */
char rx_buf[RX_BUF_SIZE];
int rx_buf_idx = 0;

/* Flag set to read port */
bool read_port = true;

/* SpiNNaker receive buffer */
uint8_t spinn_rx_buf[SPINN_BUF_SIZE];
int spinn_buf_idx = 0;

/* Timing variables */
Timer timer;
int start_time = 0;
int latest_time = 0;

/* Data storage for determining transitions */
uint8_t previous_spinn = 0;
uint8_t previous_difference = 0;
uint8_t current_spinn = 0;

/* GPIO setup for SpiNNaker link */
DigitalIn b0(p21);
DigitalIn b1(p22);
DigitalIn b2(p23);
DigitalIn b3(p24);
DigitalIn b4(p25);
DigitalIn b5(p26);
DigitalIn b6(p27);
DigitalIn spinn_pins[] = {b0, b1, b2, b3, b4, b5, b6};
DigitalOut ack(p28);

DigitalOut t0(p13);
DigitalOut t1(p14);
DigitalOut t2(p15);
DigitalOut t3(p16);
DigitalOut t4(p17);
DigitalOut t5(p18);
DigitalOut t6(p19);
DigitalIn tx_ack(p20);

/* Debug LEDs for checking transmission is occurring */
DigitalOut led0(LED1);
DigitalOut led1(LED2);
DigitalOut led2(LED3);
DigitalOut led3(LED4);

/* Buffer for transmitting SpiNNaker data */
uint8_t spin_tx_buf[SPINN_BUF_SIZE];
uint8_t *p_spin_tx = &spin_tx_buf[0];
bool ack_state = false;

uint8_t pkts_received = 0;
uint8_t flag_send_data = 0;

/* Template for UART callback */
void frame_rx();

int main() {
    bool failed = false;
    uint32_t duration = 0;
    uint8_t prev_val = 0;
    uint8_t port_val = 0;
    int sym_start_time = 0;
    // Set up peripherals interrupts
    uart.attach(&frame_rx, Serial::RxIrq);

    // Start timer
    timer.start();
    
    // Ensure output bits are the same
    t0.write(0);
    t1.write(0);
    t2.write(0);
    t3.write(0);
    t4.write(0);
    t5.write(0);
    t6.write(0);

    led0.write(0);
    led1.write(0);
    led2.write(0);
    led3.write(0);

    // Main program loop
    while(1) {

        /* Allowed to read port and acknowledge */
        if (read_port)
        {
            current_spinn =  b0.read() | (b1.read() << 1) | (b2.read() << 2) |
                    (b3.read() << 3) | (b4.read() << 4) | (b5.read() << 5) |
                    (b6.read() << 6);

            led0.write((current_spinn & 0x10) > 0 ? 1 : 0);
            led1.write((current_spinn & 0x20) > 0 ? 1 : 0);
            led2.write((current_spinn & 0x40) > 0 ? 1 : 0);
            led3.write((current_spinn & 0x80) > 0 ? 1 : 0);

            /* Read port and check against previous */
            if (current_spinn != previous_spinn)
            {
                /* We have a change, so store it, update previous, and ack */
                spinn_rx_buf[spinn_buf_idx++] = current_spinn ^ previous_spinn;
                previous_spinn = current_spinn;

                /* Acknowledge */
                ack.write(ack.read() ^ 1);

                /* Record timings */
                if (start_time == 0)
                {
                    start_time = timer.read_us();
                }
                latest_time = timer.read_us();

                if (spinn_buf_idx == SPINN_BUF_SIZE)
                {
                    spinn_buf_idx = 0;
                }
            }
        }
        else
        {
            /* Ignoring port until we get a trigger */
            wait(0.01);
        }
        
        if (flag_send_data)
        {
            /* PC has requested that we send the buffer to the STM */
            timer.reset();
            start_time = timer.read_us();
            
            prev_val =  t0.read() | (t1.read() << 1) | (t2.read() << 2) |
                (t3.read() << 3) | (t4.read() << 4) | (t5.read() << 5) |
                (t6.read() << 6);
            
            for (uint8_t *p_tx = spin_tx_buf; p_tx < p_spin_tx; p_tx++)
            {
                sym_start_time = timer.read_us();
                /* Store acknowledge, transmit symbol, wait for ack */
                ack_state = tx_ack.read();
                              
                port_val = (*p_tx) ^ prev_val;

                t0.write((port_val & 0x1)  >> 0);
                t1.write((port_val & 0x2)  >> 1);
                t2.write((port_val & 0x4)  >> 2);
                t3.write((port_val & 0x8)  >> 3);
                t4.write((port_val & 0x10) >> 4);
                t5.write((port_val & 0x20) >> 5);
                t6.write((port_val & 0x40) >> 6);
                
                prev_val = port_val;

                /* Wait for acknowledge or 100ms */
                while (ack_state == tx_ack.read() && 
                       ((timer.read_us() - sym_start_time) < 100000));
                if (ack_state == tx_ack.read())
                {
                    failed = true;
                    break;
                }
            }

            if (!failed)
            {
                latest_time = timer.read_us();

                /* Send duration */
                duration = ((uint32_t) latest_time - start_time);
                uart.putc((duration & 0xFF000000) >> 24);
                uart.putc((duration & 0x00FF0000) >> 16);
                uart.putc((duration & 0x0000FF00) >>  8);
                uart.putc((duration & 0x000000FF)      );
            }
            else
            {
                /* Failed, so put zeroes */
                uart.putc(0);
                uart.putc(0);
                uart.putc(0);
                uart.putc(0);
            }                

            p_spin_tx = &spin_tx_buf[0];
            failed = false;
            uart.putc('\r');
            flag_send_data = false;
        }
        
        wait(0.001);
    }
}

// Define function for a callback on serial interrupts
void frame_rx()
{
    uint32_t duration = 0;
    char cmd_buf[5];

    // Copy characters into buffer until carriage return is reached, signifying
    // a complete command
    while (uart.readable())
    {
        rx_buf[rx_buf_idx++] = uart.getc();

        /* Check if a command has been received */
        if (rx_buf[rx_buf_idx-1] == '\r')
        {
            memcpy(cmd_buf, rx_buf, 4);
            cmd_buf[4] = 0;

            /* Figure out what command it is and act upon it */
            if (strcmp(cmd_buf, PC_CMD_ID) == 0)
            {
                uart.printf(PC_RESP_ID);
            }
            else if (strcmp(cmd_buf, PC_CMD_GET_SPINN)== 0)
            {
                /* Send most recent duration */
                duration = ((uint32_t) latest_time - start_time);
                uart.putc((duration & 0xFF000000) >> 24);
                uart.putc((duration & 0x00FF0000) >> 16);
                uart.putc((duration & 0x0000FF00) >>  8);
                uart.putc((duration & 0x000000FF)      );

                /* Put number of SpiNNaker packets received */
                uart.putc(spinn_buf_idx);

                /* Send contents of SpiNN buffer */
                for (int i = 0; i < spinn_buf_idx; i++)
                {
                    uart.putc(spinn_rx_buf[i]);
                }

                /* Finish with carriage return */
                uart.putc('\r');

                /* Clear buffer and timings */
                start_time = 0;
                latest_time = 0;
                spinn_buf_idx = 0;
                timer.reset();
            }
            else if (strcmp(cmd_buf, PC_CMD_WAIT) == 0)
            {
                /* Clear flag allowing us to read port */
                read_port = false;

                /* Clear buffer and timings */
                start_time = 0;
                latest_time = 0;
                spinn_buf_idx = 0;
                timer.reset();
            }
            else if (strcmp(cmd_buf, PC_CMD_TRIGGER) == 0)
            {
                /* Set flag allowing us to read port */
                read_port = true;
            }
            else if (strcmp(cmd_buf, PC_CMD_SIM) == 0)
            {
                if (read_port)
                {
                    if (start_time == 0)
                    {
                        start_time = timer.read_us();
                    }

                    latest_time = timer.read_us();

                    for (int i = 4; i < rx_buf_idx-1; i++)
                    {
                        spinn_rx_buf[spinn_buf_idx++] = rx_buf[i];
                    }
                }
            }
            else if (strcmp(cmd_buf, PC_CMD_RESET) == 0)
            {
                NVIC_SystemReset();
            }
            else if (strcmp(cmd_buf, PC_CMD_USE_SPINN) == 0)
            {
                /* We have a new packet to place into the buffer */
                if ((&spin_tx_buf[SPINN_BUF_SIZE-1] - p_spin_tx) >= 11)
                {
                    memcpy(p_spin_tx, &rx_buf[4], 11);
                    p_spin_tx += 11;
                }
            }
            else if (strcmp(cmd_buf, PC_CMD_TRIG_SPINN) == 0)
            {
                flag_send_data = true;
            }

            /* As we have consumed the command, clear the buffer */
            rx_buf_idx = 0;
        }

        if (rx_buf_idx == RX_BUF_SIZE)
        {
            /* Some error has occurred; clear buffer and start over */
            rx_buf_idx = 0;
        }
    }
}
