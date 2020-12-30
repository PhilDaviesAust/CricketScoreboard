// assign network name (ssid) and password
// (password must be min 8 characters)
const char* ssid     = "scoreboardnet";
const char* password = "scoreboard";

// assign IP address
IPAddress local_IP(10,0,0,200);
IPAddress gateway(10,0,0,1);
IPAddress subnet(255,255,255,0);
