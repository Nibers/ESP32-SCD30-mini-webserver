#ifndef CONFIG_H
#define CONFIG_H

// WiFi networks (add as many as you want)
const char* WIFI_SSIDS[] = {
    "Wifi1",
    "Wifi2",
    "Wifi3"
};

const char* WIFI_PASSWORDS[] = {
    "PW1",
    "PW2",
    "PW3"
};

// Number of WiFi networks
const int WIFI_NETWORK_COUNT = sizeof(WIFI_SSIDS) / sizeof(WIFI_SSIDS[0]);

#endif
