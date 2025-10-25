#pragma once

struct WiFiCred {
  const char* ssid;
  const char* password;
};

// Change this to your mDNS name
#define DEVICE_MDNS_NAME "akonomy"

// List of WiFi networks to try
const WiFiCred knownNetworks[] = {
  {"Minerii din bomboclat", "castravete"},
  {"C004", "53638335"},
  {"VIP", "castravete"}
};

const int NUM_NETWORKS = sizeof(knownNetworks) / sizeof(knownNetworks[0]);
