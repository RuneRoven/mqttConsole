# MQTT Console Client

This is a simple console-based MQTT client written in C++ using the Eclipse Paho library and ncurses for the UI.

## Dependencies
- Eclipse Paho C++ MQTT Library
- ncurses

## Installation

### 1. Install Dependencies

#### On Debian/Ubuntu:
```bash
sudo apt update
sudo apt install build-essential cmake git libncurses5-dev libncursesw5-dev
```
### Install Eclipse Paho C++ Library
```bash
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
cmake -Bbuild -H. -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_BUILD_SAMPLES=TRUE
cmake --build build/
sudo cmake --install build/
```
### Build the Project
```bash
mkdir build
cd build
cmake ..
make
```
### Run the Client
```bash
./mqtt_console
```
### Test the Client 
Publish a message to the test/topic topic using mosquitto_pub:
mosquitto_pub -t "test/topic" -m "Hello from MQTT Console!"
You should see the message appear in your console client.


### How to Use
