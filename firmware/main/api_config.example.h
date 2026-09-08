#pragma once

// Copy to api_config.local.h. Do not commit the token or share the binary.
namespace api_config {
inline constexpr char kEndpoint[] = ""; // https://YOUR_DOMAIN/api/v1/measurements
inline constexpr char kDeviceId[] = "home-air-01";
inline constexpr char kDeviceToken[] = ""; // Same as backend DEVICE_TOKEN.
inline constexpr unsigned kIntervalSeconds = 30;
// Only for a deliberate LAN experiment against http://MAC_LAN_IP:3000/...
inline constexpr bool kAllowInsecureHttp = false;
}
