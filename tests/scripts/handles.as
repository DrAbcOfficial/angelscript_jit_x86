string g_out;

interface INode
{
    int read() const;
}

class Node : INode
{
    int value;

    Node(int input)
    {
        value = input;
    }

    int read() const
    {
        return value;
    }
}

class SpecialNode : Node
{
    SpecialNode(int input)
    {
        super(input);
    }

    int read() const override
    {
        return value * 10;
    }
}

int main()
{
    Node base(3);
    SpecialNode special(4);
    Node@ first = @base;
    Node@ second;
    @second = @first;
    g_out += first is second ? "same\n" : "different\n";

    INode@ interfaceHandle = @special;
    SpecialNode@ downcast = cast<SpecialNode>(interfaceHandle);
    Node@ upcast = downcast;
    SpecialNode@ failed = cast<SpecialNode>(first);
    g_out += itos(interfaceHandle.read()) + "," + itos(upcast.read()) + "\n";
    g_out += failed is null ? "null\n" : "bad\n";

    const Node@ readOnlyObject = @base;
    Node@ const fixedHandle = @base;
    const Node@ const fixedReadOnly = @base;
    g_out += itos(readOnlyObject.read() + fixedHandle.read() + fixedReadOnly.read()) + "\n";

    array<Node@> nodes(2);
    @nodes[0] = @base;
    @nodes[1] = @special;
    g_out += itos(nodes[0].read() + nodes[1].read()) + "\n";

    @second = null;
    g_out += second is null && first !is null ? "released\n" : "bad\n";
    return 0;
}
