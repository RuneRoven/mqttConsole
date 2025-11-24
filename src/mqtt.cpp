#include "mqtt.h"
#include <mqtt/client.h>
#include <fstream>
#include <iostream>
#include <cmd.h>
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <ctime>


MqttClient::MqttClient(cmd::CommandQueue& uiToMqttQueue, cmd::CommandQueue& mqttToUiQueue, SharedData& sharedData) 
    : m_uiToMqttQueue(uiToMqttQueue), 
      m_mqttToUiQueue(mqttToUiQueue),
      m_sharedData(sharedData),
      m_callback(m_sharedData)
{ 
    m_cfg = ClientConfig::loadFromFile("client_config.json");
    createClient();
}
MqttClient::~MqttClient() {
    if (m_connected) {
        MqttClient::disconnect();
    }
}
void MqttClient::createClient(){
    cmd::Command cmdSend;
    m_client = std::make_unique<mqtt::client>(m_cfg.server+":"+m_cfg.port, m_cfg.clientId);
    m_client->set_callback(m_callback);
    m_connOpts = mqtt::connect_options_builder::v3()
                        .keep_alive_interval(std::chrono::seconds(30))
                        .automatic_reconnect(std::chrono::seconds(2), std::chrono::seconds(30))
                        .clean_session(false)
                        .finalize();
    if (!m_cfg.username.empty()) m_connOpts.set_user_name(m_cfg.username);
    if (!m_cfg.password.empty()) m_connOpts.set_password(m_cfg.password);
    cmdSend.topic = "__CFG";
    cmdSend.subtopic = "SERVER";
    cmdSend.message = m_cfg.server;
    m_mqttToUiQueue.push(cmdSend);
    cmdSend.subtopic = "PORT";
    cmdSend.message = m_cfg.port;
    m_mqttToUiQueue.push(cmdSend);
    cmdSend.subtopic = "CLIENTID";
    cmdSend.message = m_cfg.clientId;
    m_mqttToUiQueue.push(cmdSend);
    cmdSend.subtopic = "USERNAME";
    cmdSend.message = !m_cfg.username.empty() ? m_cfg.username : "none";
    m_mqttToUiQueue.push(cmdSend);
    cmdSend.subtopic = "PASSWORD";
    cmdSend.message = !m_cfg.password.empty() ? m_cfg.password : "none";
    m_mqttToUiQueue.push(cmdSend);
}
std::string MqttClient::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&in_time_t);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() % std::chrono::seconds(1)
    ).count();

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);

    char result[40]; 
    std::snprintf(result, sizeof(result), "%s.%03dZ", buffer, static_cast<int>(ms));
    return std::string(result);
}

void MqttClient::run() {
    m_running = true;
    while (m_running) { // Or until a "quit" command
        cmd::Command cmd;
        if (m_uiToMqttQueue.try_pop(cmd)) {
            // Publish the command via MQTT
            if (cmd.topic == "__CMD"){
                if (cmd.message == "CONNECT")
                {
                    if (m_cfgChanged){
                        m_client.reset();
                        m_cfgChanged = false;
                        createClient();
                    }
                    std::string errorMessage;
                    if (MqttClient::connect(errorMessage)) { 
                        cmd::Command cmdSend;
                        cmdSend.topic = "__STATUS";
                        cmdSend.subtopic = "CONNECTED";
                        cmdSend.message = "TRUE";
                        m_mqttToUiQueue.push(cmdSend);
                    } else {
                        cmd::Command cmdSend;
                        cmdSend.topic = "__STATUS";
                        cmdSend.subtopic = "FAILED"; 
                        cmdSend.message = errorMessage; 
                        m_mqttToUiQueue.push(cmdSend);
                    }

                }
                else if (cmd.message == "DISCONNECT")
                {
                    MqttClient::disconnect();
                }
                else if (cmd.message == "QUIT"){
                    MqttClient::disconnect();
                    m_running = false;
                }
            } else if (cmd.topic == "__CFG"){
                m_cfgChanged = true;
                if (cmd.subtopic == "ADDRESS"){
                    m_cfg.server = cmd.message;
                } else if (cmd.subtopic == "PORT"){
                    m_cfg.port = cmd.message;
                } else if (cmd.subtopic == "CLIENTID"){
                    m_cfg.clientId = cmd.message;
                } else if (cmd.subtopic == "USERNAME"){
                    m_cfg.username = cmd.message;
                } else if (cmd.subtopic == "PASSWORD"){
                    m_cfg.password = cmd.message;
                }
            } else {
            //m_client.publish(cmd.topic, cmd.message); 
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

}

bool MqttClient::connect(std::string& errorMessage) { 
    try {
        mqtt::connect_response rsp = m_client->connect(m_connOpts);
        if (m_client->is_connected()) {
            m_connected = true;
            try {
                m_client->subscribe("#", 1);
            } catch (const mqtt::exception& exc) {
                errorMessage = exc.what();
                m_connected = false;
                return false;
            }
            return true;
        }
    } catch (const mqtt::exception& exc) {
        errorMessage = exc.what(); 
        m_connected = false;
        return false;
    }
    return false;
}

void MqttClient::disconnect(){
    if (m_client->is_connected()){
        m_client->disconnect();
    }
    m_connected = false;
    cmd::Command cmdSend;
    cmdSend.topic = "__STATUS";
    cmdSend.subtopic = "DISCONNECTED";
    cmdSend.message = "TRUE";
    m_mqttToUiQueue.push(cmdSend);
}
