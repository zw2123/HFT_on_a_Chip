#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include "order_book.hpp"

using namespace std;
using namespace hls;

string directionToString(ap_uint<3> direction) {
    switch (direction.to_uint()) { // Convert ap_uint to unsigned int for comparison
        case 2: return "ASK";
        case 3: return "BID";
        case 4: return "REMOVE_ASK";
        case 5: return "REMOVE_BID";
        default: return "UNKNOWN";
    }
}

void order_book(stream<order> &order_stream,
                stream<Time> &incoming_time,
                stream<metadata> &incoming_meta,
                stream<order> &top_bid,
                stream<order> &top_ask,
                stream<Time> &outgoing_time,
                stream<metadata> &outgoing_meta,
                ap_uint<32> &top_bid_id,
                ap_uint<32> &top_ask_id);
int main() {
	//Output data-structures
	stream<order> top_bid_stream;
	stream<order> top_ask_stream;
	stream<Time> outgoing_time;
	stream<metadata> outgoing_meta;
	order top_bid;
	order top_ask;
	Time out_time;
	metadata out_meta;

	//Input data-structures
	stream<order> test_stream;
	stream<Time> test_time;
	stream<metadata> test_meta;
	sockaddr_in temp_1;
	metadata temp_meta;
	Time t;
	order test;

	//AXI-Lite
	ap_uint<32> top_bid_id;
	ap_uint<32> top_ask_id;

    ap_ufixed<16, 8> testprices [544] = {23.79, 32.73, 24.17, 23.26, 25.79, 24.84, 27.22, 25.85, 26.63, 27.53, 25.03, 29.46, 27.35, 27.56, 30.70, 25.07, 25.20, 31.65, 30.90, 31.00, 29.79, 26.20, 32.84, 32.14, 28.78, 28.34, 26.14, 24.10, 28.94, 24.58, 32.02, 26.23, 31.00, 24.10, 25.20, 30.53, 29.46, 29.51, 30.70, 30.53, 27.53, 29.23, 26.36, 32.84, 27.22, 25.73, 29.51, 25.96, 21.07, 30.90, 26.63, 29.88, 33.50, 23.26, 34.29, 24.58, 31.65, 32.14, 25.73, 25.82, 28.94, 26.12, 25.96, 33.50, 26.23, 29.88, 26.33, 25.07, 26.12, 32.73, 27.30, 26.36, 27.56, 27.78, 31.53, 26.20, 27.53, 28.34, 24.84, 31.17, 26.19, 28.27, 26.33, 27.35, 29.79, 33.91, 26.81, 27.59, 33.91, 28.27, 27.59, 26.81, 27.56, 28.78, 29.30, 26.21, 32.25, 26.21, 27.78, 26.79, 22.37, 27.83, 32.02, 27.83, 28.56, 24.71, 25.99, 22.37, 26.79, 29.37, 25.99, 31.53, 21.07, 26.14, 29.53, 25.03, 28.02, 23.99, 32.25, 26.42, 27.53, 29.23, 29.53, 26.19, 24.29, 28.56, 24.17, 25.79, 28.37, 23.99, 29.37, 25.82, 26.42, 24.71, 25.85, 23.02, 28.69, 29.17, 29.17, 29.04, 23.79, 31.22, 23.02, 28.37, 28.02, 28.69, 27.56, 29.04, 24.71, 31.17, 27.30, 29.82, 29.82, 19.67, 23.38, 26.31, 24.71, 26.43, 33.41, 25.33, 19.67, 23.38, 33.41, 24.73, 29.30, 26.31, 31.22, 25.33, 26.43, 24.73, 27.92, 14.04, 27.92, 14.04, 21.39, 21.39, 27.11, 27.11, 29.27, 33.20, 24.77, 28.10, 21.51, 25.00, 25.00, 28.22, 27.05, 21.51, 33.20, 29.27, 27.05, 28.22, 28.10, 26.26, 24.77, 26.26, 28.58, 25.46, 28.63, 25.42, 25.42, 23.62, 23.62, 28.63, 25.46, 28.58, 25.06, 24.34, 31.98, 30.37, 29.10, 30.67, 24.34, 28.52, 26.63, 26.63, 32.15, 30.98, 31.98, 26.26, 25.04, 23.32, 22.49, 27.01, 25.04, 30.67, 25.06, 23.32, 29.10, 21.53, 23.21, 29.41, 30.96, 23.21, 28.52, 28.92, 29.84, 30.96, 29.50, 30.98, 28.92, 26.13, 24.38, 26.26, 27.08, 24.42, 28.38, 26.13, 26.66, 27.01, 28.85, 28.21, 27.08, 26.44, 27.42, 24.26, 28.85, 31.06, 31.25, 29.50, 30.37, 29.41, 30.61, 27.41, 31.06, 27.86, 28.20, 24.42, 26.44, 23.97, 28.20, 27.26, 27.42, 21.53, 23.97, 25.99, 27.41, 28.41, 29.84, 28.38, 24.26, 31.87, 32.94, 30.35, 30.23, 27.91, 30.61, 28.41, 22.72, 28.73, 24.53, 23.97, 30.35, 22.72, 28.61, 30.23, 23.97, 26.70, 28.38, 21.11, 22.99, 31.25, 27.86, 26.66, 27.91, 28.10, 32.15, 24.53, 31.87, 27.26, 28.61, 25.99, 28.38, 22.50, 28.83, 26.70, 23.88, 32.34, 22.99, 23.88, 27.15, 23.74, 26.62, 34.11, 28.83, 30.27, 26.71, 27.01, 26.62, 29.60, 29.24, 30.86, 25.25, 32.90, 30.43, 21.48, 28.24, 28.73, 29.60, 27.69, 34.00, 32.90, 22.50, 23.74, 27.69, 27.20, 28.21, 30.43, 27.01, 24.05, 27.20, 26.35, 23.92, 31.72, 31.44, 27.72, 23.92, 27.66, 25.25, 25.83, 27.42, 24.05, 29.21, 28.10, 31.49, 30.27, 22.70, 31.81, 29.54, 25.38, 29.21, 26.78, 32.34, 27.06, 26.96, 26.70, 32.94, 31.44, 27.06, 25.31, 27.15, 36.56, 29.79, 29.54, 27.17, 27.47, 24.28, 25.96, 28.21, 31.49, 27.99, 24.28, 22.62, 28.21, 28.50, 22.62, 28.52, 21.11, 28.91, 29.79, 28.91, 28.78, 28.31, 28.52, 27.17, 27.94, 22.71, 29.00, 25.08, 28.71, 29.39, 27.99, 29.20, 25.83, 29.75, 31.13, 28.39, 28.78, 22.71, 24.38, 31.04, 22.49, 26.16, 26.59, 29.44, 26.80, 26.78, 20.53, 27.66, 28.39, 32.47, 29.20, 25.88, 28.95, 27.47, 27.94, 31.81, 25.31, 29.48, 30.86, 26.35, 25.60, 28.71, 34.11, 25.49, 25.96, 26.59, 26.90, 25.43, 23.21, 27.25, 30.58, 25.71, 29.41, 31.18, 29.48, 29.56, 26.56, 31.32, 25.43, 22.70, 28.95, 31.06, 31.13, 26.57, 21.01, 26.96, 29.41, 27.01, 28.57, 28.57, 26.13, 23.99, 25.53, 24.96, 23.99, 28.30, 26.90, 32.47, 31.32, 25.60, 25.38, 31.93, 23.70, 31.06, 24.81, 34.11, 28.77, 34.11, 22.94, 34.00, 20.22, 21.01, 26.48, 30.11, 29.66, 23.67, 26.48, 27.44, 24.58, 27.72, 31.17, 31.62, 31.18, 29.61, 26.70, 26.57, 30.11, 26.54, 23.21, 27.66, 25.71, 24.68, 23.67, 27.25, 24.75, 29.44, 20.53, 28.77, 23.73, 27.68, 26.83, 29.56, 28.71, 31.30, 25.08, 29.75, 26.71, 48.19, 27.01, 37.43, 24.68, 30.18, 24.18, 25.49, 31.04, 28.31, 27.31, 24.46, 37.43, 26.28, 13.45, 30.58, 31.72, };
    ap_uint<3> testtypes [544] = {3, 2, 3, 3, 2, 2, 2, 3, 2, 3, 3, 2, 3, 2, 3, 3, 2, 3, 3, 3, 2, 2, 3, 2, 2, 3, 2, 2, 3, 3, 3, 3, 5, 4, 4, 2, 4, 2, 5, 4, 5, 3, 2, 5, 4, 2, 4, 3, 3, 5, 4, 2, 3, 5, 3, 5, 5, 4, 4, 3, 5, 2, 5, 5, 5, 4, 2, 5, 4, 4, 2, 4, 3, 2, 3, 4, 3, 5, 4, 2, 3, 2, 4, 5, 4, 3, 3, 3, 5, 4, 5, 5, 4, 4, 2, 3, 2, 5, 4, 3, 2, 3, 5, 5, 3, 2, 3, 4, 5, 2, 5, 5, 5, 4, 2, 5, 2, 2, 4, 2, 5, 5, 4, 5, 5, 5, 5, 4, 2, 4, 4, 5, 4, 4, 5, 2, 3, 3, 5, 2, 5, 3, 4, 4, 4, 5, 5, 4, 3, 4, 4, 2, 4, 3, 3, 3, 5, 3, 3, 2, 5, 5, 5, 2, 4, 5, 5, 4, 5, 4, 3, 2, 5, 4, 3, 5, 2, 4, 2, 2, 2, 2, 2, 3, 5, 2, 2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 2, 2, 2, 4, 2, 4, 4, 4, 4, 3, 3, 2, 2, 3, 3, 5, 2, 3, 5, 3, 3, 4, 3, 3, 3, 2, 3, 5, 5, 5, 5, 5, 3, 3, 3, 2, 5, 4, 2, 3, 4, 2, 5, 4, 3, 3, 5, 3, 3, 3, 5, 2, 5, 3, 3, 5, 2, 3, 3, 5, 3, 3, 4, 4, 5, 3, 3, 5, 2, 2, 5, 4, 3, 4, 3, 5, 5, 5, 2, 5, 3, 5, 5, 5, 3, 3, 2, 2, 3, 5, 5, 2, 2, 2, 3, 4, 4, 3, 4, 5, 3, 2, 3, 2, 5, 4, 4, 5, 2, 5, 4, 5, 5, 5, 4, 4, 3, 3, 5, 2, 2, 4, 4, 2, 2, 2, 2, 5, 2, 2, 3, 4, 2, 2, 2, 3, 2, 2, 3, 4, 4, 4, 2, 3, 4, 5, 4, 4, 3, 5, 4, 5, 3, 5, 2, 3, 3, 3, 2, 5, 2, 5, 3, 3, 5, 3, 4, 3, 4, 3, 2, 3, 2, 5, 2, 4, 3, 2, 3, 5, 5, 5, 2, 4, 3, 3, 5, 2, 2, 3, 3, 2, 5, 2, 5, 3, 4, 2, 5, 3, 5, 2, 5, 4, 3, 3, 5, 4, 3, 3, 2, 2, 2, 2, 4, 3, 5, 3, 3, 2, 5, 5, 5, 3, 4, 3, 2, 2, 2, 4, 3, 4, 4, 3, 5, 3, 3, 4, 5, 4, 4, 3, 4, 4, 3, 3, 4, 2, 5, 4, 2, 2, 2, 3, 3, 3, 3, 2, 5, 2, 5, 3, 4, 5, 5, 3, 5, 2, 2, 4, 5, 2, 2, 4, 3, 3, 2, 3, 5, 2, 4, 5, 5, 5, 4, 3, 2, 5, 3, 2, 2, 4, 3, 5, 3, 4, 2, 3, 2, 2, 4, 2, 3, 4, 2, 2, 4, 3, 5, 4, 5, 3, 4, 3, 5, 3, 4, 5, 3, 4, 5, 2, 2, 3, 2, 4, 5, 3, 4, 5, 4, 3, 4, 3, 5, 3, 3, 4, 5, 5, 2, 3, 5, 2, 2, 5, 5, };
    ap_uint<8> testsizes [544] = {93, 131, 217, 21, 85, 20, 63, 61, 233, 9, 204, 22, 217, 17, 163, 23, 92, 21, 233, 171, 233, 98, 60, 168, 233, 82, 6, 21, 140, 129, 1, 233, 171, 21, 92, 178, 22, 92, 163, 178, 9, 177, 80, 60, 63, 42, 92, 55, 62, 233, 233, 35, 233, 21, 222, 129, 21, 168, 42, 12, 140, 133, 55, 233, 233, 35, 30, 23, 133, 131, 13, 80, 19, 112, 59, 98, 208, 82, 20, 112, 89, 233, 30, 217, 233, 106, 24, 18, 106, 233, 18, 24, 17, 233, 20, 125, 181, 125, 112, 21, 69, 4, 1, 4, 4, 39, 195, 69, 21, 127, 195, 59, 62, 6, 20, 204, 240, 85, 181, 14, 208, 177, 20, 89, 222, 4, 217, 85, 29, 85, 127, 12, 14, 39, 61, 70, 137, 136, 136, 124, 93, 121, 70, 29, 240, 137, 19, 124, 76, 112, 13, 88, 88, 148, 129, 7, 76, 35, 80, 17, 148, 129, 80, 104, 20, 7, 121, 17, 35, 104, 27, 233, 27, 233, 5, 5, 231, 231, 178, 233, 154, 55, 36, 66, 66, 113, 44, 36, 233, 178, 44, 113, 55, 44, 154, 44, 54, 88, 4, 12, 12, 31, 31, 4, 88, 54, 137, 68, 233, 11, 233, 40, 68, 146, 150, 150, 38, 19, 233, 53, 182, 225, 144, 8, 182, 40, 137, 225, 233, 106, 7, 121, 82, 7, 146, 11, 147, 82, 159, 19, 11, 12, 64, 53, 77, 103, 38, 12, 147, 8, 29, 8, 77, 75, 101, 67, 29, 42, 7, 159, 11, 121, 27, 70, 42, 45, 103, 103, 75, 2, 103, 24, 101, 106, 2, 53, 70, 165, 147, 38, 67, 235, 22, 5, 22, 143, 27, 165, 239, 46, 74, 223, 5, 239, 2, 22, 223, 77, 71, 1, 110, 7, 45, 147, 143, 74, 38, 74, 235, 24, 2, 53, 71, 57, 129, 77, 125, 63, 110, 125, 77, 154, 32, 33, 129, 6, 88, 138, 32, 5, 175, 14, 180, 125, 12, 8, 175, 46, 5, 29, 13, 125, 57, 154, 29, 96, 8, 12, 138, 25, 96, 123, 73, 111, 46, 150, 73, 61, 180, 119, 17, 25, 4, 74, 84, 6, 60, 16, 21, 79, 4, 10, 63, 15, 9, 28, 22, 46, 15, 104, 77, 233, 23, 21, 194, 46, 233, 9, 39, 84, 21, 233, 9, 39, 68, 9, 144, 1, 12, 23, 12, 39, 89, 144, 194, 17, 172, 169, 4, 55, 8, 21, 75, 119, 96, 46, 175, 39, 172, 64, 44, 144, 29, 52, 36, 60, 10, 149, 61, 175, 131, 75, 56, 3, 46, 17, 16, 104, 134, 14, 123, 9, 233, 33, 77, 9, 52, 45, 30, 32, 82, 1, 135, 42, 110, 134, 46, 233, 20, 30, 60, 3, 233, 46, 51, 23, 9, 42, 163, 93, 93, 233, 13, 62, 17, 13, 63, 45, 131, 20, 9, 79, 49, 184, 233, 233, 6, 147, 6, 30, 13, 194, 23, 197, 127, 43, 13, 197, 233, 126, 150, 45, 30, 110, 2, 28, 51, 127, 141, 32, 27, 135, 85, 13, 82, 77, 36, 149, 17, 52, 7, 17, 46, 233, 233, 4, 96, 88, 186, 163, 132, 85, 36, 86, 77, 44, 89, 18, 89, 132, 30, 99, 1, 111, };
    ap_uint<32> testids [544] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 20, 28, 17, 36, 12, 38, 15, 36, 10, 42, 43, 23, 7, 46, 38, 48, 49, 19, 9, 52, 53, 4, 55, 30, 18, 24, 46, 60, 29, 62, 48, 53, 32, 52, 67, 16, 62, 2, 71, 43, 73, 74, 75, 22, 77, 26, 6, 80, 81, 82, 67, 13, 21, 86, 87, 88, 86, 82, 88, 87, 14, 25, 95, 96, 97, 96, 74, 100, 101, 102, 31, 102, 105, 106, 107, 101, 100, 110, 107, 75, 49, 27, 115, 11, 117, 118, 97, 120, 77, 42, 115, 81, 55, 105, 3, 5, 129, 118, 110, 60, 120, 106, 8, 136, 137, 138, 138, 140, 1, 142, 136, 129, 117, 137, 73, 140, 149, 80, 71, 152, 152, 154, 155, 156, 149, 158, 159, 160, 154, 155, 159, 164, 95, 156, 142, 160, 158, 164, 171, 172, 171, 172, 175, 175, 178, 178, 180, 181, 182, 183, 184, 185, 185, 187, 188, 184, 181, 180, 188, 187, 183, 195, 182, 195, 199, 200, 201, 202, 202, 204, 204, 201, 200, 199, 210, 211, 212, 213, 214, 215, 211, 217, 218, 218, 220, 221, 212, 223, 224, 225, 226, 227, 224, 215, 210, 225, 214, 233, 234, 235, 236, 234, 217, 239, 240, 236, 242, 221, 239, 245, 246, 223, 248, 249, 250, 245, 252, 227, 254, 255, 248, 257, 258, 259, 254, 261, 262, 242, 213, 235, 266, 267, 261, 269, 270, 249, 257, 273, 270, 275, 258, 233, 273, 279, 267, 281, 240, 250, 259, 285, 286, 287, 288, 289, 266, 281, 292, 293, 294, 295, 287, 292, 298, 288, 295, 301, 302, 303, 304, 262, 269, 252, 289, 309, 220, 294, 285, 275, 298, 279, 302, 317, 318, 301, 320, 321, 304, 320, 324, 325, 326, 327, 318, 329, 330, 331, 326, 333, 334, 335, 336, 337, 338, 339, 334, 293, 333, 343, 344, 337, 317, 325, 343, 349, 255, 338, 331, 353, 349, 355, 356, 357, 358, 359, 356, 361, 336, 363, 364, 353, 366, 309, 368, 329, 370, 371, 372, 373, 366, 375, 321, 377, 378, 379, 286, 358, 377, 383, 324, 385, 386, 372, 388, 389, 390, 391, 392, 368, 394, 390, 396, 392, 398, 396, 400, 303, 402, 386, 402, 405, 406, 400, 388, 409, 410, 411, 412, 413, 414, 394, 416, 363, 418, 419, 420, 405, 410, 246, 424, 226, 426, 427, 428, 429, 375, 431, 361, 420, 434, 416, 436, 437, 389, 409, 371, 383, 442, 335, 355, 445, 446, 327, 448, 391, 427, 451, 452, 453, 454, 455, 456, 457, 458, 442, 460, 385, 462, 452, 370, 437, 466, 419, 468, 469, 378, 457, 472, 473, 473, 475, 476, 477, 478, 476, 480, 451, 434, 462, 445, 373, 486, 487, 466, 489, 490, 491, 490, 493, 344, 495, 469, 497, 498, 499, 500, 497, 502, 503, 359, 505, 506, 458, 508, 379, 468, 498, 512, 453, 514, 456, 516, 500, 454, 519, 428, 431, 522, 523, 524, 525, 460, 446, 528, 412, 418, 330, 532, 472, 534, 516, 536, 537, 448, 424, 406, 541, 542, 534, 544, 545, 455, 357, };
    
	ap_uint<32> expected_top_bid_ids[20] = {
    1, 
    1, 
    3,
    3,
    3,
    3,
    3,
    8,
    8,
    10,
    10,
    10,
    10,
    10,
    15,
    15,
    15,
    18,
    18,
    18  
};

   ap_uint<32> expected_top_ask_ids[20] = {
    0, 
    2, 
    2, 
    2,
    5,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6  
};


    unsigned int correct_predictions = 0;
    double total_latency = 0.0;

    for (unsigned int i = 0; i < 20; i++) {
        // Set up the test data for each test case
        test.price = testprices[i];
        test.size = testsizes[i];
        test.orderID = testids[i];
        test.direction = testtypes[i];
        test_stream.write(test);
        test_time.write(t);
        test_meta.write(temp_meta);

        auto start_time = chrono::high_resolution_clock::now();

        // Call the order_book function
        order_book(test_stream, test_time, test_meta, top_bid_stream, top_ask_stream, outgoing_time, outgoing_meta, top_bid_id, top_ask_id);

        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double, std::micro> latency = end_time - start_time;
        total_latency += latency.count();

        // Initialize variables to track if data is read
        bool read_top_bid = false, read_top_ask = false;

        // Read from the streams if they're not empty and track the read status
        if (!top_bid_stream.empty()) {
            top_bid = top_bid_stream.read();
            read_top_bid = true;
        }

        if (!top_ask_stream.empty()) {
            top_ask = top_ask_stream.read();
            read_top_ask = true;
        }

        // Only perform checks if data was read
        bool correct_bid = read_top_bid && (top_bid_id == expected_top_bid_ids[i]);
        bool correct_ask = read_top_ask && (top_ask_id == expected_top_ask_ids[i]);
        
        if (correct_bid && correct_ask) {
            correct_predictions++;
        }

        while (!test_stream.empty()) { test_stream.read(); }
        while (!test_time.empty()) { test_time.read(); }
        while (!test_meta.empty()) { test_meta.read(); }
        while (!top_bid_stream.empty()) { top_bid_stream.read(); }
        while (!top_ask_stream.empty()) { top_ask_stream.read(); }
        while (!outgoing_time.empty()) { outgoing_time.read(); }
        while (!outgoing_meta.empty()) { outgoing_meta.read(); }

        // Display the test case result, including the test values and whether data was read from each stream
        std::cout << "Test Case " << i + 1 << ":\n";
        std::cout << "Price: " << test.price << ", Size: " << test.size << ", Order ID: " << test.orderID << ", Direction: " << directionToString(test.direction) << "\n";
        std::cout << "Expected Top Bid ID: " << expected_top_bid_ids[i] << ", Actual: " << (read_top_bid ? std::to_string(top_bid_id) : "No Data") << "\n";
        std::cout << "Expected Top Ask ID: " << expected_top_ask_ids[i] << ", Actual: " << (read_top_ask ? std::to_string(top_ask_id) : "No Data") << "\n";
        std::cout << "Latency: " << latency.count() << " microseconds\n";
        std::cout << "Result: " << ((correct_bid && correct_ask) ? "Correct" : "Incorrect") << "\n\n";
    }

    // Display correction rate and total latency
    double correction_rate = static_cast<double>(correct_predictions) / 20 * 100;
    std::cout << "Correction Rate: " << correction_rate << "%\n";
    return 0;
}