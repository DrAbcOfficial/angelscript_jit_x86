string g_out;

int main()
{
    int a = 7;
    int b = 3;
    int i = 2147483647;
    uint u = 4000000000;
    float f1 = 1.5f;
    float f2 = 2.25f;
    double d1 = 1.25;
    double d2 = 0.5;

    g_out += itos(a + b) + "\n";
    g_out += itos(a - b) + "\n";
    g_out += itos(a * b) + "\n";
    g_out += itos(a / b) + "\n";
    g_out += itos(a % b) + "\n";
    g_out += itos(-a) + "\n";

    g_out += itos(i + 1) + "\n";
    g_out += itos(i * 2) + "\n";

    g_out += itos(int(u - 1)) + "\n";
    g_out += itos(int(u / 2)) + "\n";
    g_out += itos(int(u % 7)) + "\n";

    g_out += ftos(f1 + f2) + "\n";
    g_out += ftos(f1 * f2) + "\n";
    g_out += ftos(f1 / f2) + "\n";
    g_out += ftos(-f1) + "\n";

    g_out += dtos(d1 + d2) + "\n";
    g_out += dtos(d1 * d2) + "\n";
    g_out += dtos(d1 - d2) + "\n";

    g_out += itos(int(d1 + d2)) + "\n";
    g_out += itos(int(f1 + f2)) + "\n";
    g_out += ftos(float(d1)) + "\n";
    g_out += dtos(double(f1)) + "\n";

    g_out += itos(a & b) + "\n";
    g_out += itos(a | b) + "\n";
    g_out += itos(a ^ b) + "\n";
    g_out += itos(~a) + "\n";
    g_out += itos(a << 2) + "\n";
    g_out += itos(a >> 1) + "\n";
    g_out += itos(-a >> 1) + "\n";

    g_out += itos(int(0x80000000) / -1) + "\n";

    int powered = 9;
    powered **= 2;
    g_out += itos(powered) + "\n";
    g_out += itos(3 ** 4) + "\n";

    int64 wide = 4294967295;
    wide = wide + 2;
    wide = wide - 5;
    wide = wide * 3;
    wide = wide / 2;
    wide = wide % 7;
    g_out += itos(int(wide)) + "\n";

    uint qu = 4000000000;
    g_out += itos(int(qu / 7)) + "\n";
    g_out += itos(int(qu % 7)) + "\n";

    int64 bits = 1234567890123;
    bits = bits << 3;
    bits = bits >> 2;
    bits = bits >>> 1;
    g_out += itos(int(bits & 0x7FFFFFFF)) + "\n";
    g_out += (bits > 100) ? "gt\n" : "le\n";

    int8 n8 = -8;
    int16 n16 = 160;
    uint8 u8v = 200;
    n8++;
    n16--;
    g_out += itos(int(n8) + int(n16) + int(u8v)) + "\n";

    float modf = 17.5f % 4.0f;
    g_out += ftos(modf) + "\n";

    array<int> values = {1, 2, 3};
    g_out += itos(values[0] + values[1] + values[2]) + "\n";

    return 0;
}
