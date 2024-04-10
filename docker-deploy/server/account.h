#ifndef ACCOUNT_H
#define ACCOUNT_H

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

class Account {
public:
    int id;
    //TODO: check if int is large enough
    int balance;
    // Store the symbol and amount this account has
    // unordered_map<string, int> position;

    Account(int accountId, double accountBalance) : id(accountId), balance(accountBalance) {}
    int getId() const;
    double getBalance() const;
    ~Account(){};

    // void set_id(string request);
    // void set_balance(string request);
    // void set_position(string request);
};

#endif