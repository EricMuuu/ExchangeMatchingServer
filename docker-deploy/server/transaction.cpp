#include "transaction.h"
#include <tinyxml2.h>

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

Transaction::Transaction(const string& symbol, int amount, float limit, const string& type, Server& server)
    : symbol(symbol), amount(amount), limit(limit), type(type), transId(0), server(server) {}


void Transaction::executeOrder(work& W, int accountId, XMLElement* resultsElement) {
    lock_guard<mutex> guard(server.dbMutex);
    bool success = true;
    // If it is buy, check balance
    if (type == "BUY") {
        result balanceCheck = W.exec("SELECT balance FROM accounts WHERE account_id = " + to_string(accountId));
        if (balanceCheck.empty() || stod(balanceCheck[0][0].c_str()) < amount * limit) {
            XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
            errorElement->SetAttribute("sym", symbol.c_str());
            errorElement->SetAttribute("amount", to_string(amount).c_str());
            errorElement->SetAttribute("limit", to_string(limit).c_str());
            errorElement->SetText("Insufficient balance");
            resultsElement->InsertEndChild(errorElement);
            success = false;
            return;
        } else {
            W.exec("UPDATE accounts SET balance = balance - " + to_string(amount * limit) + " WHERE account_id = " + to_string(accountId));
        }
        // If has sufficient balance, update balance
        // W.exec("UPDATE accounts SET balance = balance - " + to_string(amount * limit) + " WHERE account_id = " + to_string(accountId));
    // If it is sell, check num shares holding
    } else {
        result sharesCheck = W.exec("SELECT amount FROM symbols WHERE account_id = " + to_string(accountId) + " AND symbol = '" + symbol + "'");
        if (sharesCheck.empty() || stoi(sharesCheck[0][0].c_str()) < abs(amount)) {
            XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
            errorElement->SetAttribute("sym", symbol.c_str());
            errorElement->SetAttribute("amount", to_string(amount).c_str());
            errorElement->SetAttribute("limit", to_string(limit).c_str());
            errorElement->SetText("Insufficient shares");
            resultsElement->InsertEndChild(errorElement);
            success = false;
            return;
        } else {
            W.exec("UPDATE symbols SET amount = amount - " + to_string(amount) + " WHERE account_id = " + to_string(accountId) + " AND symbol = '" + symbol + "'");
        }
    }   
    // Execute
    if (success) {
        // lock_guard<mutex> guard(server.dbMutex);
        result order_result = W.exec("INSERT INTO orders (account_id, sym, amount, limit_price, state, time) VALUES (" +
            to_string(accountId) + ", '" + symbol + "', " + to_string(amount) + ", " + to_string(limit) +
            ", 'OPEN', NOW()) RETURNING trans_id, time");
        
        
        // Set and get transId 
        this->transId = order_result[0]["trans_id"].as<int>();
        string orderTime = order_result[0]["time"].as<string>();
        matchOrder(W, accountId, symbol, amount, limit, orderTime);
        W.commit();

        XMLElement* createdElement = resultsElement->GetDocument()->NewElement("opened");
        createdElement->SetAttribute("sym", symbol.c_str());
        createdElement->SetAttribute("amount", to_string(amount).c_str());
        createdElement->SetAttribute("limit", to_string(limit).c_str());
        createdElement->SetAttribute("id", to_string(transId).c_str());
        resultsElement->InsertEndChild(createdElement);
    } else {
        W.abort();
    }
}

void Transaction::matchOrder(work& W, int accountId, const string& symbol, int& amount, float limit, const string& orderTime) {
    // lock_guard<mutex> guard(server.dbMutex);
    const int originalAmount = abs(amount);
    string orderType = (amount >= 0) ? "BUY" : "SELL";
    string oppositeOrderType = (amount >= 0) ? "SELL" : "BUY";
    // Find the possible matches from best to worst
    // string query = "SELECT trans_id, account_id, amount, limit_price, time FROM orders "
    //            "WHERE sym = '" + symbol + "' AND state = 'OPEN' AND "
    //            "(('" + orderType + "' = 'BUY' AND limit_price <= " + to_string(limit) + ") OR "
    //            "('" + orderType + "' = 'SELL' AND limit_price >= " + to_string(limit) + ")) AND "
    //            "amount * " + to_string(amount) + " < 0 "
    //            "ORDER BY time ASC, limit_price " + (orderType == "BUY" ? "ASC" : "DESC");
    string query = "SELECT trans_id, account_id, amount, limit_price, time FROM orders "
               "WHERE sym = '" + symbol + "' AND state = 'OPEN' AND account_id != " + to_string(accountId) + " AND "
               "(('" + orderType + "' = 'BUY' AND limit_price <= " + to_string(limit) + ") OR "
               "('" + orderType + "' = 'SELL' AND limit_price >= " + to_string(limit) + ")) AND "
               "amount * " + to_string(amount) + " < 0 "
               "ORDER BY time ASC, limit_price " + (orderType == "BUY" ? "ASC" : "DESC");


    result matches = W.exec(query);
    // For all possible matches
    for (auto match : matches) {
        int matchTransId = match["trans_id"].as<int>();
        int matchAccountId = match["account_id"].as<int>();
        int matchAmount = match["amount"].as<int>();
        float matchLimit = match["limit_price"].as<float>();
        string matchTime = match["time"].as<string>();

        // Determine price based on order time
        float executionPrice = (orderTime <= matchTime) ? limit : matchLimit;

        // Determine amount of execution
        int executedAmount = min(abs(amount), abs(matchAmount));

        // If we are the buyer
        if (orderType == "BUY") {
            // Refund the difference
            if (executionPrice < limit) {
                W.exec("UPDATE accounts SET balance = balance + " + to_string((limit - executionPrice) * executedAmount) +
                    " WHERE account_id = " + to_string(accountId));
            }
            // Add the executed amount to the seller's balance
            W.exec("UPDATE accounts SET balance = balance + " + to_string(executedAmount * executionPrice) +
                " WHERE account_id = " + to_string(matchAccountId));

            // If this order is fully executed
            if (executedAmount == amount) {
                // If matched order is also fully executed
                if (executedAmount == matchAmount) {
                    W.exec("UPDATE orders SET state = 'EXECUTED', time = NOW(), limit_price = " + to_string(executionPrice) + 
                        " WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                } else {
                    // Update the matched order's amount
                    W.exec("UPDATE orders SET amount = amount + " + to_string(executedAmount) + " WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                    // Insert a new order for this executed part
                    W.exec("INSERT INTO orders (account_id, sym, amount, limit_price, state, time, trans_id) "
                        "VALUES (" + to_string(matchAccountId) + ", '" + symbol + "', -" + to_string(executedAmount) +
                        ", " + to_string(executionPrice) + ", 'EXECUTED', NOW(), " + to_string(matchTransId) + ")");
                }
                // Update this order to EXECUTED
                W.exec("UPDATE orders SET state = 'EXECUTED', time = NOW(), limit_price = " + to_string(executionPrice) + " WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
                W.commit();
                return;
            // This order is not fully executed but the matched order is 
            } else {
                // Update the matched order to be fully executed
                W.exec("UPDATE orders SET state = 'EXECUTED', WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                // Insert a new order for this executed part
                W.exec("INSERT INTO orders (account_id, sym, amount, limit_price, state, time, trans_id) "
                    "VALUES (" + to_string(accountId) + ", '" + symbol + "', " + to_string(executedAmount) +
                    ", " + to_string(executionPrice) + ", 'EXECUTED', NOW(), " + to_string(transId) + ")");
                // Update the amount of this order
                W.exec("UPDATE orders SET amount = amount - " + to_string(executedAmount) + " WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
                amount -= executedAmount;
            }
        // If we are the seller
        } else {
            // We add the executed amount to our balance
            W.exec("UPDATE accounts SET balance = balance + " + to_string(executedAmount * executionPrice) +
                " WHERE account_id = " + to_string(accountId));
            // If the execution price is lower than the buyer's limit, we refund the buyer
            if (executionPrice < matchLimit) {
                W.exec("UPDATE accounts SET balance = balance + " + to_string((matchLimit - executionPrice) * executedAmount) +
                    " WHERE account_id = " + to_string(matchAccountId));
            }
            // If this order is fully executed
            if (executedAmount == abs(amount)) {
                // If the matched order is also fully executed
                if (executedAmount == abs(matchAmount)) {
                    W.exec("UPDATE orders SET state = 'EXECUTED', time = NOW(), limit_price = " + to_string(executionPrice) + 
                           " WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                } else {
                    // Update the matched order to open and executed
                    W.exec("UPDATE orders SET amount = amount - " + to_string(executedAmount) + " WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                    // Insert a new order for this executed part
                    W.exec("INSERT INTO orders (account_id, sym, amount, limit_price, state, time, trans_id) "
                            "VALUES (" + to_string(matchAccountId) + ", '" + symbol + "', " + to_string(executedAmount) +
                           ", " + to_string(executionPrice) + ", 'EXECUTED', NOW(), " + to_string(matchTransId) + ")");
                }
                // Update this order to EXECUTED, commit and return
                W.exec("UPDATE orders SET state = 'EXECUTED', time = NOW(), limit_price = " + to_string(executionPrice) + 
                       " WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
                W.commit();
                return;
            // If the matched order is fully executed
            } else {
                // Update the matched order to EXECUTED
                W.exec("UPDATE orders SET state = 'EXECUTED', time = NOW(), limit_price = " + to_string(executionPrice) + 
                       " WHERE trans_id = " + to_string(matchTransId) + " AND state = 'OPEN'");
                // Insert executed part for this order
                W.exec("INSERT INTO orders (account_id, sym, amount, limit_price, state, time, trans_id) "
                    "VALUES (" + to_string(accountId) + ", '" + symbol + "', -" + to_string(executedAmount) +
                    ", " + to_string(executionPrice) + ", 'EXECUTED', NOW(), " + to_string(transId) + ")");
                // Update the amount of this order
                W.exec("UPDATE orders SET amount = amount + " + to_string(executedAmount) + " WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
                amount += executedAmount;
            }
        }

    }
    W.commit();
}

