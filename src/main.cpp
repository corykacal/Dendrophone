#include <gpiod.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <bitset>
#include <iomanip>

// Debug print flags - set to true for what you want to see
struct DebugFlags {
    bool show_pin_states = true;       // Show raw pin readings
    bool show_bck_edges = true;        // Show BCK edge detections
    bool show_bit_collection = true;    // Show bits as they're collected
    bool show_data_assembly = true;    // Show data as it's being built
    bool show_final_value = true;      // Show final 24-bit values
} debug;

void print_pin_states(bool bck, bool lrck, bool din) {
    static int count = 0;
    if (count++ % 1000 == 0) {  // Only print every 1000th reading to avoid flooding
        std::cout << "Pins - BCK: " << bck << " LRCK: " << lrck << " DIN: " << din << std::endl;
    }
}

void print_bit_collection(int bit_count, bool din) {
    std::cout << "Bit " << std::setw(2) << bit_count << ": " << din << std::endl;
}

void print_data_assembly(uint32_t data, int bit_count) {
    std::cout << "Data after " << std::setw(2) << bit_count << " bits: 0x"
              << std::hex << std::setw(6) << std::setfill('0') << data
              << " (binary: " << std::bitset<24>(data) << ")"
              << std::dec << std::endl;
}

int main() {
    const std::string chip_name = "/dev/gpiochip0";
    const unsigned int DIN_PIN = 20;
    const unsigned int BCK_PIN = 18;
    const unsigned int LRCK_PIN = 19;

    try {
        // Print startup info
        std::cout << "Starting PCM1802 GPIO reader with debug output" << std::endl;
        std::cout << "Using pins - DIN: " << DIN_PIN << " BCK: " << BCK_PIN << " LRCK: " << LRCK_PIN << std::endl;

        gpiod::chip chip(chip_name);
        auto din_line = chip.get_line(DIN_PIN);
        auto bck_line = chip.get_line(BCK_PIN);
        auto lrck_line = chip.get_line(LRCK_PIN);

        std::cout << "GPIO chip opened successfully" << std::endl;

        din_line.request({"pcm1802_reader", gpiod::line_request::DIRECTION_INPUT});
        bck_line.request({"pcm1802_reader", gpiod::line_request::DIRECTION_INPUT});
        lrck_line.request({"pcm1802_reader", gpiod::line_request::DIRECTION_INPUT});

        std::cout << "GPIO lines requested successfully" << std::endl;
        std::cout << "Starting main loop..." << std::endl;

        uint32_t data = 0;
        int bit_count = 0;
        bool last_bck = false;
        int sample_count = 0;
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            bool current_bck = bck_line.get_value();
            bool current_lrck = lrck_line.get_value();
            bool current_din = din_line.get_value();

            // Print pin states
            if (debug.show_pin_states) {
                print_pin_states(current_bck, current_lrck, current_din);
            }

            // Detect BCK rising edge
            if (current_bck && !last_bck) {
                if (debug.show_bck_edges) {
                    std::cout << "BCK Rising Edge Detected!" << std::endl;
                }

                // Collect bit
                data = (data << 1) | current_din;
                bit_count++;

                if (debug.show_bit_collection) {
                    print_bit_collection(bit_count, current_din);
                }

                if (debug.show_data_assembly) {
                    print_data_assembly(data, bit_count);
                }

                // After collecting 24 bits
                if (bit_count == 24) {
                    sample_count++;
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>
                                 (current_time - start_time).count();

                    if (debug.show_final_value) {
                        std::cout << "\n=== COMPLETE SAMPLE ===" << std::endl;
                        std::cout << "Channel: " << (current_lrck ? "Right" : "Left") << std::endl;
                        std::cout << "Value (dec): " << data << std::endl;
                        std::cout << "Value (hex): 0x" << std::hex << data << std::dec << std::endl;
                        std::cout << "Value (bin): " << std::bitset<24>(data) << std::endl;
                        if (elapsed > 0) {
                            std::cout << "Samples per second: " << sample_count / elapsed << std::endl;
                        }
                        std::cout << "=====================\n" << std::endl;
                    }

                    data = 0;
                    bit_count = 0;
                }
            }

            last_bck = current_bck;

            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}