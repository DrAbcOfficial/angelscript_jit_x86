string g_out;

int sumArray(const array<int>&in values)
{
    int total = 0;
    for (uint i = 0; i < values.length(); i++)
        total += values[i];
    return total;
}

int main()
{
    int8 i8 = -8;
    int16 i16 = 160;
    int32 i32 = 3200;
    int64 i64 = 64000;
    int64 carry = 4294967295;
    int64 increment64 = 2;
    carry = carry + increment64;
    uint8 u8 = 8;
    uint16 u16 = 160;
    uint32 u32 = 3200;
    uint64 u64 = 64000;
    uint literals = 0b1010 + 0o10 + 0d10 + 0x10;
    float real32 = 1.25f;
    double real64 = 2.5e1;
    bool flag = true;
    auto inferred = i32 + 7;
    const auto fixedValue = 9;

    string singleQuoted = 'single';
    string multiLine = """
alpha
beta""";

    array<int> values = {1, 2, 3};
    int[] shorthand = {4, 5};
    array<array<int>> matrix = {{6, 7}, {8, 9}};
    array<int>@ valuesHandle = @values;
    valuesHandle[1] = 20;

    g_out += itos(int(i8) + int(i16) + i32 + int(i64)) + "\n";
    g_out += itos(int(u8) + int(u16) + int(u32) + int(u64)) + "\n";
    g_out += dtos(double(carry)) + "\n";
    g_out += itos(int(literals)) + "\n";
    g_out += ftos(real32) + "\n";
    g_out += dtos(real64) + "\n";
    g_out += flag ? "true\n" : "false\n";
    g_out += itos(inferred + fixedValue) + "\n";
    g_out += singleQuoted + "\n";
    g_out += itos(int(multiLine.length())) + "\n";
    g_out += itos(sumArray(values)) + "\n";
    g_out += itos(sumArray(shorthand)) + "\n";
    g_out += itos(matrix[0][1] + matrix[1][0]) + "\n";
    g_out += itos(sumArray({10, 11, 12})) + "\n";
    g_out += itos(sumArray(array<int> = {13, 14})) + "\n";
    return 0;
}
