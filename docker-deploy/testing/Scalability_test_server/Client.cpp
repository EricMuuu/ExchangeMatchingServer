#include "Client.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <boost/bind/bind.hpp>
#include <random>
#include "tinyxml2.h"
#include <sstream>

using namespace tinyxml2;
using std::string;
using std::to_string;

std::vector<std::string> symbolName = {"SPY", "BTC", "T5asdf"};

Client::Client(boost::asio::io_context& io_context, const std::string& server, const std::string& port, int account_id)
    : io_context_(io_context), socket_(io_context), server_(server), port_(port), account_id_(account_id) {}

void Client::start() {
    // Resolve the server address
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(server_, port_);
    boost::asio::async_connect(socket_, endpoints,
    boost::bind(&Client::handleConnect, this, boost::asio::placeholders::error));
}

void Client::handleConnect(const boost::system::error_code& ec) {
    if (!ec) {
        // add test here
        std::string createXml = generateCreateXML();
        sendRequest(createXml, [this]() {
           std::string transXml = generateTransXML();
            sendRequest(transXml, []() {
                //std::cout << "All requests sent." << std::endl;
            });
        });
        // sendRequest(createXml, []() {
        //     std::cout << "Create request sent." << std::endl;
        // });
    } else {
        std::cerr << "Connect error: " << ec.message() << std::endl;
    }
}

void Client::sendRequest(const std::string& xml, std::function<void()> onCompletion) {
    unsigned int xml_length_net = htonl(xml.length());
    //std::cout << xml << std::endl;

    boost::asio::async_write(socket_, boost::asio::buffer(&xml_length_net, sizeof(xml_length_net)),
                             [this, xml, onCompletion](const boost::system::error_code& ec, std::size_t) {
                                 if (!ec) {
                                     boost::asio::async_write(socket_, boost::asio::buffer(xml),
                                                              [this, onCompletion](const boost::system::error_code& ec, std::size_t) {
                                                                  if (!ec) {
                                                                      this->readResponse(onCompletion);
                                                                  } else {
                                                                      std::cerr << "Write XML data error: " << ec.message() << std::endl;
                                                                  }
                                                              });
                                 } else {
                                     std::cerr << "Write XML length error: " << ec.message() << std::endl;
                                 }
                             });
}

void Client::readResponse(std::function<void()> onCompletion) {
    auto responseLengthBuffer = std::make_shared<std::vector<char>>(sizeof(unsigned int));
    boost::asio::async_read(socket_, boost::asio::buffer(*responseLengthBuffer),
                            [this, responseLengthBuffer, onCompletion](const boost::system::error_code& ec, std::size_t) {
                                if (!ec) {
                                    unsigned int responseLength = ntohl(*reinterpret_cast<unsigned int*>(responseLengthBuffer->data()));
                                    auto buffer = std::make_shared<std::vector<char>>(responseLength);
                                    boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
                                                            [this, buffer, onCompletion](const boost::system::error_code& ec, std::size_t) {
                                                                if (!ec) {
                                                                    std::string response(buffer->begin(), buffer->end());
                                                                   // std::cout << "Response received from server: " << response << std::endl;
                                                                    onCompletion(); // Proceed to next step
                                                                } else {
                                                                    std::cerr << "Read XML response error: " << ec.message() << std::endl;
                                                                }
                                                            });
                                } else {
                                    std::cerr << "Read response length error: " << ec.message() << std::endl;
                                }
                            });
}




void Client::createAccount(int account_id, float balance, XMLDocument& request) {
    XMLElement* root = request.RootElement();
    XMLElement* account = request.NewElement("account");
    account->SetAttribute("id", account_id);
    account->SetAttribute("balance", balance);
    root->InsertEndChild(account);
}

void Client::createSymbol(const std::string& sym, int account_id, int amount, XMLDocument& request) {
    XMLElement* root = request.RootElement();
    XMLElement* symbol = request.NewElement("symbol");
    symbol->SetAttribute("sym", sym.c_str());

    XMLElement* account = request.NewElement("account");
    account->SetAttribute("id", account_id);
    XMLText* num = request.NewText(to_string(amount).c_str());
    account->InsertFirstChild(num);
    symbol->InsertEndChild(account);
    root->InsertEndChild(symbol);
}

void Client::addOrder(const std::string& sym, int amount, float limit_price, XMLDocument& request) {
    XMLElement* root = request.RootElement();
    XMLElement* order = request.NewElement("order");
    order->SetAttribute("sym", sym.c_str());
    order->SetAttribute("amount", amount);
    order->SetAttribute("limit", limit_price);
    root->InsertEndChild(order);
}

void Client::addQuery(XMLDocument& request, int transid) {
    XMLElement* root = request.RootElement();
    XMLElement* query = request.NewElement("query");
    query->SetAttribute("id", transid);
    root->InsertEndChild(query);
}

void Client::addCancel(XMLDocument& request, int transid) {
    XMLElement* root = request.RootElement();
    XMLElement* cancel = request.NewElement("cancel");
    cancel->SetAttribute("id", transid);
    root->InsertEndChild(cancel);
}

std::string Client::generateCreateXML() {
    XMLDocument doc;
    auto* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    auto* create = doc.NewElement("create");
    doc.InsertEndChild(create);

    createAccount(account_id_, INIT_BAL, doc);

    for (int i = 0; i < SYM_NUM; i++) {
        std::string sym = symbolName[i % symbolName.size()];
        createSymbol(sym, account_id_, INIT_SYM, doc);
    }

    XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}

std::string Client::generateTransXML() {
    XMLDocument doc;
    auto* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    auto* transactions = doc.NewElement("transactions");
    transactions->SetAttribute("id", 0);
    // TODO: later should randomly generate accountid
    // transactions->SetAttribute("id", account_id_);
    doc.InsertEndChild(transactions);

    // send order request
    for (int i = 0; i < NUM_ORDER; i++) {
        std::string sym = symbolName[i % symbolName.size()];
        addOrder(sym, getRandomINT(1, INIT_SYM), getRandomINT(LLIMIT_PRICE, HLIMIT_PRICE), doc); // Buy
        addOrder(sym, -getRandomINT(1, INIT_SYM), getRandomINT(LLIMIT_PRICE, HLIMIT_PRICE), doc); // Sell
    }

    // send query request
    for (int i = 126; i > 119; i--) {
        addQuery(doc, i);
    }

    // send cancel request
    for (int i = 22; i > NUM_ORDER; i--) {
        addCancel(doc, i);
    }

    XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}


int Client::getRandomINT(int min, int max) {
    static std::random_device rd;  
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}


