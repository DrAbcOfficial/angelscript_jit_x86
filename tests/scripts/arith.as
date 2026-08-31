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

    return 0;
}
