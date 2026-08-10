string g_out;
int destroyed = 0;

interface IMeasure
{
    int measure() const;
}

abstract class MeasureBase : IMeasure
{
    protected int factor;
    private int secret = 2;

    MeasureBase(int value)
    {
        factor = value;
    }

    int basePart() const
    {
        return protectedPart() + privatePart();
    }

    protected int protectedPart() const
    {
        return factor;
    }

    private int privatePart() const
    {
        return secret;
    }

    int measure() const
    {
        return factor;
    }
}

class IntermediateMeasure : MeasureBase
{
    IntermediateMeasure(int value)
    {
        super(value);
    }

    int measure() const override
    {
        return factor * 2 + protectedPart();
    }
}

final class FinalMeasure : IntermediateMeasure
{
    int value = 4;

    FinalMeasure(int input)
    {
        super(input + 1);
        value = input;
    }

    int measure() const override final
    {
        return IntermediateMeasure::measure() + value + basePart();
    }
}

class Constructed
{
    int value;

    Constructed()
    {
        value = 1;
    }

    Constructed(int input)
    {
        value = input;
    }

    Constructed(const Constructed&in other)
    {
        value = other.value + 1;
    }

    Constructed(string input) explicit
    {
        value = int(input.length());
    }
}

class Counter
{
    int value = 3;

    int mode()
    {
        value++;
        return value;
    }

    int mode() const
    {
        return value;
    }
}

class PropertyBox
{
    private int stored;
    private int first;
    private int second;

    int value
    {
        get const { return stored; }
        set { stored = value; }
    }

    int get_slot(int index) const property
    {
        return index == 0 ? first : second;
    }

    void set_slot(int index, int input) property
    {
        if (index == 0)
            first = input;
        else
            second = input;
    }
}

class Lifetime
{
    ~Lifetime()
    {
        destroyed++;
    }
}

void createLifetimes()
{
    Lifetime first;
    {
        Lifetime second;
    }
}

int main()
{
    FinalMeasure measured(5);
    IMeasure@ interfaceHandle = @measured;
    g_out += itos(interfaceHandle.measure()) + "\n";

    Constructed first(8);
    Constructed copied(first);
    Constructed explicitValue = Constructed("abcd");
    g_out += itos(first.value + copied.value + explicitValue.value) + "\n";

    Counter counter;
    const Counter@ constCounter = @counter;
    g_out += itos(counter.mode()) + "," + itos(constCounter.mode()) + "\n";

    PropertyBox properties;
    properties.value = 12;
    properties.slot[0] = 7;
    properties.slot[1] = 9;
    g_out += itos(properties.value + properties.slot[0] + properties.slot[1]) + "\n";

    createLifetimes();
    g_out += itos(destroyed) + "\n";
    return 0;
}
