#ifndef TRANSACTIONREQUEST_H
#define TRANSACTIONREQUEST_H

#include <algorithm>
#include <iostream>
#include <istream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <tinyxml2.h>
#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "request.h"
#include "transaction.h"
#include "server.h"


using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;


class TransactionRequest : public Request {
public:
    TransactionRequest(const XMLElement* root, connection* C, Server& server) : Request(root, C), server(server) {
        parseRequest(C);
    };
    // ~TransactionRequest();

    void parseRequest(connection* C) override;
    void executeRequest(connection* C);
    bool checkAccountExistence(connection* C);
    XMLDocument responseDoc;
    // if 0, valid and call executeRequest to get XML, if 1, get XML after parse
    int is_valid = 0;
    int accountId;
    connection* conn;
    Server& server;


};

// handle query subnode
class Query {
public:
    Query(int TransId, Server& server, int accountId);
    void execute(work& W, XMLElement* resultsElement);
private:
    int transId;
    Server& server;
    int accountId;
};

// // handle cancel subnode
class Cancel {
public:
    Cancel(int TransId, Server& server, int accountId);
    void execute(work& W, XMLElement* resultsElement);
private:
    int transId;
    Server& server;
    int accountId;
};

#endif
