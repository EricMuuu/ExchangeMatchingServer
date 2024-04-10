#ifndef SERVER_H
#define SERVER_H

#include <tinyxml2.h>
#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>

#define MAX_THREADS 4
using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

class Server {
public:
     Server(asio::io_context& io_context, short port);
    ~Server();
    
    void start();
    void handle_accept(const boost::system::error_code& error, const std::shared_ptr<tcp::socket>& socket);
    void ThreadControl();
    mutex dbMutex;
    

private:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    //std::shared_ptr<tcp::socket> socket_;
    pqxx::connection *dbConnection;

    
    void initialize_db();
    void handleClientConnection(std::shared_ptr<tcp::socket> socket);
    void start_accept();
    void database_statement(connection *C);
    void readMessageLength(const std::shared_ptr<tcp::socket>& socket);
    void readXMLMessage(const std::shared_ptr<tcp::socket>& socket, size_t length);
    void processXML(const std::shared_ptr<tcp::socket>& socket, const std::string& xmlData);
    void handle_response(const std::shared_ptr<tcp::socket>& socket, const tinyxml2::XMLDocument& responseDoc);


    void handle_request(const XMLDocument& xmlDoc,const shared_ptr<tcp::socket>& socket);
    void handle_create(const XMLDocument& xmlDoc,const shared_ptr<tcp::socket>& socket);
    void handle_transaction(const XMLDocument& xmlDoc,const shared_ptr<tcp::socket>& socket);

    connection* connect_to_db(const string& dbName, const string& userName, const string& password);
    void initialize_db(connection* C);
    // void matchOrders();
    // void start_order_matching();

    void readAndPrintXML(const shared_ptr<boost::asio::ip::tcp::socket>& socket);

    int currentThreads = 0;
    std::queue<std::function<void()>> requestQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    shared_ptr<XMLDocument> xmlDoc;
  
    void processRequestQueue();
    void enqueueRequest(std::function<void()> requestHandler);
};

#endif
