#include "client.h"
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

Client::Client(boost::asio::io_context& io_context, const string& server, const string& port, int account_id)
    : io_context_(io_context), socket_(io_context), server_(server), port_(port), account_id_(account_id) {}

void Client::start() {
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(server_, port_);
    boost::asio::async_connect(socket_, endpoints, boost::bind(&Client::handleConnect, this, boost::asio::placeholders::error));
}

void Client::handleConnect(const boost::system::error_code& ec) {
    if (!ec) {
        std::string firstXml = readXmlFromFile("xml/create1.xml");
        sendRequest(firstXml, [this]() {
            std::string secondXml = readXmlFromFile("xml/order1.xml");
            sendRequest(secondXml, [this]() {
                std::string thirdXml = readXmlFromFile("xml/order2.xml");
                sendRequest(thirdXml, [this]() {
                    std::string fourthXml = readXmlFromFile("xml/order3.xml");
                    sendRequest(fourthXml, []() {
                        std::cout << "All requests sent." << std::endl;
                    });
                });
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
    std::cout << xml << std::endl;

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
                                                                    std::cout << "Response received from server: " << response << std::endl;
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

std::string Client::readXmlFromFile(const std::string& fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
    return content;
}
