#pragma once
#include <ncurses.h>
#undef timeout
#include "cmd.h"
#include "data.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
class PayloadRing {
public:
    PayloadRing(size_t cap = 0);
    void push(const std::string& value);
    const std::string& operator[](size_t idx) const;
    size_t size() const;
    size_t capacity() const;
    void resize(size_t newCap);

private:
    size_t m_capacity;
    std::vector<std::string> m_buffer;
    size_t m_head;  
    size_t m_size;  
};

class UI {
public:
    UI(cmd::CommandQueue& uiToMqttQueue, cmd::CommandQueue& mqttToUiQueue, SharedData& sharedData);
    ~UI();
    void run();  
    
private:
    enum SelectedWindow {TOP, LEFT, RIGHT };
    enum TopControlKeys {
        CONNECT,
        ADDRESS,
        ADDRESSSTR,
        PORT,
        PORTSTR,
        USER,
        USERSTR,
        PASS,
        PASSSTR,
        CLIENTID,
        CLIENTIDSTR,
        LOGLIMIT,
        LOGLIMITSTR,
        QUIT,
        HELP,
        N_CONTROLS // Always last, for counting
    };
    typedef struct {
        std::string text;
        bool selectable;
        int y, x;
        TopControlKeys key;
    } TopControl;
    TopControl m_topControls[15] = {
        {"Connect",  true, 0, 0, TopControlKeys::CONNECT},
        {"Address:",  true, 0, 0, TopControlKeys::ADDRESS},
        {"localhost",  false, 0, 0, TopControlKeys::ADDRESSSTR},
        {"Port:",  true, 0, 0, TopControlKeys::PORT},
        {"1883",  false, 0, 0, TopControlKeys::PORTSTR},
        {"User:",  true, 0, 0, TopControlKeys::USER},
        {"user",  false, 0, 0, TopControlKeys::USERSTR},
        {"Pass:",  true, 0, 0, TopControlKeys::PASS},
        {"pass",  false, 0, 0, TopControlKeys::PASSSTR},
        {"ClientID:",  true, 0, 0, TopControlKeys::CLIENTID},
        {"mqtterm",  false, 0, 0, TopControlKeys::CLIENTIDSTR},
        {"LogLimit", true, 0, 0, TopControlKeys::LOGLIMIT},
        {"10", false, 0, 0, TopControlKeys::LOGLIMITSTR},
        {"Quit",  true, 0, 0, TopControlKeys::QUIT},
        {"Help",  true, 0, 0, TopControlKeys::HELP}
    };
    int m_currentTopControl = TopControlKeys::CONNECT; // Index of the currently selected control
    bool m_connected{false};
    bool m_pause{false};
    int m_selectedRow{0};
    int m_numberOfVisibleRows{0};
    int m_scrollOffset{0}; 
    int m_screenHeight{0};
    int m_screenWidth{0};
    int m_topWindowHeight{0};
    int m_topWindowWidth{0};
    int m_leftWindowHeight{0};
    int m_leftWindowWidth{0};
    int m_rightWindowHeight{0};
    int m_rightWindowWidth{0};
    int m_rightWindowParentHeight{0};
    int m_rightWindowParentWidth{0};
    int m_leftWindowParentHeight{0};
    int m_leftWindowParentWidth{0};
    int m_childScreenOffset{1};
    cmd::CommandQueue& m_uiToMqttQueue;
    cmd::CommandQueue& m_mqttToUiQueue;
    SharedData& m_sharedData;
    struct TopicNode {
        std::string name;
        std::string fullPath;
        std::string payload;
        bool expanded = false;
        bool selected = false; 
        int rowNr = 0;
        std::map<std::string, std::unique_ptr<TopicNode>> children;
        TopicNode* parent = nullptr;
        std::unique_ptr<PayloadRing> log;
    };
    TopicNode* m_selectedNode = nullptr; 
    bool m_running{true};
    void drawUI();
    void drawTitles();
    void drawTopics();
    void drawPayload();
    void drawTopControls();
    void topControlKeyHandler(int ch);
    void leftControlKeyHandler(int ch);
    void rightControlKeyHandler(int ch);
    void displayError(const std::string& msg);
    bool editSettings(std::string& title, std::string& editStr, bool numeric);
    void handleCmd(const cmd::Command& cmd);
    //* createNewWindow(int height, int width, int starty, int startx);
    // windows for top/left/right panes
    WINDOW* m_leftWinParent{nullptr};
    WINDOW* m_leftWin{nullptr};
    WINDOW* m_rightWinParent{nullptr};
    WINDOW* m_rightWin{nullptr};
    WINDOW* m_topWin{nullptr};
    SelectedWindow m_selectedWindow{TOP};
    void collapseAllChildren(TopicNode* node);
    void drawTopicNode(TopicNode* node, int& row, int depth);
    std::vector<std::string> splitTopic(const std::string& topic);
    void searchTree(const TopicNode* node, const std::string& term, std::vector<const TopicNode*>& results);
    void search();
    void addPayload(TopicNode* node, const std::string& value);
    void mergeTreeFromData(const std::unordered_map<std::string, std::string>& data);
    std::unique_ptr<TopicNode> m_topicTree;
};