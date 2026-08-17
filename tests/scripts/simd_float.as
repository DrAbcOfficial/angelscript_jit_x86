string g_out;

int main()
{
    float a0 = 1.0f;
    float a1 = 2.0f;
    float a2 = 3.0f;
    float a3 = 4.0f;
    float a4 = 5.0f;
    float a5 = 6.0f;
    float a6 = 7.0f;
    float a7 = 8.0f;
    float b0 = 0.5f;
    float b1 = 1.0f;
    float b2 = 1.5f;
    float b3 = 2.0f;
    float b4 = 2.5f;
    float b5 = 3.0f;
    float b6 = 3.5f;
    float b7 = 4.0f;

    a0 += b0;
    a1 += b1;
    a2 += b2;
    a3 += b3;
    a4 += b4;
    a5 += b5;
    a6 += b6;
    a7 += b7;

    a0 *= b0;
    a1 *= b1;
    a2 *= b2;
    a3 *= b3;
    a4 *= b4;
    a5 *= b5;
    a6 *= b6;
    a7 *= b7;

    a0 -= b0;
    a1 -= b1;
    a2 -= b2;
    a3 -= b3;
    a4 -= b4;
    a5 -= b5;
    a6 -= b6;
    a7 -= b7;

    float total = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7;
    g_out += ftos(total) + "\n";
    return int(total);
}
