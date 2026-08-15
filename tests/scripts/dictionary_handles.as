string g_out;
int destroyed;

class DictionaryOwned
{
    int value;

    DictionaryOwned(int input)
    {
        value = input;
    }

    ~DictionaryOwned()
    {
        destroyed++;
    }
}

int main()
{
    int sum = 0;
    {
        dictionary values;
        DictionaryOwned box(7);
        values.set("box", @box);
        for (int i = 0; i < 32; i++)
        {
            DictionaryOwned@ found;
            if (values.get("box", @found) && found !is null)
                sum += found.value;
        }
        g_out += itos(sum) + "," + itos(destroyed) + "\n";
    }
    g_out += itos(destroyed) + "\n";
    return sum + destroyed;
}
