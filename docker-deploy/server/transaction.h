#ifndef TRANSACTION_H
#define TRANSACTION_H

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
#include "server.h"

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

// Handle Order subnode
class Transaction {
public:
    Transaction(const string& symbol, int amount, float limit, const string& type, Server& server);
    void executeOrder(work& W, int accountId, XMLElement* resultsElement);
    void matchOrder(work& W, int accountId, const string& symbol, int& amount, float limit, const string& orderTime);

private:
    string symbol;
    int amount;
    float limit;
    string type;
    int transId;
    Server& server;
};

#endif