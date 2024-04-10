#ifndef REQUEST_H
#define REQUEST_H

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

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;


class Request {
public:
    Request(const XMLElement* root, connection* C) : rootElement(root), conn(C) {}
    virtual void parseRequest(connection* C) = 0;
    //virtual void executeRequest() = 0;
    virtual void executeRequest(connection* C) = 0; 
    //virtual XMLDocument generateResponse() = 0;
//     virtual XMLDocument generateResponse() {
//     return 0; // this is a just basic return, need modify
// }

protected:
    const XMLElement* rootElement;
    string requestType;
    connection* conn;
};

#endif
