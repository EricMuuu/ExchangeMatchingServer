#ifndef CLIENT_H
#define CLIENT_H

#include <boost/asio.hpp>
#include <string>
#include "tinyxml2.h"
#include <boost/thread/thread.hpp>
#include <atomic>


#define INIT_SYM 50 // Initial amount for each symbol
#define INIT_BAL 1000.0 // Initial balance for accounts
#define NUM_ORDER 2 // Number of buy and sell orders sent by each account (a total of NUM_ORDER*2 orders)
#define LLIMIT_PRICE 5 // Minimum price value for trading
#define HLIMIT_PRICE 30 // Maximum price value for trading
#define MAX_LENGTH 65536
#define SYM_NUM 2 // Number of symbols added in each Create Request


using namespace tinyxml2;

class Client {
public:
    Client(boost::asio::io_context& io_context, const std::string& server, const std::string& port, int account_id);

    void start();
    void close();

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::string server_;
    std::string port_;
    int account_id_;

    std::atomic<bool> createRequestCompleted{false};
    std::atomic<int> pendingCreateRequests{0};
    boost::mutex mutex_;

    // Asynchronous handlers
    void handleConnect(const boost::system::error_code& ec);
    void handleWrite(const boost::system::error_code& ec, std::string xml);
    void handleRead(const boost::system::error_code& ec, std::shared_ptr<std::vector<char>> buffer);



    void sendCreateRequestTask();
    void sendTransactionRequestTask();

    std::string generateCreateXML();
    std::string generateTransXML();
    void sendRequest(const std::string& xml, std::function<void()> onCompletion); 
    void createAccount(int account_id, float balance, tinyxml2::XMLDocument& request);
    void createSymbol(const std::string& sym, int account_id, int amount, tinyxml2::XMLDocument& request);
    void addOrder(const std::string& sym, int amount, float limit_price, tinyxml2::XMLDocument& request);
    void addQuery(XMLDocument& request, int transid);
    void addCancel(XMLDocument& request, int transid);
    int getRandomINT(int min, int max); 
    void readResponse(std::function<void()> onCompletion);

};

#endif // CLIENT_H
