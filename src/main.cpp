#include <thread>
#include "ui.hpp"
#include "cmd.h"
#include <memory>
#include <mqtt/client.h>
#include "mqtt.h"
#include "data.h"

int main() {
    cmd::CommandQueue uiToMqttQueue;
    cmd::CommandQueue mqttToUiQueue;
    SharedData shared;
    MqttClient mqttClient(uiToMqttQueue, mqttToUiQueue, shared);
    UI ui(uiToMqttQueue, mqttToUiQueue, shared);
    std::thread mqttThread([&mqttClient] { mqttClient.run(); });
    std::thread uiThread([&ui] { ui.run(); });
    uiThread.join();
    mqttThread.join();
    return 0;
}

