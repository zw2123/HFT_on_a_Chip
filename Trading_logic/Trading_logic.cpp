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
#include <ap_fixed.h>
#include <algorithm> 

// Define fixed-point types for optimized arithmetic operations
using fixed_t = ap_fixed<16, 4>; // Adjust these parameters based on the required precision and range
using fixed_t_large = ap_fixed<32, 8>; // For calculations that require a larger range or more precision

const int shortTermPeriod = 5;
const int longTermPeriod = 20;
const int rsiPeriod = 14;

// Thresholds and constants defined as fixed-point for consistency
const fixed_t upperRsiThreshold = 70.0;
const fixed_t lowerRsiThreshold = 30.0;
const fixed_t maxRiskPerTrade = 0.02;
const fixed_t tradeThresholdMultiplier = 1.1;
const fixed_t volatilityThreshold = 0.05;

// Use fixed-point types for all variables to maintain consistency and optimize FPGA resource usage
fixed_t shortTermSMA = 0.0;
fixed_t longTermSMA = 0.0;
fixed_t rsi = 50.0; // Starting RSI in the middle range
fixed_t volatilityIndex = 0.0;

fixed_t totalCapital = 100000.0; // Use fixed-point for capital representation

// Optimized moving average calculation using fixed-point arithmetic


void updateMovingAverages(fixed_t price, fixed_t &shortTermSMA, fixed_t &longTermSMA, int &updateCounter) {
    #pragma HLS INLINE off
    static fixed_t shortTermPrices[shortTermPeriod] = {};
    static fixed_t longTermPrices[longTermPeriod] = {};
    static fixed_t_large shortSum = 0, longSum = 0; // Now static to maintain state across calls

    #pragma HLS ARRAY_PARTITION variable=shortTermPrices complete dim=1
    #pragma HLS ARRAY_PARTITION variable=longTermPrices complete dim=1

    // Update running sums by subtracting the oldest value and adding the new one
    if (updateCounter >= shortTermPeriod) { // Ensure we have filled the array before subtracting
        shortSum -= shortTermPrices[updateCounter % shortTermPeriod];
    }
    shortTermPrices[updateCounter % shortTermPeriod] = price;
    shortSum += price;

    if (updateCounter >= longTermPeriod) {
        longSum -= longTermPrices[updateCounter % longTermPeriod];
    }
    longTermPrices[updateCounter % longTermPeriod] = price;
    longSum += price;

    // Update the SMAs
    if (updateCounter >= shortTermPeriod - 1) { // Start calculating average after array is first filled
        shortTermSMA = shortSum / shortTermPeriod;
    }
    if (updateCounter >= longTermPeriod - 1) {
        longTermSMA = longSum / longTermPeriod;
    }

    updateCounter++;
}

using fixed_t = ap_fixed<16, 4>; // Adjust based on required precision and range
using fixed_t_large = ap_fixed<32, 8>; // For larger range calculations

// Updated updateRSI function
void updateRSI(fixed_t price, fixed_t &rsi, int &updateCounter) {
   #pragma HLS INLINE off
    static fixed_t previousPrice = price; // Initialization assumes first call provides the initial price
    static fixed_t_large totalGain = 0, totalLoss = 0;
    static fixed_t averageGain = 0, averageLoss = 0;

    fixed_t change = price - previousPrice;
    fixed_t gain = (change > 0) ? change : fixed_t(0); // Explicitly cast to fixed_t to avoid ambiguity
    fixed_t loss = (change < 0) ? fixed_t(-change) : fixed_t(0);

    #pragma HLS PIPELINE
    if (updateCounter < rsiPeriod) {
        totalGain += gain;
        totalLoss += loss;
    } else {
        averageGain = ((rsiPeriod - 1) * averageGain + gain) / rsiPeriod;
        averageLoss = ((rsiPeriod - 1) * averageLoss + loss) / rsiPeriod;
    }

   fixed_t rs = averageLoss != fixed_t(0) ? fixed_t(averageGain / averageLoss) : fixed_t(0);


    rsi = 100 - (100 / (1 + rs));

    previousPrice = price;
    updateCounter++;
}


order createOrder(const order& bid, const order& ask, bool buy) {
    // Direct initialization reduces unnecessary logic and operations
    return order{
        .price = buy ? bid.price : ask.price,
        .size = buy ? bid.size : ask.size,
        .orderID = buy ? bid.orderID : ask.orderID,
        .direction = buy ? 1 : 0
    };
}


void trading_logic(stream<order> &top_bid, stream<order> &top_ask,
                      stream<Time> &incoming_time, stream<metadata> &incoming_meta,
                      stream<order> &outgoing_order, stream<Time> &outgoing_time,
                      stream<metadata> &outgoing_meta, ap_ufixed<16,8> &shortTermSMA_out, 
                      ap_ufixed<16,8> &longTermSMA_out, ap_ufixed<16,8> &rsi_out) {

    #pragma HLS INLINE off
    #pragma HLS PIPELINE

    static int updateCounter = 0;

    if (!top_bid.empty() && !top_ask.empty() && !outgoing_order.full()) {
        order bid = top_bid.read();
        order ask = top_ask.read();

        // Assuming updateMovingAverages and updateRSI are optimized for parallel execution
        updateMovingAverages((bid.price + ask.price) / 2, shortTermSMA, longTermSMA, updateCounter);
        updateRSI((bid.price + ask.price) / 2, rsi, updateCounter);

        bool tradeCondition = bid.price >= ask.price && (shortTermSMA_out > longTermSMA_out) && (rsi_out > 30 && rsi_out < 70);

        if (tradeCondition && !incoming_time.empty() && !incoming_meta.empty() && !outgoing_time.full() && !outgoing_meta.full()) {
            order tradeOrder = createOrder(bid, ask, true);
            outgoing_order.write(tradeOrder);
            
            Time t = incoming_time.read();
            metadata m = incoming_meta.read();
            outgoing_meta.write(m);
            outgoing_time.write(t);
        }
    }

    shortTermSMA_out = shortTermSMA;
    longTermSMA_out = longTermSMA;
    rsi_out = rsi;
}
