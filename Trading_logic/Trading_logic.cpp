/*How does this work:
 
  1.The code starts by defining various constants related to the trading strategy, such as the periods 
  for moving averages and the Relative Strength Index (RSI), as well as thresholds for RSI that define 
  overbought and oversold conditions.
  
  2.It initializes state variables for short-term and long-term Simple Moving Averages (SMA), RSI, and 
  a volatility index.Functions like updateMovingAverages, updateRSI, and calculateVolatilityIndex are 
  declared but not defined here. They are meant to update the respective indicators as new price data 
  comes in.
  
  3.The main function simple_threshold continuously reads from input streams that supply bid and ask 
  orders, time, and metadata. It updates the moving averages and RSI with the latest price, which is 
  an average of the bid and ask prices.It calculates a volatility index based on the difference 
  between the short-term and long-term SMAs.
  
  4. Risk per trade is controlled by the maxRiskPerTrade parameter, which defines the maximum risk 
  allowed for each trade as a percentage of the total capital. The totalCapital variable is a mock-up 
  for demonstration purposes, representing the total capital available for risk management calculations.
  A cooldown period is implemented to avoid rapid and frequent trading, which can be detrimental due to 
  market volatility and transaction costs.*/
  
#include "Trading_logic.hpp"
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

// Trade execution thresholds
const float tradeThresholdMultiplier = 1.1;
const float volatilityThreshold = 0.05;

// Mock parameters for demonstration
float totalCapital = 100000.0; // Mock total capital for risk management calculation

// Update Moving Averages
void updateMovingAverages(float price, float &shortTermSMA, float &longTermSMA, int &updateCounter) {
    static float shortTermPrices[shortTermPeriod] = {0};
    static float longTermPrices[longTermPeriod] = {0};

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

// Update RSI
void updateRSI(float price, float &rsi, int &updateCounter) {
    static float previousPrice = price;  // Initialize with the first price
    static float totalGain = 0;
    static float totalLoss = 0;
    static float averageGain = 0;
    static float averageLoss = 0;

    float gain = 0;
    float loss = 0;
    float change = price - previousPrice;

    if (updateCounter > 0) {  // Skip the first update to avoid self-comparison
        gain = std::max(change, 0.0f);
        loss = std::max(-change, 0.0f);
    }

    if (updateCounter < rsiPeriod) {
        // Accumulate sums for the initial SMA calculation
        totalGain += gain;
        totalLoss += loss;
        if (updateCounter == rsiPeriod - 1) {
            averageGain = totalGain / rsiPeriod;
            averageLoss = totalLoss / rsiPeriod;
        }
    } else {
        // Apply EMA formula
        averageGain = (gain * (2.0f / (1 + rsiPeriod))) + averageGain * (1 - (2.0f / (1 + rsiPeriod)));
        averageLoss = (loss * (2.0f / (1 + rsiPeriod))) + averageLoss * (1 - (2.0f / (1 + rsiPeriod)));
    }

    // Calculate RSI
    if (averageLoss == 0) {
        rsi = 100;
    } else {
        float rs = averageGain / averageLoss;
        rsi = 100 - (100 / (1 + rs));
    }

    previousPrice = price;  // Update the previous price for the next call
    updateCounter++;
}


// Calculate Volatility Index
float calculateVolatilityIndex(float shortTermSMA, float longTermSMA) {
    return fabs(shortTermSMA - longTermSMA) / longTermSMA;
}

order createOrder(const order& bid, const order& ask, bool buy) {
    order newOrder;
    newOrder.price = buy ? bid.price : ask.price;
    newOrder.size = buy ? bid.size : ask.size;
    newOrder.orderID = buy ? bid.orderID : ask.orderID;
    newOrder.direction = buy ? 1 : 0;  // Assuming 1 for buy, 0 for sell
    return newOrder;
}

void trading_logic(stream<order> &top_bid, stream<order> &top_ask,
                      stream<Time> &incoming_time, stream<metadata> &incoming_meta,
                      stream<order> &outgoing_order, stream<Time> &outgoing_time,
                      stream<metadata> &outgoing_meta, ap_ufixed<16,8> &shortTermSMA_out, 
                      ap_ufixed<16,8> &longTermSMA_out, ap_ufixed<16,8> &rsi_out) {

    static int updateCounter = 0;

    if (!top_bid.empty() && !top_ask.empty()) {
        order bid = top_bid.read();
        order ask = top_ask.read();

        updateMovingAverages((bid.price + ask.price) / 2, shortTermSMA, longTermSMA, updateCounter);
        updateRSI((bid.price + ask.price) / 2, rsi, updateCounter);

        // Define trading conditions based on SMA and RSI
        bool smaCondition = (shortTermSMA_out > longTermSMA_out); // Example condition: short-term SMA above long-term SMA
        bool rsiCondition = (rsi_out > 30 && rsi_out < 70); // Example condition: RSI not in the oversold or overbought zone

        // Execute trade if bid price >= ask price and all additional conditions are met
        if (bid.price >= ask.price && smaCondition && rsiCondition) {
            order tradeOrder = createOrder(bid, ask, true);
            if (!outgoing_order.full()) {
                outgoing_order.write(tradeOrder);
                if (!incoming_time.empty() && !incoming_meta.empty() && !outgoing_time.full() && !outgoing_meta.full()) {
                    Time t = incoming_time.read();
                    metadata m = incoming_meta.read();
                    outgoing_meta.write(m);
                    outgoing_time.write(t);
                }
            }
        }
    }

    shortTermSMA_out = shortTermSMA;
    longTermSMA_out = longTermSMA;
    rsi_out = rsi;
}

