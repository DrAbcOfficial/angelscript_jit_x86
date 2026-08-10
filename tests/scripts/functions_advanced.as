string g_out;

funcdef int Binary(int, int);

int referenceValue = 0;

int overloaded(int value)
{
    return value + 1;
}

float overloaded(float value)
{
    return value + 0.5f;
}

int combine(int left, int right, int extra = 1)
{
    return left * 100 + right * 10 + extra;
}

int noArguments(void)
{
    return 23;
}

void transform(const int&in input, int&out output, int&out total)
{
    output = input * 2;
    total = input + output;
}

int& globalReference()
{
    return referenceValue;
}

int add(int left, int right)
{
    return left + right;
}

int apply(int left, int right, Binary@ op)
{
    return op(left, right);
}

class Target
{
    int bias;

    Target(int value)
    {
        bias = value;
    }

    int addBias(int left, int right)
    {
        return left + right + bias;
    }
}

class ReferenceBox
{
    int value;

    int& memberReference()
    {
        return value;
    }
}

void mutate(ReferenceBox&inout box)
{
    box.value += 3;
}

int main()
{
    g_out += itos(overloaded(4)) + "\n";
    g_out += ftos(overloaded(4.0f)) + "\n";
    g_out += itos(combine(right: 2, left: 3)) + "\n";
    g_out += itos(combine(extra: 9, left: 1, right: 2)) + "\n";
    g_out += itos(noArguments()) + "\n";

    int output = 0;
    int total = 0;
    transform(6, output, total);
    g_out += itos(output) + "," + itos(total) + "\n";

    globalReference() = 41;
    ReferenceBox box;
    box.memberReference() = 17;
    mutate(box);
    g_out += itos(referenceValue + box.value) + "\n";

    Binary@ direct = @add;
    Binary@ system = @add2;
    Binary@ lambda = function(int left, int right) { return left - right; };
    Target target(7);
    Binary@ delegate = Binary(target.addBias);
    g_out += itos(apply(8, 3, direct)) + "\n";
    g_out += itos(apply(8, 3, system)) + "\n";
    g_out += itos(apply(8, 3, lambda)) + "\n";
    g_out += itos(apply(8, 3, delegate)) + "\n";
    g_out += direct !is null ? "handle\n" : "null\n";
    return 0;
}
