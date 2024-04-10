
#include <iostream>
#include <fstream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include "tinyxml2.h"
#include <sstream>
#include <functional>

using namespace tinyxml2;
using std::string;
using std::to_string;

class Client {
public:
    Client(boost::asio::io_context& io_context, const string& server, const string& port, int account_id);

    void start();
    void handleConnect(const boost::system::error_code& ec);
    void sendRequest(const std::string& xml, std::function<void()> onCompletion);
    void readResponse(std::function<void()> onCompletion);
    std::string readXmlFromFile(const std::string& fileName);

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::string server_;
    std::string port_;
    int account_id_;
};
