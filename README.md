# si443x_drv
Silicon Labs Si443x(RFM22b) user space Linux driver

This code is in early development, and not usable yet.

# ToDo

   * Test/Fix over-/underflow handling
   * Different header length support
   * Fixed packet len support
   * Interrupt support
   * Transmit support
   * Man page/Documentation
   * Systemd config


# Configuration
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

# Usage
