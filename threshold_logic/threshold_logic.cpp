#include "threshold_logic.hpp"
#include <cmath> 
#include <ap_fixed.h>

// Enhanced trading strategy parameters
const int shortTermPeriod = 5;
const int longTermPeriod = 20;
const int rsiPeriod = 14; // Period for RSI calculation
const float upperRsiThreshold = 70.0; // Overbought threshold for RSI
const float lowerRsiThreshold = 30.0; // Oversold threshold for RSI
const float maxRiskPerTrade = 0.02; // Maximum risk per trade as a percentage of total capital

// Initialize moving averages, RSI, and volatility measurement
float shortTermSMA = 0.0;
float longTermSMA = 0.0;
float rsi = 50.0; // Starting RSI in the middle range
float volatilityIndex = 0.0;

// Cooldown and backtesting parameters
const int cooldownPeriod = 10;
int cooldownTimer = cooldownPeriod;

// Trade execution thresholds
const float tradeThresholdMultiplier = 1.1;
const float volatilityThreshold = 0.05;

// Mock parameters for demonstration
float totalCapital = 100000.0; // Mock total capital for risk management calculation

// Implement the utility functions mentioned in the strategy here:
// - updateMovingAverages
// - updateRSI
// - calculateVolatilityIndex
// - prepareTradeOrder
// These functions should encapsulate the logic for each respective calculation, 
// ensuring the main trading function remains clear and maintainable.
// Update Moving Averages
void updateMovingAverages(float price, float &shortTermSMA, float &longTermSMA, int &updateCounter) {
    static float shortTermPrices[shortTermPeriod] = {0};
    static float longTermPrices[longTermPeriod] = {0};

    #pragma HLS ARRAY_PARTITION variable=shortTermPrices complete dim=1
    #pragma HLS ARRAY_PARTITION variable=longTermPrices complete dim=1

    shortTermPrices[updateCounter % shortTermPeriod] = price;
    longTermPrices[updateCounter % longTermPeriod] = price;

    float shortSum = 0.0, longSum = 0.0;
    for(int i = 0; i < shortTermPeriod; ++i) {
        shortSum += shortTermPrices[i];
    }
    for(int i = 0; i < longTermPeriod; ++i) {
        longSum += longTermPrices[i];
    }
    shortTermSMA = shortSum / shortTermPeriod;
    longTermSMA = longSum / longTermPeriod;

    updateCounter++; // Ensure this counter is incremented elsewhere if not in a looping call
}

// Update RSI - Simplified version for HLS
void updateRSI(float price, float &rsi, int &updateCounter) {
    static float priceChanges[rsiPeriod] = {0};

    #pragma HLS ARRAY_PARTITION variable=priceChanges complete dim=1

    float gain = 0.0, loss = 0.0;

    // Calculate price change from previous price
    float previousPrice = (updateCounter > 0) ? priceChanges[(updateCounter - 1) % rsiPeriod] : price;
    float change = price - previousPrice;
    priceChanges[updateCounter % rsiPeriod] = price;

    // Classify change as gain or loss for RSI calculation
    if(change > 0) gain += change;
    else loss -= change;

    // Simplified average gain and loss calculation
    float avgGain = gain / rsiPeriod;
    float avgLoss = loss / rsiPeriod;

    // Calculate RSI
    float rs = (avgLoss == 0) ? 100 : (avgGain / avgLoss);
    rsi = 100 - (100 / (1 + rs));
}

// Calculate Volatility Index
float calculateVolatilityIndex(float shortTermSMA, float longTermSMA) {
    // Volatility index based on the percentage difference between short and long term SMAs
    return fabs(shortTermSMA - longTermSMA) / longTermSMA;
}

// Prepare Trade Order
order prepareTradeOrder(bool isUptrend, bool isDowntrend, order &bid, order &ask) {
    order tradeOrder = {0, 0, 0, 0}; // Initialize a dummy order

    // Convert tradeThresholdMultiplier to ap_ufixed<16, 8> before the operation
    ap_ufixed<16, 8> multiplier = tradeThresholdMultiplier;

    if(isUptrend) {
        tradeOrder.orderID = 123; // Example order ID
        tradeOrder.direction = 0; // Sell
        tradeOrder.size = bid.size; // Use bid size
        tradeOrder.price = bid.price * multiplier; // Adjusted price
    } else if(isDowntrend) {
        tradeOrder.orderID = 456; // Example order ID
        tradeOrder.direction = 1; // Buy
        tradeOrder.size = ask.size; // Use ask size
        tradeOrder.price = ask.price / multiplier; // Adjusted price
    }
    
    return tradeOrder;
}

// Main trading function with enhanced strategy
void simple_threshold(stream<order> &top_bid, stream<order> &top_ask,
                      stream<Time> &incoming_time, stream<metadata> &incoming_meta,
                      stream<order> &outgoing_order, stream<Time> &outgoing_time,
                      stream<metadata> &outgoing_meta) {

#pragma HLS INTERFACE axis port=top_bid
#pragma HLS INTERFACE axis port=top_ask
#pragma HLS INTERFACE axis port=incoming_time
#pragma HLS INTERFACE axis port=incoming_meta
#pragma HLS INTERFACE axis port=outgoing_order
#pragma HLS INTERFACE axis port=outgoing_time
#pragma HLS INTERFACE axis port=outgoing_meta

#pragma HLS INTERFACE ap_ctrl_none port=return

	static const ap_ufixed<16,8> bid_threshold = 27.4;
	static const ap_ufixed<16,8> ask_threshold = 27.4;

	static order market_buy;
	market_buy.price = 1;
	market_buy.orderID = 123;
	market_buy.direction = 1;
	market_buy.size = 2;

	static order market_sell;
	market_sell.price = 1;
	market_sell.orderID = 123;
	market_sell.direction = 0;
	market_sell.size = 2;

	static order prev_bid;
	static order prev_ask;

    static int updateCounter = 0;

    if (!top_bid.empty() && !top_ask.empty() && cooldownTimer <= 0) {
        order bid = top_bid.read();
        order ask = top_ask.read();
        
        // Enhanced strategy calculations
        updateMovingAverages((bid.price + ask.price) / 2, shortTermSMA, longTermSMA, updateCounter);
        updateRSI((bid.price + ask.price) / 2, rsi, updateCounter);
        volatilityIndex = calculateVolatilityIndex(shortTermSMA, longTermSMA);

        // Decision making based on technical indicators and volatility
        bool isUptrend = shortTermSMA > longTermSMA && rsi > 50;
        bool isDowntrend = shortTermSMA < longTermSMA && rsi < 50;
        
        if (volatilityIndex > volatilityThreshold) {
            order tradeOrder = prepareTradeOrder(isUptrend, isDowntrend, bid, ask);
            if (tradeOrder.size > 0) {
                outgoing_order.write(tradeOrder);
                cooldownTimer = cooldownPeriod;
                
                // Pass metadata and time with trade orders
                outgoing_meta.write(incoming_meta.read());
                outgoing_time.write(incoming_time.read());
            }
        }
    }

    // Cooldown management
    if (cooldownTimer > 0) {
        cooldownTimer--;
    }
}


