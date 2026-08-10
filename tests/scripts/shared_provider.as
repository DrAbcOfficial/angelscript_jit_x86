shared enum SharedModuleMode
{
    SharedModuleOff,
    SharedModuleOn = 2
}

shared funcdef int SharedModuleOperation(int);

shared interface ISharedModuleCounter
{
    int get() const;
}

shared class SharedModuleCounter : ISharedModuleCounter
{
    int value;

    SharedModuleCounter(int input)
    {
        value = input;
    }

    void bump(int input)
    {
        value += input;
    }

    int get() const
    {
        return value;
    }
}

shared int sharedModuleDouble(int value)
{
    return value * 2;
}
