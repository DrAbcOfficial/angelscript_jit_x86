string g_out;

shared enum SharedState
{
    SharedIdle,
    SharedReady = 4
}

shared funcdef int SharedOperation(int);

shared interface ISharedValue
{
    int get() const;
}

shared class SharedValue : ISharedValue
{
    int value;

    SharedValue(int input)
    {
        value = input;
    }

    int get() const
    {
        return value;
    }
}

shared int sharedTwice(int value)
{
    return value * 2;
}

mixin class IncrementMixin
{
    int count;

    void increment()
    {
        count++;
    }
}

class MixedCounter : IncrementMixin
{
    int run(int times)
    {
        for (int i = 0; i < times; i++)
            increment();
        return count;
    }
}

int main()
{
    SharedState state = SharedReady;
    SharedValue value(6);
    ISharedValue@ interfaceHandle = @value;
    SharedOperation@ operation = @sharedTwice;
    MixedCounter mixed;

    g_out += itos(int(state)) + "\n";
    g_out += itos(interfaceHandle.get()) + "\n";
    g_out += itos(operation(7)) + "\n";
    g_out += itos(mixed.run(5)) + "\n";
    return 0;
}
