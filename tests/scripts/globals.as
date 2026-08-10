string g_out;

// line comment syntax
/* block comment syntax */

enum Direction
{
    North,
    East = 3,
    South,
    Scaled = East * 10
}

typedef int32 Score;
funcdef int BinaryOp(int, int);

interface IReadable
{
    int read() const;
}

namespace Alpha
{
    int seed = 5;

    int twice(int value)
    {
        return value * 2;
    }

    namespace Nested
    {
        int value = 9;
    }
}

namespace Alpha::More
{
    int value = 11;
}

int globalValue = 7;
const uint globalMask = 0x10;

int globalProperty
{
    get { return globalValue; }
    set { globalValue = value; }
}

class Reading : IReadable
{
    int value;

    Reading(int v)
    {
        value = v;
    }

    int read() const
    {
        return value;
    }
}

int addValues(int left, int right)
{
    return left + right;
}

;

int main()
{
    Direction direction = South;
    Score score = 12;
    BinaryOp@ op = @addValues;
    IReadable@ readable = Reading(6);

    globalProperty = 13;
    g_out += itos(int(direction)) + "\n";
    g_out += itos(score) + "\n";
    g_out += itos(op(2, 8)) + "\n";
    g_out += itos(readable.read()) + "\n";
    g_out += itos(Alpha::seed + Alpha::Nested::value + Alpha::More::value) + "\n";
    g_out += itos(::globalValue + int(globalMask)) + "\n";
    g_out += itos(Alpha::twice(4)) + "\n";
    return 0;
}
