string g_out;
int destroyed;
int decremented = 2;

class Lifetime
{
    ~Lifetime()
    {
        destroyed++;
    }
}

class DecrementLifetime
{
    ~DecrementLifetime()
    {
        decremented--;
    }
}

Lifetime@ retained;

void retainLifetime()
{
    Lifetime value;
    @retained = @value;
}

int main()
{
    retainLifetime();
    g_out += itos(destroyed) + "\n";
    @retained = null;
    g_out += itos(destroyed) + "\n";
    {
        Lifetime direct;
    }
    g_out += itos(destroyed) + "\n";
    {
        DecrementLifetime direct;
    }
    g_out += itos(decremented) + "\n";
    return destroyed + decremented;
}
