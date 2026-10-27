#pragma once

// Boot the paired Helios image from the Artemis flash through the ROM UART
// loader and the temporary SPI image receiver. Leaves UART0 at the runtime baud.
bool UniBootK210();
