#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <vector>
#include <stdlib.h>
#include "fast.h"

using namespace std;

std::string getOrderType(int type) {
        switch(type) {
        case 0: return "Market Sell";
        case 1: return "Market Buy";
        case 2: return "Limited Sell";
        case 3: return "Limited Buy";
        default: return "Unknown Type";
        }
    }

int main(int argc, char *argv[])
{

    // Arguments are: executable, input data, output data
    if (argc != 3)
    {
        return 1;
    }

    ifstream ifs;
    ifs.open(argv[1], ifstream::in);
    if (!ifs)
    {
        cerr << "Failed to open file: " << argv[1] << endl;
        return 1;
    }

    ifstream ofs;
    ofs.open(argv[2], ifstream::in);
    if (!ofs)
    {
        cerr << "Failed to open file: " << argv[2] << endl;
        return 1;
    }

    // Receive path stream parameter
    stream<axiWord> lbRxDataIn;
    stream<metadata> lbRxMetadataIn;
    stream<uint16> lbRequestPortOpenOut;
    stream<bool> lbPortOpenReplyIn;
    stream<metadata> metadata_to_book;
    stream<uint64> tagsIn;
    stream<uint64> time_to_book;
    stream<order> order_to_book;

    // Transmit path stream parameters
    stream<metadata> metadata_from_book;
    stream<axiWord> lbTxDataOut;
    stream<metadata> lbTxMetadataOut;
    stream<uint16> lbTxLengthOut;
    stream<uint64> time_from_book;
    stream<uint64> tagsOut;
    stream<order> order_from_book;

    // Load FAST message
    unsigned num_test_cases;
    ifs >> num_test_cases;
    for (unsigned j = 0; j < num_test_cases; j++)
    {
        ap_uint<64> first_packet = 0;
        ap_uint<64> second_packet = 0;
        ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = { 0. };
        order decoded_message = { 0, 0, 0, 0 };

        axiWord first_packet_buff;
        axiWord second_packet_buff;
        metadata metadata_buff;
        ap_uint<64> time_buff;
        ap_uint<16> lbRequestPortOpenOut_buff;
        ap_uint<16> length_buff;

        for (unsigned i = 0; i < MESSAGE_BUFF_SIZE; i++)
        {
            ifs >> encoded_message[i];
        }

        for (int i = NUM_BYTES_IN_PACKET - 1; i >= 0; i--)
        {
            first_packet = (first_packet << BYTE) | encoded_message[i];
            second_packet = (second_packet << BYTE)
                    | encoded_message[i + NUM_BYTES_IN_PACKET];
        }

        while (true)
        {
            if (!lbRxDataIn.full() && !lbRxMetadataIn.full() && !tagsIn.full())
            {
                if (j == 0)
                {
                    lbPortOpenReplyIn.write(true);
                }
                first_packet_buff.data = first_packet;
                first_packet_buff.keep = 0xFF;
                first_packet_buff.last = 0;
                lbRxDataIn.write(first_packet_buff);
                lbRxMetadataIn.write(metadata_buff);
                tagsIn.write(time_buff);
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            }
        }

        while (true)
        {
            if (!lbRxDataIn.full())
            {
                second_packet_buff.data = second_packet;
                second_packet_buff.keep = 0xFF;
                second_packet_buff.last = 1;
                lbRxDataIn.write(second_packet_buff);
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            }
        }

        while (true)
        {
            if (!metadata_to_book.empty() && !time_to_book.empty()
                    && !order_to_book.empty())
            {
                if (!lbRequestPortOpenOut.empty() && j == 0)
                {
                    lbRequestPortOpenOut_buff = lbRequestPortOpenOut.read();
                    cout << "Port: " << lbRequestPortOpenOut_buff << endl;
                }
                metadata_buff = metadata_to_book.read();
                time_buff = time_to_book.read();
                decoded_message = order_to_book.read();
                break;
            }
            else
            {
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
            }
        }

        cout << "Success: Order received! Encoding test starts!" << endl;

        ap_fixed<16, 8> price;
        ap_uint<8> size;
        ap_uint<32> orderID;
        ap_uint<2> type;
        ofs >> price;
        ofs >> size;
        ofs >> orderID;
        ofs >> type;

        std::cout << "Order:" << j << std::endl;
        std::cout << std::left << std::setw(15) << "Size:" << decoded_message.size << std::endl;
        std::cout << std::left << std::setw(15) << "Instrument ID:" << decoded_message.orderID << std::endl;
        std::cout << std::left << std::setw(15) << "Order Type:" << getOrderType(decoded_message.type) << std::endl;
        std::cout << std::endl;


        //////////////
        // encoding //
        //////////////

        auto start = std::chrono::high_resolution_clock::now();

        // Send order
        while (true)
        {
            if (!metadata_from_book.full() && !time_from_book.full()
                    && !order_from_book.full())
            {
                metadata_from_book.write(metadata_buff);
                time_from_book.write(time_buff);
                order_from_book.write(decoded_message);
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            }
        }
          
        auto end = std::chrono::high_resolution_clock::now();
        auto encoding_latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // Receive first packet
        while (true)
        {
            if (!lbTxMetadataOut.empty() && !lbTxDataOut.empty()
                    && !tagsOut.empty())
            {
                metadata_buff = lbTxMetadataOut.read();
                first_packet_buff = lbTxDataOut.read();
                time_buff = tagsOut.read();
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            } else {
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
            }
        }

        // Receive second packet
        cout << "Success: all encoded packets received for the order! Encoding Latency: " << encoding_latency << " nanoseconds. Decoding test starts! Order message breakdown:" << endl;
        while (true)
        {
            if (!lbTxDataOut.empty() && !lbTxLengthOut.empty())
            {
                second_packet_buff = lbTxDataOut.read();
                length_buff = lbTxLengthOut.read();
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            } else {
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
            }
        }

        if (first_packet_buff.last != 0)
        {
            cout << "ERROR first keep: " << first_packet_buff.keep << " != 0"
                    << endl;
            return 1;

        }
        if (second_packet_buff.last != 1)
        {
            cout << "ERROR second keep: " << second_packet_buff.keep << " != 1"
                    << endl;
            return 1;

        }

        for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++)
        {
            encoded_message[i] = first_packet_buff.data >> (BYTE * i);
            encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet_buff.data
                    >> (BYTE * i);
        }

        cout << "Encoded Message: ";
        for (unsigned i = 0; i < MESSAGE_BUFF_SIZE; i++) {
        cout << "[" << i << "]: 0x" << std::hex << (int)encoded_message[i] << " ";
        }
        cout << std::dec << endl;

        //////////////
        // decoding //
        //////////////

        auto startAnother = std::chrono::high_resolution_clock::now();

        while (true)
        {
            if (!lbRxDataIn.full() && !lbRxMetadataIn.full() && !tagsIn.full())
            {
                lbRxDataIn.write(first_packet_buff);
                lbRxMetadataIn.write(metadata_buff);
                tagsIn.write(time_buff);
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            }
        }

        while (true)
        {
            if (!lbRxDataIn.full())
            {
                lbRxDataIn.write(second_packet_buff);
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
                break;
            }
        }

        while (true)
        {
            if (!metadata_to_book.empty() && !time_to_book.empty()
                    && !order_to_book.empty())
            {
                metadata_buff = metadata_to_book.read();
                time_buff = time_to_book.read();
                decoded_message = order_to_book.read();
                break;
            }
            else
            {
                fast_protocol(lbRxDataIn,
                              lbRxMetadataIn,
                              lbRequestPortOpenOut,
                              lbPortOpenReplyIn,
                              lbTxDataOut,
                              lbTxMetadataOut,
                              lbTxLengthOut,
                              tagsIn,
                              tagsOut,
                              metadata_to_book,
                              metadata_from_book,
                              time_to_book,
                              time_from_book,
                              order_to_book,
                              order_from_book);
            }
        }
        auto endAnother = std::chrono::high_resolution_clock::now();
        auto Delatency = std::chrono::duration_cast<std::chrono::nanoseconds>(endAnother - startAnother).count();

        cout << "Decoding Success! Decoded Message:\n";
        std::cout << "Order:" << j << std::endl;
        std::cout << std::left << std::setw(15) << "Size:" << decoded_message.size << std::endl;
        std::cout << std::left << std::setw(15) << "Instrument ID:" << decoded_message.orderID << std::endl;
        std::cout << std::left << std::setw(15) << "Order Type:" << getOrderType(decoded_message.type) << std::endl;
        std::cout << "Decoding Latency: " << Delatency << " nanoseconds\n\n"<< std::endl;
       
        

        if (decoded_message.price != 1)
        {
            cout << "ERROR price: " << decoded_message.price << " != 1"
                    << endl;
            return 1;

        }
        if (decoded_message.size != size)
        {
            cout << "ERROR size: " << decoded_message.size << " != " << size
                    << endl;
            return 1;
        }
        if (decoded_message.orderID != orderID)
        {
            cout << "ERROR orderID: " << decoded_message.orderID << " != "
                    << orderID << endl;
            return 1;
        }
        if (decoded_message.type != type)
        {
            cout << "ERROR type: " << decoded_message.type << " != " << type
                    << endl;
            return 1;
        }
    }

    ifs.close();
    ofs.close();

    return 0;
}

