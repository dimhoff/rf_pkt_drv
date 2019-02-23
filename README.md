# rf_pkt_drv
User space Linux driver for radio transceivers with packet engine connected
using SPI to the host.

The idea is that you supply a fixed configuration for the transceiver. The
configuration defines the RF parameters, like modulation and encoding. The
rf_pkt_drv daemon uses this to configure the transceiver and will open a FIFO
socket. Other application can send and receive packet by simply writting and
reading to/from the FIFO.

Currently supported transceivers:

  * Silicon Labs Si443x (/ RFM22b)
  * Semtech SX1231 (/ RFM69)

**WARNING: Code Maturity**
This code is in early development, and only partially usable. For me this is
just a side project. I currently only use the receive functionality for my
Sensof-HT sensors.

# ToDo
Appart from everything else, these point are still on the todo list:

   * Make transceiver selection a runtime option instead of compile time
   * [Si443x] Test/Fix over-/underflow handling
   * [Si443x] Different header length support
   * [Si443x] Fixed packet len support
   * Interrupt support
   * [Si443x] Transmit support
   * [Sx1231] Test Transmitting and the rest...
   * Man page/Documentation
   * Systemd config

And ofcourse test everything...

# Compile
To compile:

   * mkdir build
   * cd build
   * Depending on the transceiver:
     * cmake -DRF_BACKEND=si443x ../
     * cmake -DRF_BACKEND=sx1231 ../
   * make

# Configuration
## Si443x
Configuring the Si443x you **should** use the
*Si443x-Register-Settings_RevB1.xls* spreadsheet provided by Silicon Labs.
This spreadsheet calculates the correct values for a lot of magic configuration
registers, which the data sheet isn't clear about how to calculate them.

Once you filled in the spreadsheet, the register configuration is available on
the *REGISTERS Settings SUMMARY* tab. To create a si443x_drv register
configuration file from this you have two options:

1. Copy range B18:C67 to a text file. The file will consists of lines in the
   format: 2 characters hexadecimal address, single tab or space, 2 characters
   hexadecimal value.
   Example:

        36	2D
        37	D4

2. Copy range D18:D67 to a text file.
   Example:

        S2 B62D
        S2 B7D4

## Sx1231
Use the [[sx1231_calculator][https://github.com/dimhoff/sx1231_calculator]].
Export the register values and store them in the configuration file(default:
/etc/rf_pkt_regs.cfg).

Configuration Requirements:

  * Data Operation Mode = Packet Mode (RegDataModul.DataMode = 00b)
  * Auto Restart RX after InterPacketRxDelay = True (RegPacketConfig2.AutoRxRestartOn = 1)
  * DIO0 = 2nd Option(-;-;-;PayloadReady;TxReady) (RegDioMapping1.Dio0Mapping = 01b)
  * RegOpMode.SequencerOff = 0 (= default)

Limitations:

  * message length > 66 bytes is not supported
  * Listen mode is not supported
  * Low battery monitoring is not supported
  * Switching configuration on the fly is not supported

# Usage
TODO:...
See Sensof repository for an example:
https://github.com/dimhoff/sensof/tree/master/software/si443x_sensof
