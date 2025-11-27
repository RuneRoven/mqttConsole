# MQTT Console Client

This is a simple console-based MQTT client written in C++ using the Eclipse Paho library and ncurses for the UI.

# Usage
change the the file to executable and run using:
```bash
chmod +x mqtt_console
./mqtt_console
```

use the client_config.json to add default settings if you like. Else change the settings in the program.
Settings can oly be changed when disconnected, except log limit.
Use TAB to switch between the windows.
If the right window with values is selected you can press SPACE to pause the UI if you want the values to be still.
Ctrl+Q exits the program.

## TODO
* Search function
* publish function
* help section

## Dependencies
- Eclipse Paho C++ MQTT Library
- ncurses

## build with dynamic linking
cmake -S ./ -B /buildDir/mqttConsole_build -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j $(nproc)

## Build with static linking
```bash
rm -rf /buildDir/mqttConsole_build_static
cmake -S /path/to/your/project -B /buildDir/mqttConsole_build_static -DCMAKE_BUILD_TYPE=Release -DSTATIC_LINKING=ON
cmake --build /buildDir/mqttConsole_build_static -j $(nproc)
ldd /buildDir/mqttConsole_build_static/mqtt_console
```
#### On Debian/Ubuntu:
```bash
sudo apt update
sudo apt install build-essential cmake git libncurses5-dev libncursesw5-dev
```

### Run the Client
```bash
./mqtt_console
```
