#ifndef CREATEREQUEST_H
#define CREATEREQUEST_H

#include <algorithm>
#include <iostream>
#include <istream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include "request.h"
#include <tinyxml2.h>
#include <pqxx/pqxx>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "account.h"
#include "server.h"

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;


class CreateRequest : public Request {
public:
    CreateRequest(const XMLElement* root, connection* C, Server& server) : Request(root, C), server(server) {
        parseRequest(C);
    };
    XMLDocument responseDoc;
    void parseRequest(connection* C) override;
    void executeRequest(connection* C) override;

   // XMLDocument generateResponse() override;
    
private:
    vector<Account> accountsToCreate;
    // Symbol name, vector of (account ID, quantity)
    vector<pair<string, vector<pair<int, int>>>> symbolsToCreate;
    connection* conn;
    Server& server;
    
};

#endif