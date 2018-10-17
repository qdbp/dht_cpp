#include "dht.h"

namespace cht {

bool operator==(const Nih &x, const Nih &y) {
    return x.checksum == y.checksum;
}
bool operator!=(const Nih &x, const Nih &y) {
    return x.checksum != y.checksum;
}

} // namespace cht
