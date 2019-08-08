#ifndef _HEX_DUMP_H_
#define _HEX_DUMP_H_
#include <cctype>
#include <iomanip>
#include <ostream>
template <unsigned RowSize, bool ShowAscii>
struct CustomHexdump {
    CustomHexdump(void* data, unsigned len)
        : data_(static_cast<unsigned char*>(data)), len_(len) {}
    const unsigned char* data_;
    const int len_;
};

template <unsigned RowSize, bool ShowAscii>
std::ostream& operator<<(std::ostream& out,
                         const CustomHexdump<RowSize, ShowAscii>& dump) {
    out.fill('0');
    for (int i = 0; i < dump.len_; i += RowSize) {
        out << "0x" << std::setw(6) << std::hex << i << ": ";
        for (int j = 0; j < RowSize; ++j) {
            if (i + j < dump.len_) {
                out << std::hex << std::setw(2)
                    << static_cast<int>(dump.data_[i + j]) << " ";
            } else {
                out << "   ";
            }
        }

        out << " ";
        if (ShowAscii) {
            for (int j = 0; j < RowSize; ++j) {
                if (i + j < dump.len_) {
                    if (std::isprint(dump.data_[i + j])) {
                        out << static_cast<char>(dump.data_[i + j]);
                    } else {
                        out << ".";
                    }
                }
            }
        }
        out << std::endl;
    }
    return out;
}

typedef CustomHexdump<16, true> Hexdump;
#endif /* _HEX_DUMP_H_ */
