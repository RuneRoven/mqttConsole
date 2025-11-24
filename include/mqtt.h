#pragma once
#include "cmd.h"
#include <string>
#include <nlohmann/json.hpp>
#include <mqtt/client.h>
#include <fstream>
#include <vector>
#include "data.h"
class MqttClient {
public:
    MqttClient(cmd::CommandQueue& uiToMqttQueue, cmd::CommandQueue& mqttToUiQueue, SharedData& sharedData);
    ~MqttClient();
    void run();
    
private:
    bool connect(std::string& error_message);
    void disconnect();
    static std::string timestamp();
    //void messageArrived(const std::string& topic, const std::string& payload); 
    //void publish(const std::string& topic, const std::string& payload);
    //void subscribe(const std::string& topic);
    //void set_callback(mqtt::callback& cb);
    void createClient();
    struct Callback : public virtual mqtt::callback {
        SharedData& shared;

        Callback(SharedData& sharedData) : shared(sharedData) {}

        void message_arrived(mqtt::const_message_ptr msg) override {
            shared.update(msg->get_topic(), MqttClient::timestamp() + " :  " + msg->to_string());
        }
    };

    struct ClientConfig {
        std::string server{"tcp://localhost"};
        std::string port{"1883"};
        std::string clientId{"mqtterm"};
        std::string username{""};
        std::string password{""};
        std::vector<std::string> topics{std::string("#")};
        static ClientConfig loadFromFile(const std::string& path) {
            ClientConfig cfg;
            std::ifstream ifs(path);
            if (!ifs.is_open()) return cfg;
            try {
                nlohmann::json j;
                ifs >> j;
                if (j.contains("server")) cfg.server = j.at("server").get<std::string>();
                if (j.contains("clientId")) cfg.clientId = j.at("clientId").get<std::string>();
                if (j.contains("port")) cfg.port = j.at("port").get<std::string>();
                if (j.contains("username")) cfg.username = j.at("username").get<std::string>();
                if (j.contains("password")) cfg.password = j.at("password").get<std::string>();
                if (j.contains("topics") && j.at("topics").is_array()) {
                    cfg.topics = j.at("topics").get<std::vector<std::string>>();
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse config '" << path << "': " << e.what() << "\nUsing defaults.\n";
            }
            return cfg;
        }
    };
    ClientConfig m_cfg;
    mqtt::connect_options m_connOpts;
    bool m_running{false};
    bool m_connected{false};
    bool m_cfgChanged{false};
    cmd::CommandQueue& m_uiToMqttQueue;
    cmd::CommandQueue& m_mqttToUiQueue;
    std::unique_ptr<mqtt::client> m_client; 
    SharedData& m_sharedData;
    Callback m_callback;
};
