
#include <cmath>
#include <gtest/gtest.h>
#include <ruuvi/ruuvi.hpp>

std::vector<uint8_t> to_raw_data(std::string const& s) {
    assert(s.size() % 2 == 0);
    std::vector<uint8_t> r;
    for (size_t i = 0; i < s.size(); i += 2) {
        auto ch = s.substr(i, 2);
        r.push_back(std::stoi(ch, nullptr, 16));
    }
    return r;
}

ble::BlePacket default_packet() {
    ble::BlePacket r;
    r.mac             = "CB:B8:33:4C:88:4F";
    r.manufacturer_id = 0x0499;
    r.device_name     = "";
    r.signal_strength = 40;
    return r;
}

ble::BlePacket default_packet5() {
    auto r = default_packet();
    r.manufacturer_data =
        to_raw_data("0512FC5394C37C0004FFFC040CAC364200CDCBB8334C884F");
    return r;
}
ble::BlePacket max_packet5() {
    auto r = default_packet();
    r.manufacturer_data =
        to_raw_data("057FFF9C40FFFE7FFF7FFF7FFFFFDEFEFFFECBB8334C884F");
    return r;
}
ble::BlePacket invalid_packet5() {
    auto r = default_packet();
    r.manufacturer_data =
        to_raw_data("058000FFFFFFFF800080008000FFFFFFFFFFFFFFFFFFFFFF");
    return r;
}
ble::BlePacket default_packet3() {
    auto r              = default_packet();
    r.manufacturer_data = to_raw_data("03291A1ECE1EFC18F94202CA0B53");
    return r;
}
ble::BlePacket broken_data() {
    auto r              = default_packet();
    r.manufacturer_data = to_raw_data("09291A1ECE1EFC18F94202CA0B53");
    return r;
}

TEST(RuuviDecodeTest, IdentifiesFormat) {
    EXPECT_EQ(ruuvi::identify_format(default_packet5()), 5)
        << "Data format 5 not identified";
    EXPECT_EQ(ruuvi::identify_format(default_packet3()), 3)
        << "Data format 3 not identified";
    auto broken = ruuvi::identify_format(broken_data());
    EXPECT_EQ(broken, ruuvi::unknown_format)
        << "Identified " << broken << ", should be unknown_format ("
        << ruuvi::unknown_format << ")";
    auto type            = default_packet5();
    type.manufacturer_id = 0x0500;
    auto type_v          = ruuvi::identify_format(type);
    EXPECT_EQ(type_v, ruuvi::not_ruuvitag)
        << "Identified " << broken << ", should be not_ruuvitag ("
        << ruuvi::not_ruuvitag << ")";
}

#define DEF_F_TEST(name, exp)                                                  \
    EXPECT_FLOAT_EQ(data.name, exp)                                            \
        << "Wrong " #name << " " << data.name << " (should be " << exp << ")"
#define DEF_I_TEST(name, exp)                                                  \
    EXPECT_EQ(data.name, exp)                                                  \
        << "Wrong " #name << " " << data.name << " (should be " << exp << ")"
#define DEF_NAN_TEST(name)                                                     \
    EXPECT_TRUE(std::isnan(data.name)) << #name " should be nan";

TEST(RuuviDecodeTest, DecodeCorrect5) {
    auto data = ruuvi::convert_data_format_5(default_packet5());

    DEF_F_TEST(temperature, 24.3f);
    DEF_F_TEST(humidity, 53.49f);
    DEF_F_TEST(acceleration[0], 0.004f);
    DEF_F_TEST(acceleration[1], -0.004f);
    DEF_F_TEST(acceleration[2], 1.036f);
    DEF_F_TEST(battery_voltage, 2.977f);

    DEF_I_TEST(pressure, 100044);
    DEF_I_TEST(measurement_sequence, 205);
    DEF_I_TEST(tx_power, 4);
    DEF_I_TEST(movement_counter, 66);
    DEF_I_TEST(signal_strength, 40);
    DEF_I_TEST(mac, default_packet().mac);
    DEF_I_TEST(contains_errors, false) << " : " << data.error_msg;
}

TEST(RuuviDecodeTest, DecodeMax5) {
    auto data = ruuvi::convert_data_format_5(max_packet5());

    DEF_F_TEST(temperature, 163.835f);
    DEF_F_TEST(humidity, 100.f);
    DEF_F_TEST(acceleration[0], 32.767f);
    DEF_F_TEST(acceleration[1], 32.767f);
    DEF_F_TEST(acceleration[2], 32.767f);
    DEF_F_TEST(battery_voltage, 3.646f);

    DEF_I_TEST(pressure, 115534);
    DEF_I_TEST(measurement_sequence, 65534);
    DEF_I_TEST(tx_power, 20);
    DEF_I_TEST(movement_counter, 254);
    DEF_I_TEST(signal_strength, 40);
    DEF_I_TEST(mac, default_packet().mac);
    DEF_I_TEST(contains_errors, false) << " : " << data.error_msg;
}

TEST(RuuviDecodeTest, DecodeInvalid5) {
    EXPECT_THROW(ruuvi::convert_data_format_5(invalid_packet5(), true),
                 std::runtime_error)
        << "convert_data_format_5 didn't throw";
    EXPECT_NO_THROW(ruuvi::convert_data_format_5(invalid_packet5()))
        << "convert_data_format_5 did throw";
    auto data = ruuvi::convert_data_format_5(invalid_packet5());

    DEF_NAN_TEST(temperature);
    DEF_NAN_TEST(humidity);
    DEF_NAN_TEST(acceleration[0]);
    DEF_NAN_TEST(acceleration[1]);
    DEF_NAN_TEST(acceleration[2]);
    DEF_NAN_TEST(battery_voltage);

#define TYPEOF(x) decltype(data.x)

    DEF_I_TEST(pressure, std::numeric_limits<TYPEOF(pressure)>::max());
    DEF_I_TEST(measurement_sequence,
               std::numeric_limits<TYPEOF(measurement_sequence)>::max());
    DEF_I_TEST(tx_power, INT8_MIN);
    DEF_I_TEST(movement_counter,
               std::numeric_limits<TYPEOF(movement_counter)>::max());
    //    DEF_I_TEST(signal_strength,
    //    std::numeric_limits<TYPEOF(signal_strength)>::min());
    DEF_I_TEST(mac, "");
    DEF_I_TEST(contains_errors, true) << "Invalid data contains no errors";
}

TEST(RuuviDecodeTest, DecodeTotalAcceleration) {
    auto data = ruuvi::convert_data_format_5(default_packet5());
    DEF_F_TEST(acceleration_total(),
               std::hypot(data.acceleration[0], data.acceleration[1],
                          data.acceleration[2]));
}
