#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <iomanip>
#include <sstream> // Include the header file for ostringstream
#include "hls_stream.h"
#include "Trading_logic.hpp"
#include "ap_int.h"

struct TestCase {
    order bid;
    order ask;
    bool hasExpectedTrade;
    order expectedTrade;
};

void clearStreams(hls::stream<order>& top_bid, hls::stream<order>& top_ask,
                  hls::stream<order>& outgoing_order, hls::stream<ap_uint<64>>& incoming_time,
                  hls::stream<ap_uint<64>>& outgoing_time, hls::stream<metadata>& incoming_meta,
                  hls::stream<metadata>& outgoing_meta) {
    // Clear all streams to avoid leftover data
    while (!top_bid.empty()) top_bid.read();
    while (!top_ask.empty()) top_ask.read();
    while (!outgoing_order.empty()) outgoing_order.read();
    while (!incoming_time.empty()) incoming_time.read();
    while (!outgoing_time.empty()) outgoing_time.read();
    while (!incoming_meta.empty()) incoming_meta.read();
    while (!outgoing_meta.empty()) outgoing_meta.read();
}

long long executeTest(hls::stream<order>& top_bid, hls::stream<order>& top_ask,
                      hls::stream<ap_uint<64>>& incoming_time, hls::stream<metadata>& incoming_meta,
                      hls::stream<order>& outgoing_order, hls::stream<ap_uint<64>>& outgoing_time,
                      hls::stream<metadata>& outgoing_meta, const TestCase& testCase, int& correctCount, int testCaseNum) {
    
    ap_ufixed<16,8> shortTermSMA_out, longTermSMA_out, rsi_out;

    top_bid.write(testCase.bid);
    top_ask.write(testCase.ask);

    auto start = std::chrono::high_resolution_clock::now();

    trading_logic(top_bid, top_ask, incoming_time, incoming_meta, outgoing_order, outgoing_time, outgoing_meta, shortTermSMA_out, longTermSMA_out, rsi_out);

    auto end = std::chrono::high_resolution_clock::now();

    long long latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    bool resultMatched = testCase.hasExpectedTrade == (!outgoing_order.empty());

    std::ostringstream smaStream;
    smaStream << shortTermSMA_out << "/" << longTermSMA_out;
    std::string smaString = smaStream.str();

// Calculate the maximum length of the SMA string
int maxSmaLength = smaString.length(); 

std::cout << "Case " << std::setw(3) << testCaseNum
              << "; Bid: $" << std::setw(3) << testCase.bid.price << std::setw(8)
              << "; Ask: $" << std::setw(3) << testCase.ask.price << std::setw(8)
              << "; Expected: " << std::setw(9) << (testCase.hasExpectedTrade ? "Trade" : "No Trade")<< std::setw(8)
              << "; Act: " << std::setw(9) << (!outgoing_order.empty() ? "Trade" : "No Trade")<< std::setw(8)
              << "; Result: " << std::setw(9) << (resultMatched ? "Correct" : "Incorrect")<< std::setw(8)
              << "; RSI: " << std::setw(10) << rsi_out<< std::setw(8)
              << "; Latency: " << latency << " ns"
              << "; Short/Long term SMA: " << shortTermSMA_out << "/" << longTermSMA_out
              << std::endl;

    
    clearStreams(top_bid, top_ask, outgoing_order, incoming_time, outgoing_time, incoming_meta, outgoing_meta);

    if (resultMatched) {
        correctCount++;
    }

    return latency;
}




int main() {
    hls::stream<order> top_bid, top_ask, outgoing_order;
    hls::stream<ap_uint<64>> incoming_time, outgoing_time;
    hls::stream<metadata> incoming_meta, outgoing_meta;

    std::vector<TestCase> testCases = {
    // Test cases where a trade is expected
    // Test cases where trades should occur (Bid >= Ask)
    {{135, 0, 135.0, 100}, {134, 1, 134.0, 100}, false, {134, 1, 134.0, 100}},
    {{147, 0, 147.0, 150}, {146, 1, 146.0, 150}, false, {146, 1, 146.0, 150}},
    {{159, 0, 159.0, 100}, {158, 1, 158.0, 100}, false, {158, 1, 158.0, 100}},
    {{173, 0, 173.0, 150}, {172, 1, 172.0, 150}, false, {172, 1, 172.0, 150}},
    {{184, 0, 184.0, 100}, {183, 1, 183.0, 100}, false, {183, 1, 183.0, 100}},

    // Test cases where trades should not occur (Bid < Ask)
    {{142, 0, 142.0, 100}, {143, 1, 143.0, 100}, false, {}},
    {{156, 0, 156.0, 150}, {157, 1, 157.0, 150}, false, {}},
    {{168, 0, 168.0, 100}, {169, 1, 169.0, 100}, false, {}},
    {{179, 0, 179.0, 150}, {180, 1, 180.0, 150}, false, {}},
    {{191, 0, 191.0, 100}, {192, 1, 192.0, 100}, false, {}},
    // Mixed cases
    {{153, 0, 150.0, 100}, {152, 1, 152.0, 100}, false, {152, 1, 152.0, 100}},
    {{161, 0, 160.0, 150}, {162, 1, 162.0, 150}, false, {}},
    {{174, 0, 175.0, 100}, {173, 1, 173.0, 100}, true, {173, 1, 173.0, 100}},
    {{187, 0, 186.0, 150}, {188, 1, 188.0, 150}, false, {}},
    {{198, 0, 199.0, 100}, {197, 1, 197.0, 100}, true, {197, 1, 197.0, 100}},

    {{200, 0, 200.0, 150}, {200, 1, 200.0, 150}, true, {200, 1, 200.0, 150}},
    {{215, 0, 215.0, 100}, {214, 1, 214.0, 100}, true, {214, 1, 214.0, 100}},
    {{225, 0, 226.0, 150}, {225, 1, 225.0, 150}, false, {225, 1, 225.0, 150}},
    {{235, 0, 234.0, 100}, {236, 1, 236.0, 100}, false, {}},
    {{245, 0, 245.0, 150}, {244, 1, 244.0, 150}, false, {244, 1, 244.0, 150}},
};

    int correctCount = 0;
    auto totalStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < testCases.size(); ++i) {
        executeTest(top_bid, top_ask, incoming_time, incoming_meta, outgoing_order, outgoing_time, outgoing_meta, testCases[i], correctCount, i + 1);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    long long totalExecutionTime = std::chrono::duration_cast<std::chrono::nanoseconds>(totalEnd - totalStart).count();

    double correctionRate = static_cast<double>(correctCount) / testCases.size();

    std::cout << "Correction Rate: " << correctionRate * 100 << "%" << std::endl;

    return 0;
}
