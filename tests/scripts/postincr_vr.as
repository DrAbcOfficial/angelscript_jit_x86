// Regression: post-increment/decrement whose value is consumed.
//
// `x-- > 0` and `arr[i++]` compile to a value-register address producer
// (LDV/LoadThisR) followed by an RDR that copies the old value and an
// INC/DEC that writes through the same address. The value register
// forwarding optimizer must not consider the register dead after just
// the first consumer, otherwise INC/DEC dereferences a stale (null on
// entry) address and crashes.

string g_out;

string char_fn(int n) { return "x"; }
int byte_fn() { return 65; }

string read_string(int len) {
    string ret = "";
    while (len-- > 0) {
        int b = byte_fn();
        if (b == 0) break;
        ret += char_fn(b);
    }
    while (len-- > 0)
        byte_fn();
    return ret;
}

int sum_to(int n) {
    int sum = 0;
    while (n-- > 0)
        sum++;
    return sum;
}

int main() {
    g_out = read_string(4) + "\n";
    g_out += itos(sum_to(10)) + "\n";

    array<int> arr = array<int>(3);
    int i = 1;
    g_out += itos(arr[i++]) + "\n";
    g_out += itos(i) + "\n";
    return 0;
}
