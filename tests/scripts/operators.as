string g_out;

class Number
{
    int value;
    int first;
    int second;

    Number(int input = 0)
    {
        value = input;
    }

    Number opNeg() const
    {
        return Number(-value);
    }

    Number opCom() const
    {
        return Number(~value);
    }

    Number opPreInc()
    {
        value++;
        return Number(value);
    }

    Number opPostInc()
    {
        Number previous(value);
        value++;
        return previous;
    }

    Number opPreDec()
    {
        value--;
        return Number(value);
    }

    Number opPostDec()
    {
        Number previous(value);
        value--;
        return previous;
    }

    bool opEquals(const Number&in other) const
    {
        return value == other.value;
    }

    int opCmp(const Number&in other) const
    {
        if (value < other.value) return -1;
        if (value > other.value) return 1;
        return 0;
    }

    Number@ opAssign(const Number&in other)
    {
        value = other.value;
        first = other.first;
        second = other.second;
        return this;
    }

    Number@ opAddAssign(int input)
    {
        value += input;
        return this;
    }

    Number opAdd(const Number&in other) const
    {
        return Number(value + other.value);
    }

    Number opAdd_r(int left) const
    {
        return Number(left + value);
    }

    Number opPow(int exponent) const
    {
        int result = 1;
        for (int i = 0; i < exponent; i++)
            result *= value;
        return Number(result);
    }

    int& opIndex(int index)
    {
        if (index == 0)
            return first;
        return second;
    }

    const int& opIndex(int index) const
    {
        if (index == 0)
            return first;
        return second;
    }

    int& opIndex(int row, int column)
    {
        if (row == column)
            return first;
        return second;
    }

    int opCall(int left, int right) const
    {
        return value + left + right;
    }

    int opConv() const
    {
        return value;
    }
}

int main()
{
    Number number(3);
    Number negative = -number;
    Number complement = ~number;
    Number before = number++;
    Number after = ++number;
    Number beforeDec = number--;
    Number afterDec = --number;
    Number sum = number + Number(4);
    Number reverse = 10 + number;
    Number power = Number(3) ** 3;
    number += 5;
    number[0] = 7;
    number[1] = 8;
    number[0, 1] = 9;

    g_out += itos(negative.value) + "," + itos(complement.value) + "\n";
    g_out += itos(before.value) + "," + itos(after.value) + "," + itos(number.value) + "\n";
    g_out += itos(beforeDec.value) + "," + itos(afterDec.value) + "\n";
    g_out += itos(sum.value) + "," + itos(reverse.value) + "," + itos(power.value) + "\n";
    g_out += number == Number(number.value) ? "equal\n" : "bad\n";
    g_out += number > Number(1) ? "greater\n" : "bad\n";
    g_out += number >= Number(1) && number <= Number(number.value) ? "bounds\n" : "bad\n";
    g_out += itos(number[0] + number[1]) + "\n";
    g_out += itos(number(2, 3)) + "\n";
    g_out += itos(int(number)) + "\n";

    int arithmetic = 8;
    arithmetic += 4;
    arithmetic -= 2;
    arithmetic *= 3;
    arithmetic /= 5;
    arithmetic %= 4;
    arithmetic **= 3;

    int bits = 1;
    bits |= 8;
    bits &= 13;
    bits ^= 3;
    bits <<= 2;
    bits >>= 1;
    uint unsignedBits = 0x80000000;
    unsignedBits >>>= 31;
    uint shifted = 0x80000000 >>> 30;

    bool words = (true and true) && (false or true) && (true xor false);
    bool symbols = true ^^ false;
    g_out += itos(arithmetic) + "\n";
    g_out += itos(+bits) + "," + itos(int(unsignedBits)) + "," + itos(int(shifted)) + "\n";
    g_out += words && symbols ? "logic\n" : "bad\n";
    return 0;
}
