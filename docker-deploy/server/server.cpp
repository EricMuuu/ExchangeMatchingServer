#include "server.h"
#include "account.h"
#include <tinyxml2.h>
#include <boost/bind.hpp>
#include "createrequest.h" 
#include "transactionrequest.h" 
#include <exception>

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;


Server::Server(asio::io_context& io_context, short port)
    : io_context_(io_context), 
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    initialize_db();
    start_accept();
    ThreadControl();
}

void Server::ThreadControl() {
    boost::thread_group threadPool;
    for (int i = 0; i <  MAX_THREADS; ++i) {
        threadPool.create_thread(boost::bind(&Server::processRequestQueue, this));
    }
}

void Server::processRequestQueue() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this]{ return !requestQueue.empty(); });
            task = requestQueue.front();
            requestQueue.pop();
        }
        task(); 
    }
}

void Server::enqueueRequest(std::function<void()> requestHandler) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        requestQueue.push(requestHandler);
    }
    queueCondition.notify_one();
}

void Server::initialize_db() {
    try {
        dbConnection = new connection("dbname=exchange_server user=postgres password=passw0rd host=db");
        if (dbConnection->is_open()) {
            initialize_db(dbConnection);
        } else {
            cerr << "Failed to open database." << endl;
        }
    } catch (const exception& e) {
        cerr << "Database connection error: " << e.what() << endl;
    }
}

void Server::start_accept() {
    auto new_socket = make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*new_socket, [this, new_socket](const boost::system::error_code& error) {
        handle_accept(error, new_socket);
    });
    // start_order_matching();
}

void Server::handle_accept(const boost::system::error_code& error, const shared_ptr<tcp::socket>& socket) {
    if (!error) {
        handleClientConnection(socket);
    } else {
        cerr << "Accept error: " << error.message() << endl;
    }
    start_accept(); // Listen for new connections
}

void Server::handleClientConnection(shared_ptr<tcp::socket> socket) {
    //this->socket_ = socket; 
    //readMessageLength(socket);
    enqueueRequest([this, socket](){
        readMessageLength(socket);
    });
}


void Server::readMessageLength(const shared_ptr<boost::asio::ip::tcp::socket>& socket) {
    auto buf = make_shared<vector<char>>(4);
    boost::asio::async_read(*socket, boost::asio::buffer(*buf), boost::asio::transfer_exactly(4),
        [this, socket, buf](const boost::system::error_code& ec, size_t /*length*/) {
            if (!ec) {
                size_t xmlLength = ntohl(*reinterpret_cast<uint32_t*>(buf->data()));
                readXMLMessage(socket, xmlLength);
            } else {
                cerr << "Error reading message length: " << ec.message() << endl;
            }
        });
}

void Server::readXMLMessage(const shared_ptr<boost::asio::ip::tcp::socket>& socket, size_t length) {
    auto buf = make_shared<vector<char>>(length);
    boost::asio::async_read(*socket, boost::asio::buffer(*buf), boost::asio::transfer_exactly(length),
        [this, socket, buf](const boost::system::error_code& ec, size_t /*length*/) {
            if (!ec) {
                string xmlData(buf->begin(), buf->end());
                this->xmlDoc = make_shared<XMLDocument>();
                this->xmlDoc->Parse(xmlData.c_str());
                this->handle_request(*this->xmlDoc, socket);
                readMessageLength(socket);
            } else {
                cerr << "Error reading XML message: " << ec.message() << endl;
            }
        });
}


// connection* Server::connect_to_db(const string& dbName, const string& userName, const string& password) {
//     try {
//         auto* connection = new connection("dbname=" + dbName + " user=" + userName + " password=" + password);
//         if (connection->is_open()) {
//             initialize_db(dbConn);
//         } else {
//             throw runtime_error("Can't open database");
//         }
//         return connection;
//     } catch (const exception& e) {
//         cerr << "Exception: " << e.what() << endl;
//         return nullptr;
//     }
// }

connection* Server::connect_to_db(const string& dbName, const string& userName, const string& password) {
    try {
        dbConnection = new connection("dbname=" + dbName + " user=" + userName + " password=" + password);
        if (dbConnection->is_open()) {
            initialize_db(dbConnection);  // Now that we are connected, initialize the DB
        } else {
            throw runtime_error("Can't open database");
        }
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        if (dbConnection) {
            delete dbConnection;
            dbConnection = nullptr;
        }
    }
    return dbConnection;
}


void Server::initialize_db(connection* C) {
    work W(*C);

    W.exec("DROP TYPE IF EXISTS states CASCADE;");
    W.exec("DROP TABLE IF EXISTS orders CASCADE;");
    W.exec("DROP TABLE IF EXISTS symbols CASCADE;");
    W.exec("DROP TABLE IF EXISTS accounts CASCADE;");
    // Define the ENUM type for 'states'
    W.exec("CREATE TYPE states AS ENUM ('OPEN', 'EXECUTED', 'CANCELLED')");

    // Create the ACCOUNT table
    W.exec("CREATE TABLE accounts ("
           "account_id INT PRIMARY KEY, "
           "balance DECIMAL(20,2)"
           ")");

    // Create the SYMBOL table
    W.exec("CREATE TABLE symbols ("
           "account_id INT, "
           "amount INT, "
           "symbol VARCHAR(20), "
           "CONSTRAINT symbol_pk PRIMARY KEY (account_id, symbol), "
           "FOREIGN KEY (account_id) REFERENCES accounts(account_id) ON DELETE CASCADE ON UPDATE CASCADE"
           ")");

    // Create the ORDERS table, now referencing the 'states' ENUM type
    W.exec("CREATE TABLE orders ("
           "account_id INT, "
           "amount INT, "
           "sym VARCHAR(20), "
           "trans_id SERIAL, "
           "limit_price DECIMAL(20,2), "
           "state states, "
           "time TIMESTAMP, "
           "CONSTRAINT order_pk PRIMARY KEY (trans_id, time, state)"
           ")");

    W.commit();
}



void Server::handle_request(const XMLDocument& xmlDoc, const shared_ptr<tcp::socket>& socket){
    // Check the type of the request and call corresponding method to parse and execute it
    const XMLElement* root = xmlDoc.RootElement();
    if (string(root->Name()) == "create") {
        handle_create(xmlDoc, socket);
    } else if (string(root->Name()) == "transactions") {
        handle_transaction(xmlDoc, socket);
    } else {
        cerr << "Unsupported type of request" << endl;
    }
}

void Server::handle_create(const XMLDocument& xmlDoc, const shared_ptr<tcp::socket>& socket) {
    CreateRequest createRequest(xmlDoc.RootElement(), dbConnection, *this);
    createRequest.executeRequest(dbConnection);
    handle_response(socket, createRequest.responseDoc);
}

void Server::handle_transaction(const XMLDocument& xmlDoc, const shared_ptr<tcp::socket>& socket) {
    const XMLElement* root = xmlDoc.RootElement();
    TransactionRequest transactionRequest(xmlDoc.RootElement(), dbConnection, *this);

    // if account is valid, call executeRequest to get XML response, otherwise ready from parse
    // if (transactionRequest.is_valid == 0) {
    //     transactionRequest.executeRequest(dbConnection);
    //     cout << "executed" << endl;
    // } 

     if (!socket->is_open()){
        cout <<"socket is not open now" << endl;
    }

    handle_response(socket, transactionRequest.responseDoc);
    // handle_response(socket_, "ERROR", "Unsupported request type");
}

// void Server::handle_response(const shared_ptr<boost::asio::ip::tcp::socket>& socket, const XMLDocument& responseDoc) {
//     XMLPrinter printer;
//     responseDoc.Print(&printer);
//     string responseStr = printer.CStr();
//     cout << responseStr << endl;
//     // boost::asio::async_write(*socket, boost::asio::buffer(responseStr), [socket](const boost::system::error_code& ec, size_t) {
//     //     if (ec) {
//     //         cerr << "Error sending response: " << ec.message() << endl;
//     //     }
//     // });
// }


void Server::handle_response(const shared_ptr<boost::asio::ip::tcp::socket>& socket, const XMLDocument& responseDoc) {
    XMLPrinter printer;
    responseDoc.Print(&printer);
    string responseStr = printer.CStr();

    cout << "Sending response:\n" << responseStr.length() << endl;
    cout << responseStr << endl;
    unsigned int responseLength = htonl(responseStr.length());

    boost::asio::async_write(*socket, boost::asio::buffer(&responseLength, sizeof(responseLength)),
        [this, socket, responseStr](const boost::system::error_code& ec, size_t) {
            if (!ec) {
                
                boost::asio::async_write(*socket, boost::asio::buffer(responseStr),
                    [socket](const boost::system::error_code& ec, size_t) {
                        if (ec) {
                            cerr << "Error sending XML response: " << ec.message() << endl;
                        }
                    });
            } else {
                cerr << "Error sending response length: " << ec.message() << endl;
            }
        });
}




Server::~Server() {
    connection* connection = connect_to_db("exchange_server", "postgres", "passw0rd");
    if (connection) {
        try {
            work W(*connection);
            // Drop all tables when the server is terminated
            W.exec("DROP TABLE IF EXISTS orders CASCADE;");
            W.exec("DROP TABLE IF EXISTS symbols CASCADE;");
            W.exec("DROP TABLE IF EXISTS accounts CASCADE;");
            W.exec("DROP TYPE IF EXISTS states CASCADE;");
            W.commit();
            // cout << "All tables dropped successfully" << endl;
        } catch (const exception& e) {
            cerr << "Exception in dropping tables: " << e.what() << endl;
        }
        connection->disconnect();
        delete connection;
    }
}

// SQL statement to check conditions
void Server::database_statement(connection* C) {
    // If an account exists
    C->prepare("check_account_exists", "SELECT 1 FROM accounts WHERE ACCOUNT_ID = $1");
    // Insert a new account
    C->prepare("insert_account", "INSERT INTO accounts (ACCOUNT_ID, BALANCE) VALUES ($1, $2)");
    // Insert a new symbol for an account
    C->prepare("insert_symbol", "INSERT INTO symbols (ACCOUNT_ID, SYM, AMOUNT) VALUES ($1, $2, $3)");
}

int main() {
    try {
        asio::io_context io_context;
        Server server(io_context, 12345);
        io_context.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}