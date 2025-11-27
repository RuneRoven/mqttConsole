#include "ui.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <cmd.h>
#include <cstdlib> // for system()
inline size_t g_maxLogSize = 10;
PayloadRing::PayloadRing(size_t cap)
    : m_capacity(cap), m_buffer(cap), m_head(0), m_size(0)
{
}
void PayloadRing::push(const std::string& value)
{
    if (m_capacity == 0) return;

    // compute index of previous stored item
    size_t prev = (m_head + m_capacity - 1) % m_capacity;

    // suppress duplicates
    if (m_size > 0 && m_buffer[prev] == value)
        return;

    // write new value
    m_buffer[m_head] = value;

    // advance head
    m_head = (m_head + 1) % m_capacity;

    // increase size up to capacity
    if (m_size < m_capacity)
        m_size++;
}

const std::string& PayloadRing::operator[](size_t index) const {
    size_t physicalIndex = (m_head + m_capacity - 1 - index) % m_capacity;
    return m_buffer[physicalIndex];
}

size_t PayloadRing::size() const
{
    return m_size;
}
size_t PayloadRing::capacity() const
{
    return m_capacity;
}
void PayloadRing::resize(size_t newCap)
{
    if (newCap == m_capacity) return;

    std::vector<std::string> newBuf(newCap);

    size_t copyCount = std::min(m_size, newCap);
    for (size_t i = 0; i < copyCount; i++)
        newBuf[i] = (*this)[m_size - copyCount + i];

    m_buffer = std::move(newBuf);
    m_capacity = newCap;
    m_size     = copyCount;
    m_head     = copyCount % newCap;
}

UI::UI(cmd::CommandQueue& uiToMqttQueue, cmd::CommandQueue& mqttToUiQueue, SharedData& sharedData) 
    : m_uiToMqttQueue(uiToMqttQueue), 
    m_mqttToUiQueue(mqttToUiQueue),
    m_sharedData(sharedData)
{
    // initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    // create windows

    //int maxy = m_screenWidth, maxx = m_screenHeight;
    getmaxyx(stdscr, m_screenHeight, m_screenWidth);
    int topHeight = std::max(3, m_screenWidth / 12);
    int leftw = std::max(20, m_screenHeight / 4);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(4, COLOR_WHITE, COLOR_MAGENTA); // selected left row
        init_pair(5, COLOR_BLACK, COLOR_GREEN);   // connected
        init_pair(6, COLOR_WHITE, COLOR_RED);     // disconnected/error
        init_pair(7, COLOR_BLACK, COLOR_YELLOW);  // selected top control
    }
    m_topWin = newwin(topHeight, m_screenHeight, 0, 0);
    m_leftWinParent = newwin(m_screenWidth - topHeight, leftw, topHeight, 0);
    m_rightWinParent = newwin(m_screenWidth - topHeight, m_screenHeight - leftw, topHeight, leftw);
    getmaxyx(m_rightWinParent, m_rightWindowParentHeight, m_rightWindowParentWidth);
    m_rightWin = newwin(m_rightWindowParentHeight - m_childScreenOffset*2, m_rightWindowParentWidth - m_childScreenOffset*2, topHeight+m_childScreenOffset, leftw+m_childScreenOffset);
    getmaxyx(m_rightWin, m_rightWindowHeight, m_rightWindowWidth);
    getmaxyx(m_leftWinParent, m_leftWindowParentHeight, m_leftWindowParentWidth);
    m_leftWin = newwin(m_leftWindowParentHeight - m_childScreenOffset*2, m_leftWindowParentWidth - m_childScreenOffset*2, topHeight+m_childScreenOffset, m_childScreenOffset);
    getmaxyx(m_topWin, m_topWindowHeight, m_topWindowWidth);
    getmaxyx(m_leftWin, m_leftWindowHeight, m_leftWindowWidth);
}
UI::~UI() {
    if (m_topWin) { delwin(m_topWin); m_topWin = nullptr; }
    if (m_leftWin) { delwin(m_leftWin); m_leftWin = nullptr; }
    if (m_rightWin) { delwin(m_rightWin); m_rightWin = nullptr; }
    endwin();
}
void UI::drawTitles(){
    if (m_connected)
        mvwprintw(m_topWin, 0, 2, " Connected");
    else {
        mvwprintw(m_topWin, 0, 2, " Disconnected");
    }
    mvwprintw(m_leftWinParent, 0, 2, " Topics ");
    mvwprintw(m_rightWinParent, 0, 2, " Messages ");  
    if (m_pause){
        wattron(m_rightWinParent, A_REVERSE);
        wattron(m_rightWinParent, A_BLINK);
        mvwprintw(m_rightWinParent, 0, 18, "PAUSED");  
        wattroff(m_rightWinParent, A_BLINK);
        wattroff(m_rightWinParent, A_REVERSE);
    }
    switch (m_selectedWindow)
    {
    case SelectedWindow::TOP:
        wattron(m_topWin, A_REVERSE);
        if (m_connected)
            mvwprintw(m_topWin, 0, 2, " Connected");
        else {
            mvwprintw(m_topWin, 0, 2, " Disconnected");
        }
        wattroff(m_topWin, A_REVERSE);
        break;
    case SelectedWindow::LEFT:
        wattron(m_leftWinParent, A_REVERSE);
        mvwprintw(m_leftWinParent, 0, 2, " Topics ");
        wattroff(m_leftWinParent, A_REVERSE);
        break;
    case SelectedWindow::RIGHT:
        wattron(m_rightWinParent, A_REVERSE);
        mvwprintw(m_rightWinParent, 0, 2, " Messages ");
        wattroff(m_rightWinParent, A_REVERSE);
        break;
    default:
        break;
    }
}
void UI::drawTopControls(){
    int usedWidth = 2;
    int y = 1, x = 2; // y is row, x is column
    if (m_connected){
        m_topControls[TopControlKeys::CONNECT].text = "Disconnect";
    } else {
        m_topControls[TopControlKeys::CONNECT].text = "Connect";
    }
    for (int i = 0; i < N_CONTROLS; i++) {
        if (i == 0) { // if first item, start at beginning.
            y = 1;
            x = 2;
        } else {
            if (usedWidth + m_topControls[i].text.length() + 1 >= m_screenWidth) { //does not fit on current line, start new line
                y++;
                x = 2;
                usedWidth = 2;
            } else {
                x = usedWidth + 1;
            }
        }
        m_topControls[i].y = y;
        m_topControls[i].x = x;
        usedWidth = m_topControls[i].text.length() + x;
        if (i == m_currentTopControl && m_selectedWindow == SelectedWindow::TOP) {
            wattron(m_topWin, A_REVERSE);
            mvwprintw(m_topWin, y, x, "%s", m_topControls[i].text.c_str());
            wattroff(m_topWin, A_REVERSE);
        } else {
            mvwprintw(m_topWin, y, x, "%s", m_topControls[i].text.c_str());
        }
    }

}
void UI::drawTopicNode(TopicNode* node, int& row, int depth = 0) {
    if (!node) return;
    //if (row >= maxRows) return; // avoid drawing off-screen
    if (row >= m_scrollOffset && row < m_scrollOffset + m_leftWindowHeight-1) {
        // Calculate where to actually draw it on screen
        int displayRow = row - m_scrollOffset;

        std::string indent(depth * 2, ' '); // 2 spaces per depth
        std::string symbol = node->children.empty() ? " " : (node->expanded ? "-" : "+");
        node->rowNr = row;
        if (row == m_selectedRow){
            node->selected = true;
            m_selectedNode = node;
        } else {
            node->selected = false;
        }
        if (node->selected)
            wattron(m_leftWin, A_REVERSE);
        mvwprintw(m_leftWin, displayRow, 2, "%s%s %s", indent.c_str(), symbol.c_str(), node->name.c_str());
        if (node->selected)
            wattroff(m_leftWin, A_REVERSE);
    }
    row++;
    m_numberOfVisibleRows = row;
    if (node->expanded) {
        for (const auto& [_, child] : node->children) {
            drawTopicNode(child.get(), row, depth + 1);
        }
    }
}
void UI::collapseAllChildren(TopicNode* node) {
    if (!node) return;
    for (auto& [_, child] : node->children) {
        child->expanded = false;
        collapseAllChildren(child.get());  // recursively collapse deeper levels
    }
}
void UI::drawPayload() {
    if (!m_selectedNode) return;

    int row = 0; 
    int col = 1;

    if (m_selectedNode->log) {
        for (size_t i = 0; i < m_selectedNode->log->size(); i++) {
            if (row >= m_rightWindowHeight - 1) break; // don’t overflow
            mvwprintw(m_rightWin, row++, col, "%s", (*m_selectedNode->log)[i].c_str());
        }
    } else if (!m_selectedNode->payload.empty()) {
        mvwprintw(m_rightWin, row++, col, "%s", m_selectedNode->payload.c_str());
    }
    
}

void UI::drawTopics(){
    int row = 0;
    drawTopicNode(m_topicTree.get(), row, 0);
}
void UI::drawUI() {
    getmaxyx(stdscr, m_screenHeight, m_screenWidth);
    int leftw = std::max(20, m_screenWidth / 4);
    int desiredTopHeight = 5;
    
    // resize top/left/right windows to match current terminal size
    if (m_topWin) {
        wresize(m_topWin, desiredTopHeight, m_screenWidth);
        mvwin(m_topWin, 0, 0);
    }
    int leftHeight = std::max(3, m_screenHeight - desiredTopHeight);
    if (m_leftWinParent) {
        wresize(m_leftWinParent, leftHeight, leftw);
        mvwin(m_leftWinParent, desiredTopHeight, 0);
    }
    if (m_leftWin) {
        wresize(m_leftWin, leftHeight-m_childScreenOffset*2, leftw-m_childScreenOffset*2);
        mvwin(m_leftWin, desiredTopHeight+m_childScreenOffset, m_childScreenOffset);
    }
    if (m_rightWinParent) {
        wresize(m_rightWinParent, leftHeight, m_screenWidth - leftw);
        mvwin(m_rightWinParent, desiredTopHeight, leftw);
    }
    if (m_rightWin) {
        wresize(m_rightWin, m_rightWindowParentHeight - m_childScreenOffset*2, m_rightWindowParentWidth - m_childScreenOffset*2);
        mvwin(m_rightWin, desiredTopHeight+m_childScreenOffset, leftw+m_childScreenOffset);
    }
   
    // windows
    werase(m_topWin);
    werase(m_leftWinParent);
    werase(m_leftWin);
    werase(m_rightWinParent);
    werase(m_rightWin);
    box(m_topWin, 0, 0);
    box(m_leftWinParent, 0, 0);
    box(m_rightWinParent, 0, 0);
    
    // title
    drawTitles();
    drawTopControls();
    drawTopics();
    if (!m_pause){
        drawPayload();
    } 
    wrefresh(m_topWin);
    wrefresh(m_leftWinParent);
    wrefresh(m_leftWin);
    wrefresh(m_rightWinParent);
    wrefresh(m_rightWin);

    getmaxyx(m_leftWinParent, m_leftWindowParentHeight, m_leftWindowParentWidth);
    getmaxyx(m_leftWin, m_leftWindowHeight, m_leftWindowWidth);
    getmaxyx(m_topWin, m_topWindowHeight, m_topWindowWidth);
    getmaxyx(m_rightWinParent, m_rightWindowParentHeight, m_rightWindowParentWidth);
    getmaxyx(m_rightWin, m_rightWindowHeight, m_rightWindowWidth); 
}
void UI::displayError(const std::string& msg) {
    // Create a 2-row popup window
    int pH = 3; // Fixed height
    int pW = std::min(static_cast<int>(m_screenWidth - 4), static_cast<int>(msg.length() + 4));
    int pX = std::max(1, (m_screenWidth - pW) / 2); // Center horizontally
    int pY = std::max(1, (m_screenHeight - pH) / 2); // Center vertically

    WINDOW* popup = newwin(pH, pW, pY, pX);
    box(popup, 0, 0);
    wattron(popup, A_REVERSE);
    wattron(popup, A_BLINK);
    mvwprintw(popup, 0, 2, " %s ", "ERROR");
    wattroff(popup, A_REVERSE);
    wattroff(popup, A_BLINK);
    wmove(popup, 1, 0);
    mvwprintw(popup, 1, 2, "%s", msg.c_str());
    wrefresh(popup);
    int ch;
    while ((ch = wgetch(popup)) != 10 && ch != 27) { // Use wgetch(popup) to capture input in the popup
        // wait
    }
}
bool UI::editSettings(std::string& title, std::string& editStr, bool numeric) {
    // Create a 2-row popup window
    int pH = 3; // Fixed height
    int pW = std::min(m_screenWidth - 4, 40); // Adjust width as needed
    int pX = std::max(1, (m_screenWidth - pW) / 2); // Center horizontally
    int pY = std::max(1, (m_screenHeight - pH) / 2); // Center vertically

    WINDOW* popup = newwin(pH, pW, pY, pX);
    box(popup, 0, 0);
    //wattron(popup, A_REVERSE);
    mvwprintw(popup, 0, 2, " %s ", title.c_str());
    //wattroff(popup, A_REVERSE);
    echo(); // Enable echoing of typed characters
    curs_set(1); // Show the cursor
    // Clear the line inside the popup and reprint the text
    wmove(popup, 1, 2);
    wclrtoeol(popup); // Clear to end of line
    mvwprintw(popup, 1, 2, "%s", editStr.c_str());
    wrefresh(popup);
    int ch;
    std::string newString = editStr;
    while ((ch = wgetch(popup)) != 10 && ch != 27) { // Use wgetch(popup) to capture input in the popup
        if (ch == KEY_BACKSPACE || ch == 127) { // Handle backspace
            if (!newString.empty()) {
                newString.pop_back();
            }
        } else if (isprint(ch)) { // Only accept printable characters
            if (newString.length() < pW - 4) { // Limit input length to fit inside the box
                if  (ch >= '0' && ch <= '9' && numeric){
                    newString += static_cast<char>(ch);
                } else if (!numeric){
                    newString += static_cast<char>(ch);
                }
            }
        }

        // Clear the line inside the popup and reprint the text
        wmove(popup, 1, 2);
        wclrtoeol(popup); // Clear to end of line
        mvwprintw(popup, 1, 2, "%s", newString.c_str());
        wrefresh(popup);
    }
    noecho(); // Disable echoing
    curs_set(0); // Hide the cursor
    delwin(popup);
    if (ch == 10){
        if (editStr != newString){
            editStr = newString;
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}
void UI::topControlKeyHandler(int ch){
    cmd::Command cmd;
    switch (ch)
    {
    case KEY_LEFT: case KEY_UP:
        if (m_currentTopControl > 0 && m_topControls[m_currentTopControl - 1].selectable) {
            m_currentTopControl--;
        } else if (m_currentTopControl > 0 && m_topControls[m_currentTopControl - 2].selectable) {
            m_currentTopControl -= 2;
        }
        break;
    case KEY_RIGHT: case KEY_DOWN:
        if (m_currentTopControl + 1 < N_CONTROLS && m_topControls[m_currentTopControl + 1].selectable) {
            m_currentTopControl++;
        } else if (m_currentTopControl + 2 < N_CONTROLS && m_topControls[m_currentTopControl + 2].selectable){
                m_currentTopControl += 2;
        }
        break;
    case 10: // Enter key
        switch (m_currentTopControl)
        {
        case TopControlKeys::CONNECT:
            if (m_connected){
                cmd.topic = "__CMD";
                cmd.message = "DISCONNECT";
                m_uiToMqttQueue.push(cmd);  
            }
            else
            { 
                cmd.topic = "__CMD";
                cmd.message = "CONNECT";
                m_uiToMqttQueue.push(cmd);
            }  
            break;
        case TopControlKeys::ADDRESS:
            if (!m_connected){
                if (editSettings(m_topControls[TopControlKeys::ADDRESS].text, m_topControls[TopControlKeys::ADDRESSSTR].text, false)){
                    cmd.topic = "__CFG";
                    cmd.subtopic = "ADDRESS";
                    cmd.message = m_topControls[TopControlKeys::ADDRESSSTR].text;
                    m_uiToMqttQueue.push(cmd);  
                }
            }
            break;
        case TopControlKeys::PORT:
            if (!m_connected){
                if (editSettings(m_topControls[TopControlKeys::PORT].text, m_topControls[TopControlKeys::PORTSTR].text, true)){
                    cmd.topic = "__CFG";
                    cmd.subtopic = "PORT";
                    cmd.message = m_topControls[TopControlKeys::PORTSTR].text;
                    m_uiToMqttQueue.push(cmd);  
                }
            }
            break;
        case TopControlKeys::USER:
            if (!m_connected){
                if (editSettings(m_topControls[TopControlKeys::USER].text, m_topControls[TopControlKeys::USERSTR].text, false)){
                    cmd.topic = "__CFG";
                    cmd.subtopic = "USER";
                    cmd.message = m_topControls[TopControlKeys::USERSTR].text;
                    m_uiToMqttQueue.push(cmd);  
                }
            }
            break;
        case TopControlKeys::PASS:
            if (!m_connected){
                if (editSettings(m_topControls[TopControlKeys::PASS].text, m_topControls[TopControlKeys::PASSSTR].text, false)){
                    cmd.topic = "__CFG";
                    cmd.subtopic = "PASSWORD";
                    cmd.message = m_topControls[TopControlKeys::PASSSTR].text;
                    m_uiToMqttQueue.push(cmd);  
                }
            }
            break;
        case TopControlKeys::CLIENTID:
            if (!m_connected){
                if (editSettings(m_topControls[TopControlKeys::CLIENTID].text, m_topControls[TopControlKeys::CLIENTIDSTR].text, false)){
                    cmd.topic = "__CFG";
                    cmd.subtopic = "CLIENTID";
                    cmd.message = m_topControls[TopControlKeys::CLIENTIDSTR].text;
                    m_uiToMqttQueue.push(cmd);  
                }
            }
            break;
        case TopControlKeys::LOGLIMIT:
            editSettings(m_topControls[TopControlKeys::LOGLIMIT].text, m_topControls[TopControlKeys::LOGLIMITSTR].text, true);
            g_maxLogSize = std::stoi( m_topControls[TopControlKeys::LOGLIMITSTR].text );
            break;
        case TopControlKeys::QUIT:
            m_running = false;
            cmd.topic = "__CMD";
            cmd.message = "QUIT";
            m_uiToMqttQueue.push(cmd);  
            break;
        case TopControlKeys::HELP:
            
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
   
}
void UI::leftControlKeyHandler(int ch){
    if (!m_topicTree) return;
    switch(ch){
        case KEY_DOWN:
            if (m_selectedRow+1 >= m_numberOfVisibleRows)
                break;
            m_selectedRow++;
            if (m_selectedRow >= m_scrollOffset + m_leftWindowHeight)
                m_scrollOffset = m_selectedRow - m_leftWindowHeight + 1;
            break;

        case KEY_UP:
            if (m_selectedRow > 0) m_selectedRow--;
            if (m_selectedRow < m_scrollOffset)
                m_scrollOffset = m_selectedRow;
            break;

        case KEY_LEFT:
            if (m_selectedNode->name == "#")
                break;
            if (m_selectedNode->expanded){
                m_selectedNode->expanded = false;
            } else {
                m_selectedNode->parent->expanded = false;
                m_selectedRow = m_selectedNode->parent->rowNr;
            }
            collapseAllChildren(m_selectedNode);
            break;

        case KEY_RIGHT:
        case 10: // Enter
            if (!m_selectedNode->children.empty())
                m_selectedNode->expanded = true;
            break;
    }

    
}
void UI::rightControlKeyHandler(int ch){
    if (ch == 32)
        m_pause = !m_pause;
}
void UI::handleCmd(const cmd::Command& cmd){
    if (cmd.topic == "__STATUS"){
        if (cmd.subtopic == "CONNECTED" && cmd.message == "TRUE"){
            m_connected = true;
        } else if (cmd.subtopic == "DISCONNECTED" && cmd.message == "TRUE"){
            m_connected = false;
        } else if (cmd.subtopic == "FAILED"){
            displayError(cmd.message);
        }

    } else if (cmd.topic == "__CFG"){
        if (cmd.subtopic == "SERVER"){
            m_topControls[TopControlKeys::ADDRESSSTR].text = cmd.message;
        } else if (cmd.subtopic == "PORT"){
            m_topControls[TopControlKeys::PORTSTR].text = cmd.message;
        } else if (cmd.subtopic == "CLIENTID"){
            m_topControls[TopControlKeys::CLIENTIDSTR].text = cmd.message;
        } else if (cmd.subtopic == "USERNAME"){
            m_topControls[TopControlKeys::USERSTR].text = cmd.message;
        } else if (cmd.subtopic == "PASSWORD"){
            m_topControls[TopControlKeys::PASSSTR].text = cmd.message;
        }
    }
}
std::vector<std::string> UI::splitTopic(const std::string& topic) {
    std::vector<std::string> parts;
    std::string part;
    for (char c : topic) {
        if (c == '/') {
            if (!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
        } else {
            part += c;
        }
    }
    if (!part.empty()) parts.push_back(part);
    return parts;
}
void UI::search(){
    std::vector<const TopicNode*> results;
    searchTree(m_topicTree.get(), "pc1", results);
}
void UI::searchTree(const TopicNode* node, const std::string& term, std::vector<const TopicNode*>& results) {
    if (!node) return;
    if (node->fullPath.find(term) != std::string::npos)
        results.push_back(node);
    for (const auto& [_, child] : node->children)
        searchTree(child.get(), term, results);
}
void UI::addPayload(TopicNode* node, const std::string& value)
{
    node->payload = value;

    if (!node->log)
        node->log = std::make_unique<PayloadRing>(g_maxLogSize);
    if (node->log->capacity() != g_maxLogSize)
        node->log->resize(g_maxLogSize);
    node->log->push(value);
}

void UI::mergeTreeFromData(const std::unordered_map<std::string, std::string>& data) {
    if (!m_topicTree) {
        m_topicTree = std::make_unique<TopicNode>();
        m_topicTree->name = "#";
        m_topicTree->fullPath = "/";
        m_topicTree->expanded = true;
    }

    for (const auto& [topic, payload] : data) {
        auto parts = splitTopic(topic);
        TopicNode* current = m_topicTree.get();
        std::string path;

        for (const auto& part : parts) {
            path += "/" + part;

            auto it = current->children.find(part);
            if (it == current->children.end()) {
                // Create new node if missing
                auto node = std::make_unique<TopicNode>();
                node->name = part;
                node->fullPath = path;
                node->expanded = false;
                node->parent = current;
                current->children.emplace(part, std::move(node));
            }
            current = current->children[part].get();
        }
        UI::addPayload(current, payload);

    }
}

void UI::run() {
    system("stty -ixon"); // Disable software flow control to capture Ctrl-S and Ctrl-Q
    while (m_running) {
        m_sharedData.swap_buffers();
        mergeTreeFromData(m_sharedData.read());
        int ch = getch();
        if (ch == 17 /*17 Ctrl-Q*/) { 
            cmd::Command cmd;
            m_running = false;
            cmd.topic = "__CMD";
            cmd.message = "QUIT";
            m_uiToMqttQueue.push(cmd);  
        } else if (ch == 9 /*TAB*/) { // cycle focus
            if (m_selectedWindow == SelectedWindow::LEFT) m_selectedWindow = SelectedWindow::RIGHT;
            else if (m_selectedWindow == SelectedWindow::RIGHT) m_selectedWindow = SelectedWindow::TOP;
            else m_selectedWindow = SelectedWindow::LEFT;
        }
        if (m_selectedWindow == SelectedWindow::TOP) {
            topControlKeyHandler(ch);
        } else if (m_selectedWindow == SelectedWindow::LEFT) {
            leftControlKeyHandler(ch);
        } else if (m_selectedWindow == SelectedWindow::RIGHT) {
            rightControlKeyHandler(ch);
        }
        drawUI();   
        cmd::Command cmd;
        if (m_mqttToUiQueue.try_pop(cmd)) {
            UI::handleCmd(cmd);
        }
       std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
    }
    
}
