#include "account.h"
#include <tinyxml2.h>

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

int Account::getId() const {
    return id;
}

double Account::getBalance() const {
    return balance;
}