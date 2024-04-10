#include "createrequest.h"
#include <tinyxml2.h>

using namespace std;
using boost::asio::ip::tcp;
namespace asio = boost::asio;
using namespace pqxx;
using namespace tinyxml2;

void CreateRequest::parseRequest(connection* C) {
    const XMLElement* accountElement = rootElement->FirstChildElement("account");
    while (accountElement) {
        int accountId = stoi(accountElement->Attribute("id"));
        float balance = stof(accountElement->Attribute("balance"));
        accountsToCreate.push_back(Account(accountId, balance));
        accountElement = accountElement->NextSiblingElement("account");
    }
    const XMLElement* symbolElement = rootElement->FirstChildElement("symbol");
    while (symbolElement) {
        string symbolName = symbolElement->Attribute("sym");
        vector<pair<int, int>> accounts;
        const XMLElement* accountElement = symbolElement->FirstChildElement("account");
        while (accountElement) {
            int accountId = stoi(accountElement->Attribute("id"));
            int quantity = stoi(accountElement->GetText());
            accounts.push_back(make_pair(accountId, quantity));
            accountElement = accountElement->NextSiblingElement("account");
        }
        symbolsToCreate.push_back(make_pair(symbolName, accounts));
        symbolElement = symbolElement->NextSiblingElement("symbol");
    }
}

void CreateRequest::executeRequest(connection* C) {
    lock_guard<mutex> guard(server.dbMutex);
    work W(*C);
    XMLElement* resultsElement = responseDoc.NewElement("results");
    responseDoc.InsertFirstChild(resultsElement);

    // Create account
    for (const auto& account : accountsToCreate) {
        result R = W.exec("SELECT 1 FROM accounts WHERE account_id = " + to_string(account.getId()));
        if (R.empty()) {
            W.exec("INSERT INTO accounts (account_id, balance) VALUES (" + 
                   to_string(account.getId()) + ", " + to_string(account.getBalance()) + ")");
            XMLElement* createdElement = responseDoc.NewElement("created");
            createdElement->SetAttribute("id", account.getId());
            resultsElement->InsertEndChild(createdElement);
        // Account create error
        } else {
            XMLElement* errorElement = responseDoc.NewElement("error");
            errorElement->SetAttribute("id", account.getId());
            errorElement->SetText("Account already exists");
            resultsElement->InsertEndChild(errorElement);
        }
    }

    // Create symbol
    for (const auto& symbolPair : symbolsToCreate) {
        const string& symbolName = symbolPair.first;
        for (const auto& accountPair : symbolPair.second) {
            int accountId = accountPair.first;
            int quantity = accountPair.second;
            // check if accout exist before adding symbol
            result account_check = W.exec("SELECT 1 FROM accounts WHERE account_id = " + to_string(accountId));
            // Invalid if symbol quantity is negative
            if (quantity < 0) {
                XMLElement* errorElement = responseDoc.NewElement("error");
                errorElement->SetAttribute("sym", symbolName.c_str());
                errorElement->SetAttribute("id", accountId);
                errorElement->SetText("Symbol cannot be created with negative number");
                resultsElement->InsertEndChild(errorElement);
            // Invalid if account does not exist
            } else if (account_check.empty()){
                XMLElement* errorElement = responseDoc.NewElement("error");
                errorElement->SetAttribute("sym", symbolName.c_str());
                errorElement->SetAttribute("id", accountId);
                errorElement->SetText("This account must exist before adding a symbol to it");
                resultsElement->InsertEndChild(errorElement);
            // Valid
            } else {
                // Check if the account already hold this symbol
                result symbol_check = W.exec("SELECT amount FROM symbols WHERE account_id = " + to_string(accountId) + " AND symbol = '" + symbolName + "'");
                // If already hold, update holding
                if (!symbol_check.empty()){
                    int currentAmount = symbol_check[0][0].as<int>();
                    int newAmount = currentAmount + quantity;
                    W.exec("UPDATE symbols SET amount = " + to_string(newAmount) + " WHERE account_id = " + to_string(accountId) + " AND symbol = '" + symbolName + "'");
                    XMLElement* createdElement = responseDoc.NewElement("created");
                    createdElement->SetAttribute("sym", symbolName.c_str());
                    createdElement->SetAttribute("id", accountId);
                    resultsElement->InsertEndChild(createdElement);
                // If not already hold, add a record
                } else {
                    W.exec("INSERT INTO symbols (account_id, symbol, amount) VALUES (" + 
                        to_string(accountId) + ", '" + symbolName + "', " + to_string(quantity) + ")");
                    XMLElement* createdElement = responseDoc.NewElement("created");
                    createdElement->SetAttribute("sym", symbolName.c_str());
                    createdElement->SetAttribute("id", accountId);
                    resultsElement->InsertEndChild(createdElement);
                }
            }
        }
    }
    W.commit();
}
