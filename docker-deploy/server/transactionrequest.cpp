#include "transactionrequest.h"
#include <tinyxml2.h>


using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

bool TransactionRequest::checkAccountExistence(connection* C) {
    lock_guard<mutex> guard(server.dbMutex);
    work W(*C);
    result check = W.exec("SELECT EXISTS(SELECT 1 FROM accounts WHERE account_id = " + to_string(accountId) + ")");
    bool isValid = check[0][0].as<bool>();
    W.commit();
    return isValid;
}

void TransactionRequest::parseRequest(connection* C) {
    // if no id included, we see it as account not found, dont terminate
    try {
        accountId = stoi(rootElement->Attribute("id"));
    } catch (const exception& e) {
        cerr << "Invalid request format: " << e.what() << endl;
    }
    bool isValid = checkAccountExistence(C);
    // If account does not exist, print error for each subnode
    if (!isValid) {
        XMLElement* resultsElement = responseDoc.NewElement("results");
        responseDoc.InsertEndChild(resultsElement);
        for (const XMLElement* child = rootElement->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
            string tagName = child->Name();
            if (tagName == "order") {
                string symbol = child->Attribute("sym");
                int amount = stoi(child->Attribute("amount"));
                float limit = stof(child->Attribute("limit"));
                XMLElement* errorElement = responseDoc.NewElement("error");
                errorElement->SetAttribute("sym", symbol.c_str());
                errorElement->SetAttribute("amount", to_string(amount).c_str());
                errorElement->SetAttribute("limit", to_string(limit).c_str());
                errorElement->SetText("Invalid account ID");
                resultsElement->InsertEndChild(errorElement); 
            } else if (tagName == "query" || tagName == "cancel") {
                XMLElement* errorElement = responseDoc.NewElement("error");
                errorElement->SetAttribute("id", to_string(accountId).c_str());
                errorElement->SetText("Invalid account ID");
                resultsElement->InsertEndChild(errorElement); 
            } else {
                cerr << "Invalid transaction request type";
            }
        }
        this->is_valid = 1;
        return;
    }
    // If exist
    else {
        XMLElement* resultsElement = responseDoc.NewElement("results");
        responseDoc.InsertEndChild(resultsElement);
        for (const XMLElement* child = rootElement->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
            string tagName = child->Name();
            // Order
            if (tagName == "order") {
                string order_symbol = child->Attribute("sym");
                int order_amount = stoi(child->Attribute("amount"));
                float order_limit = stof(child->Attribute("limit"));
                // Should not have 0 amount order?
                string order_type = (order_amount >= 0) ? "BUY" : "SELL";
                work W(*C);
                Transaction* transaction = new Transaction(order_symbol, order_amount, order_limit, order_type, server);
                transaction->executeOrder(W, accountId, resultsElement);
                W.commit();
                delete transaction;
            // Query
            } else if (tagName == "query") {
                int transId = stoi(child->Attribute("id"));
                work W(*C);
                Query* query = new Query(transId, server, accountId);
                query->execute(W, resultsElement);
                W.commit();
                delete query;
            // Cancel
            } else if (tagName == "cancel") {
                int transId = stoi(child->Attribute("id"));
                work W(*C);
                Cancel* cancel = new Cancel(transId, server, accountId);
                cancel->execute(W, resultsElement);
                W.commit();
                delete cancel;
            }
        }
    }
}

// Dummy function
void TransactionRequest::executeRequest(connection* C) {
    this->is_valid = 1;
}


Query::Query(int TransId, Server& server, int accountId) : transId(TransId), server(server), accountId(accountId) {}


void Query::execute(work& W, XMLElement* resultsElement) {
    lock_guard<mutex> guard(server.dbMutex);
    // Invalid if transaction does not exist
    result queryResult = W.exec("SELECT state, amount, limit_price, time FROM orders WHERE trans_id = " + to_string(transId));
    if (queryResult.empty()){
        XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
        errorElement->SetAttribute("id", to_string(transId).c_str());
        errorElement->SetText("No Transaction found");
        resultsElement->InsertEndChild(errorElement);
        return; 
    }
    // Invalid if account id does not match 
    result check_account = W.exec("SELECT account_id FROM orders WHERE trans_id = " + to_string(transId));
    int order_account = check_account[0][0].as<int>();
    if (order_account != this->accountId){
        XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
        errorElement->SetAttribute("id", to_string(transId).c_str());
        errorElement->SetText("This is not your order");
        resultsElement->InsertEndChild(errorElement);
        return; 
    }

    XMLElement* statusElement = resultsElement->GetDocument()->NewElement("status");
    statusElement->SetAttribute("id", to_string(transId).c_str());

    for (const auto& row : queryResult) {
        string state = row["state"].as<string>();
        if (state == "OPEN") {
            XMLElement* openElement = resultsElement->GetDocument()->NewElement("open");
            openElement->SetAttribute("shares", to_string(row["amount"].as<int>()).c_str());
            statusElement->InsertEndChild(openElement);
        } else if (state == "CANCELLED") {
            XMLElement* canceledElement = resultsElement->GetDocument()->NewElement("canceled");
            canceledElement->SetAttribute("shares", to_string(row["amount"].as<int>()).c_str());
            canceledElement->SetAttribute("time", row["time"].c_str());
            statusElement->InsertEndChild(canceledElement);
        } else if (state == "EXECUTED") {
            XMLElement* executedElement = resultsElement->GetDocument()->NewElement("executed");
            executedElement->SetAttribute("shares", to_string(row["amount"].as<int>()).c_str());
            executedElement->SetAttribute("price", to_string(row["limit_price"].as<double>()).c_str());
            executedElement->SetAttribute("time", row["time"].c_str());
            statusElement->InsertEndChild(executedElement);
        }
    }

    resultsElement->InsertEndChild(statusElement);
}

Cancel::Cancel(int TransId, Server& server, int accountId) : transId(TransId), server(server), accountId(accountId) {}

void Cancel::execute(work& W, XMLElement* resultsElement) {
    lock_guard<mutex> guard(server.dbMutex);
    // Invalid if transaction does not exist
    result cancelResult = W.exec("SELECT amount, limit_price FROM orders WHERE trans_id = " + to_string(transId));
    if (cancelResult.empty()){
        XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
        errorElement->SetAttribute("id", to_string(transId).c_str());
        errorElement->SetText("No Transaction found");
        resultsElement->InsertEndChild(errorElement);
        return; 
    }
    result check_account = W.exec("SELECT account_id FROM orders WHERE trans_id = " + to_string(transId));
    int order_account = check_account[0][0].as<int>();
    if (order_account != this->accountId){
        XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
        errorElement->SetAttribute("id", to_string(transId).c_str());
        errorElement->SetText("This is not your order");
        resultsElement->InsertEndChild(errorElement);
        return; 
    }
    result orderResult = W.exec("SELECT time, amount, limit_price, account_id FROM orders WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
    result executedResult = W.exec("SELECT amount, limit_price, time FROM orders WHERE trans_id = " + to_string(transId) + " AND state = 'EXECUTED'");
    result canceledResult = W.exec("SELECT amount, limit_price, time FROM orders WHERE trans_id = " + to_string(transId) + " AND state = 'CANCELLED'");
    
    if (orderResult.empty()) {
        XMLElement* errorElement = resultsElement->GetDocument()->NewElement("error");
        errorElement->SetAttribute("id", to_string(transId).c_str());
        errorElement->SetText("This order has no open part to be canceled");
        resultsElement->InsertEndChild(errorElement);
        return; 
    }
    
    W.exec("UPDATE orders SET state = 'CANCELLED', time = NOW() WHERE trans_id = " + to_string(transId) + " AND state = 'OPEN'");
    
    // Refund
    if (!orderResult.empty()) {
        int amount = orderResult[0]["amount"].as<int>();
        double limitPrice = orderResult[0]["limit_price"].as<double>();
        int accountID = orderResult[0]["account_id"].as<int>();
        W.exec("UPDATE accounts SET balance = balance + " + to_string(amount * limitPrice) + " WHERE account_id = " + to_string(accountID));
    }

    // Canceled
    XMLElement* canceledElement = resultsElement->GetDocument()->NewElement("canceled");
    canceledElement->SetAttribute("id", to_string(transId).c_str());
    for (const auto& row : orderResult) {
        string state = "canceled";
        XMLElement* childElement = resultsElement->GetDocument()->NewElement(state.c_str());
        childElement->SetAttribute("shares", to_string(row["amount"].as<int>()).c_str());
        childElement->SetAttribute("time", row["time"].as<string>().c_str());
        canceledElement->InsertEndChild(childElement);
    }


    // Executed
    for (const auto& row : executedResult) {
        string state = "executed";
        XMLElement* childElement = resultsElement->GetDocument()->NewElement(state.c_str());
        childElement->SetAttribute("shares", to_string(row["amount"].as<int>()).c_str());
        childElement->SetAttribute("time", row["time"].as<string>().c_str());
        childElement->SetAttribute("price", to_string(row["limit_price"].as<double>()).c_str());
        canceledElement->InsertEndChild(childElement);
    }
    resultsElement->InsertEndChild(canceledElement);
}
