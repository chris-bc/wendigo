#ifndef WENDIGO_STATUS_H
#define WENDIGO_STATUS_H

#include <stdbool.h>

void display_status_interactive(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported);
void display_status_uart(bool uuidDictionarySupported, bool btClassicSupported,
                                bool btBLESupported, bool wifiSupported);

#endif