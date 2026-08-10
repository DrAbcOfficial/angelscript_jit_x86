#include "as_jit_x86.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstdstring.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

const char* kArithInt32 = R"AS(
int main()
{
    int sum = 0;
    int a = 7;
    int b = 3;
    int limit = 3000000;
    for (int i = 0; i < limit; i++)
    {
        int v = a + b;
        v -= 2;
        v *= 3;
        v /= 2;
        v %= 7;
        v = -v;
        v **= 2;
        int w = (a & b) | (a ^ b);
        w += ~a;
        w += a << 2;
        w += a >> 1;
        w += (-a) >> 1;
        w ^= 3;
        w &= 13;
        w |= 8;
        w <<= 1;
        w >>= 2;
        sum += v + w + (i & 1);
        a += 3;
        b = (b + 1) & 0xF;
    }
    return sum;
}
)AS";

const char* kArithUint32 = R"AS(
int main()
{
    uint sum = 0;
    uint u = 4000000000;
    uint limit = 3000000;
    for (uint i = 0; i < limit; i++)
    {
        uint v = u - 1;
        v /= 2;
        v %= 7;
        v *= 3;
        v += 5;
        uint w = 0x80000000;
        w >>>= 31;
        w |= 8;
        w &= 13;
        w ^= 3;
        w <<= 2;
        w >>= 1;
        uint shifted = 0x80000000 >>> 30;
        sum += v + w + shifted + (i & 3);
        u += 7;
    }
    return int(sum & 0x7FFFFFFF);
}
)AS";

const char* kArithInt64 = R"AS(
int main()
{
    int64 sum = 0;
    int64 carry = 4294967295;
    int64 inc = 2;
    int limit = 3000000;
    for (int i = 0; i < limit; i++)
    {
        carry = carry + inc;
        int64 v = carry - 5;
        v *= 3;
        v /= 2;
        v %= 7;
        sum += v + (i & 1);
    }
    return int(sum & 0x7FFFFFFF);
}
)AS";

const char* kArithFloat = R"AS(
int main()
{
    float sum = 0.0f;
    float f1 = 1.5f;
    float f2 = 2.25f;
    int limit = 3000000;
    for (int i = 0; i < limit; i++)
    {
        float v = f1 + f2;
        v *= f1;
        v /= f2;
        v -= 0.5f;
        v = -v;
        v += float(int(v) & 3);
        sum += v;
        f1 += 0.25f;
        if (f1 > 4.0f) f1 = 1.5f;
    }
    return int(sum);
}
)AS";

const char* kArithDouble = R"AS(
int main()
{
    double sum = 0.0;
    double d1 = 1.25;
    double d2 = 0.5;
    int limit = 3000000;
    for (int i = 0; i < limit; i++)
    {
        double v = d1 + d2;
        v *= d1;
        v -= d2;
        v /= 2.0;
        sum += v;
        d1 += 0.125;
        if (d1 > 4.0) d1 = 1.25;
    }
    return int(sum) + int(float(d1));
}
)AS";

const char* kBranchSwitch = R"AS(
int main()
{
    int total = 0;
    for (int i = 0; i < 1000000; i++)
    {
        int sel = i & 7;
        switch (sel)
        {
        case 0: total += 1; break;
        case 1: total += 2; break;
        case 2:
        case 3: total += 3; break;
        default: total += (sel > 5 ? 4 : 5); break;
        }
        if (sel < 2) total += 10;
        else if (sel < 4) total += 20;
        else total += 30;
        bool t = (i & 1) == 1;
        bool f = false;
        if (t && !f) total++;
        if (t || f) total++;
        int j = 0;
        do { j++; } while (j < 2);
        total += j;
        int k = 0;
        for (;;) { k++; if (k == 2) break; }
        total += k;
        for (int a = 0, b = 2; a < 2; a++, b--)
        {
            if ((a & 1) == 0) continue;
            total += a * b;
        }
        {
            int scoped = sel;
            scoped *= 3;
            total += scoped;
        }
        ;
    }
    return total;
}
)AS";

const char* kCallsBasic = R"AS(
int sum3(int a, int b, int c) { return a + b + c; }
float avgf(float a, float b) { return (a + b) * 0.5f; }
double avgd(double a, double b) { return (a + b) * 0.5; }
void bump(int&out v) { v += 1; }
int withdefault(int a, int b = 5, int c = 7) { return a + b + c; }
int overloaded(int value) { return value + 1; }
float overloaded(float value) { return value + 0.5f; }
int noArguments(void) { return 23; }

int main()
{
    int sum = 0;
    int limit = 500000;
    for (int i = 0; i < limit; i++)
    {
        sum += sum3(i & 7, 2, 3);
        sum += int(avgf(1.0f, float(i & 3)));
        sum += int(avgd(1.0, double(i & 3)));
        int v = i & 15;
        bump(v);
        sum += v;
        sum += withdefault(1);
        sum += withdefault(1, 2);
        sum += withdefault(1, 2, 3);
        sum += withdefault(b: 2, a: 1);
        sum += overloaded(i & 3);
        sum += int(overloaded(4.0f));
        sum += noArguments();
    }
    return sum;
}
)AS";

const char* kCallsRecursive = R"AS(
int fib(int n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main()
{
    int sum = 0;
    for (int i = 0; i < 400; i++)
        sum += fib(20);
    return sum;
}
)AS";

const char* kCallsIndirect = R"AS(
funcdef int Binary(int, int);

int add(int left, int right) { return left + right; }

int apply(int left, int right, Binary@ op) { return op(left, right); }

class Target
{
    int bias;
    Target(int value) { bias = value; }
    int addBias(int left, int right) { return left + right + bias; }
}

int main()
{
    Binary@ direct = @add;
    Binary@ system = @add2;
    Binary@ lambda = function(int left, int right) { return left - right; };
    Target target(7);
    Binary@ delegate = Binary(target.addBias);
    int sum = 0;
    for (int i = 0; i < 500000; i++)
    {
        sum += apply(8, 3, direct);
        sum += apply(8, 3, system);
        sum += apply(8, 3, lambda);
        sum += apply(1, 2, delegate);
    }
    if (direct !is null) sum++;
    return sum;
}
)AS";

const char* kClassMethods = R"AS(
interface IMeasure
{
    int measure() const;
}

abstract class MeasureBase : IMeasure
{
    protected int factor;
    private int secret = 2;

    MeasureBase(int value) { factor = value; }
    int basePart() const { return protectedPart() + privatePart(); }
    protected int protectedPart() const { return factor; }
    private int privatePart() const { return secret; }
    int measure() const { return factor; }
}

class IntermediateMeasure : MeasureBase
{
    IntermediateMeasure(int value) { super(value); }
    int measure() const override { return factor * 2 + protectedPart(); }
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

int main()
{
    FinalMeasure a(5);
    FinalMeasure b(8);
    IMeasure@ handle = @a;
    int sum = 0;
    for (int i = 0; i < 500000; i++)
    {
        sum += handle.measure();
        sum += b.measure();
        sum += b.basePart();
    }
    return sum;
}
)AS";

const char* kClassLifetime = R"AS(
int destroyed = 0;

class Lifetime
{
    ~Lifetime() { destroyed++; }
}

class Constructed
{
    int value;

    Constructed() { value = 1; }
    Constructed(int input) { value = input; }
    Constructed(const Constructed&in other) { value = other.value + 1; }
    Constructed(string input) explicit { value = int(input.length()); }
}

int main()
{
    int sum = 0;
    for (int i = 0; i < 300000; i++)
    {
        Constructed a(i & 7);
        Constructed b(a);
        sum += a.value + b.value;
        {
            Lifetime scoped;
        }
    }
    Constructed explicitValue = Constructed("abcd");
    return sum + explicitValue.value + destroyed;
}
)AS";

const char* kClassProperties = R"AS(
class Counter
{
    int value = 3;

    int mode() { value++; return value; }
    int mode() const { return value; }
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
        if (index == 0) first = input;
        else second = input;
    }
}

int main()
{
    PropertyBox properties;
    Counter counter;
    const Counter@ constCounter = @counter;
    int sum = 0;
    for (int i = 0; i < 500000; i++)
    {
        properties.value = i & 0xFF;
        properties.slot[0] = i & 7;
        properties.slot[1] = i & 3;
        sum += properties.value + properties.slot[0] + properties.slot[1];
        sum += counter.mode() + constCounter.mode();
    }
    return sum;
}
)AS";

const char* kOperatorsClass = R"AS(
class Number
{
    int value;
    int first;
    int second;

    Number(int input = 0) { value = input; }

    Number opNeg() const { return Number(-value); }
    Number opCom() const { return Number(~value); }
    Number opPreInc() { value++; return Number(value); }
    Number opPostInc() { Number previous(value); value++; return previous; }
    Number opPreDec() { value--; return Number(value); }
    Number opPostDec() { Number previous(value); value--; return previous; }
    bool opEquals(const Number&in other) const { return value == other.value; }
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
    Number@ opAddAssign(int input) { value += input; return this; }
    Number opAdd(const Number&in other) const { return Number(value + other.value); }
    Number opAdd_r(int left) const { return Number(left + value); }
    Number opPow(int exponent) const
    {
        int result = 1;
        for (int i = 0; i < exponent; i++) result *= value;
        return Number(result);
    }
    int& opIndex(int index) { if (index == 0) return first; return second; }
    const int& opIndex(int index) const { if (index == 0) return first; return second; }
    int& opIndex(int row, int column) { if (row == column) return first; return second; }
    int opCall(int left, int right) const { return value + left + right; }
    int opConv() const { return value; }
}

int main()
{
    int sum = 0;
    for (int i = 0; i < 200000; i++)
    {
        Number number(i & 7);
        Number negative = -number;
        Number complement = ~number;
        Number before = number++;
        Number after = ++number;
        Number beforeDec = number--;
        Number afterDec = --number;
        Number total = number + Number(4);
        Number reverse = 10 + number;
        Number power = Number(3) ** 2;
        number += 5;
        number[0] = i & 7;
        number[1] = i & 3;
        number[0, 1] = i & 1;
        if (number == Number(number.value)) sum++;
        if (number > Number(1)) sum++;
        if (number >= Number(1) && number <= Number(number.value)) sum++;
        sum += number[0] + number[1];
        sum += number(2, 3);
        sum += int(number);
        sum += negative.value + complement.value + before.value + after.value;
        sum += beforeDec.value + afterDec.value + total.value + reverse.value + power.value;
    }
    return sum;
}
)AS";

const char* kHandlesRefs = R"AS(
interface INode
{
    int read() const;
}

class Node : INode
{
    int value;
    Node(int input) { value = input; }
    int read() const { return value; }
}

class SpecialNode : Node
{
    SpecialNode(int input) { super(input); }
    int read() const override { return value * 10; }
}

int referenceValue = 0;

int& globalReference()
{
    return referenceValue;
}

int main()
{
    Node base(3);
    SpecialNode special(4);
    array<Node@> nodes(2);
    int sum = 0;
    for (int i = 0; i < 300000; i++)
    {
        Node@ first = @base;
        Node@ second;
        @second = @first;
        if (first is second) sum++;
        INode@ interfaceHandle = @special;
        SpecialNode@ downcast = cast<SpecialNode>(interfaceHandle);
        if (downcast !is null) sum += downcast.read();
        Node@ upcast = downcast;
        sum += upcast.read();
        SpecialNode@ failed = cast<SpecialNode>(first);
        if (failed is null) sum++;
        const Node@ readOnlyObject = @base;
        Node@ const fixedHandle = @base;
        const Node@ const fixedReadOnly = @base;
        sum += readOnlyObject.read() + fixedHandle.read() + fixedReadOnly.read();
        @nodes[0] = @base;
        @nodes[1] = @special;
        sum += nodes[0].read() + nodes[1].read();
        globalReference() = i & 0xFF;
        sum += referenceValue;
        @second = null;
        if (second is null && first !is null) sum++;
    }
    return sum;
}
)AS";

const char* kStringOps = R"AS(
int main()
{
    string singleQuoted = 'single';
    string multiLine = """alpha
beta""";
    int sum = int(singleQuoted.length()) + int(multiLine.length());
    string acc;
    for (int i = 0; i < 30000; i++)
    {
        string s = "a" + itos(i & 1023);
        s += "x";
        if (s == "a5x") sum++;
        if (s != "a0x") sum++;
        if (s > "a0x") sum++;
        if (s < "zzz") sum++;
        s[0] = 98;
        sum += int(s.length());
        string sub = s.substr(1, 2);
        sum += int(sub.length());
        acc = s;
    }
    return sum + int(acc.length());
}
)AS";

const char* kArrayOps = R"AS(
int sumArray(const array<int>&in values)
{
    int total = 0;
    for (uint i = 0; i < values.length(); i++)
        total += values[i];
    return total;
}

int main()
{
    array<int> values = {1, 2, 3};
    int[] shorthand = {4, 5};
    array<array<int>> matrix = {{6, 7}, {8, 9}};
    array<int>@ valuesHandle = @values;
    int sum = 0;
    for (int i = 0; i < 50000; i++)
    {
        valuesHandle[1] = i & 0xFF;
        sum += sumArray(values);
        sum += sumArray(shorthand);
        sum += matrix[0][1] + matrix[1][0];
        sum += sumArray({10, 11, 12});
        sum += sumArray(array<int> = {13, 14});
    }
    return sum;
}
)AS";

const char* kGlobalsNs = R"AS(
enum Direction
{
    North,
    East = 3,
    South,
    Scaled = East * 10
}

typedef int32 Score;

namespace Alpha
{
    int seed = 5;

    int twice(int value) { return value * 2; }

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

int main()
{
    Direction direction = South;
    Score score = 12;
    int sum = 0;
    for (int i = 0; i < 1000000; i++)
    {
        globalProperty = i & 0xFF;
        sum += globalProperty;
        sum += Alpha::twice(4) + Alpha::seed + Alpha::Nested::value + Alpha::More::value;
        sum += ::globalValue + int(globalMask) + int(direction) + score;
    }
    return sum;
}
)AS";

const char* kTypesMixed = R"AS(
int main()
{
    int8 i8 = -8;
    int16 i16 = 160;
    int64 i64 = 64000;
    uint8 u8 = 8;
    uint16 u16 = 160;
    uint64 u64 = 64000;
    uint literals = 0b1010 + 0o10 + 0d10 + 0x10;
    bool flag = true;
    auto inferred = 7;
    const auto fixedValue = 9;
    int sum = 0;
    for (int i = 0; i < 1000000; i++)
    {
        i8 = int8(i & 0x7F);
        i16 = int16(i & 0x7FFF);
        sum += int(i8) + int(i16);
        sum += int(u8) + int(u16);
        i64 += 2;
        u64 += 3;
        sum += int(literals) + inferred + fixedValue;
        bool words = (true and flag) && (false or flag) && (true xor false);
        bool symbols = true ^^ flag;
        if (words && symbols) sum++;
    }
    return sum + int(i64 & 0x7FFFFFFF) + int(u64 & 0x7FFFFFFF);
}
)AS";

const char* kSysCalls = R"AS(
int main()
{
    int sum = 0;
    int total = 0;
    for (int i = 0; i < 1000000; i++)
    {
        sum += add2(3, 4);
        sum += int(mul2f(2.5f, 4.0f));
        accumulate(total, 1);
        sum += total & 0xFF;
    }
    return sum;
}
)AS";

const char* kExceptions = R"AS(
class Obj
{
    int v;
}

int main()
{
    int caught = 0;
    int limit = 20000;
    for (int i = 0; i < limit; i++)
    {
        try { RaiseError(); } catch { caught++; }
        try { int zero = 0; int value = 42 / zero; caught += value; } catch { caught++; }
        try { int minv = int(0x80000000); int neg = 0 - 1; int value = minv / neg; caught += value; } catch { caught++; }
        try { int minv = int(0x80000000); int neg = 0 - 1; int value = minv % neg; caught += value; } catch { caught++; }
        try { Obj@ o = null; o.v = 5; } catch { caught++; }
    }
    return caught;
}
)AS";

const char* kImportsProvider = R"AS(
int importedAdd(int left, int right)
{
    return left + right;
}

void importedAccumulate(int&out total, int value)
{
    total = value + 11;
}
)AS";

const char* kImportsConsumer = R"AS(
import int importedAdd(int, int) from "showcase_imports_provider";
import void importedAccumulate(int&out, int) from "showcase_imports_provider";

int main()
{
    int total = 0;
    for (int i = 0; i < 1000000; i++)
    {
        total = importedAdd(total & 0xFF, 7);
        importedAccumulate(total, i & 0xFF);
    }
    return total;
}
)AS";

const char* kSharedProvider = R"AS(
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

    SharedModuleCounter(int input) { value = input; }
    void bump(int input) { value += input; }
    int get() const { return value; }
}

shared int sharedModuleDouble(int value)
{
    return value * 2;
}
)AS";

const char* kSharedConsumer = R"AS(
external shared enum SharedModuleMode;
external shared funcdef int SharedModuleOperation(int);
external shared interface ISharedModuleCounter;
external shared class SharedModuleCounter;
external shared int sharedModuleDouble(int);

mixin class IncrementMixin
{
    int count;

    void increment() { count++; }
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
    SharedModuleMode mode = SharedModuleOn;
    SharedModuleCounter counter(5);
    ISharedModuleCounter@ interfaceHandle = @counter;
    SharedModuleOperation@ operation = @sharedModuleDouble;
    MixedCounter mixed;
    int sum = int(mode);
    for (int i = 0; i < 200000; i++)
    {
        counter.bump(1);
        sum += interfaceHandle.get() & 0xFF;
        sum += operation(6);
        mixed.count = 0;
        sum += mixed.run(4);
    }
    return sum;
}
)AS";

const char* kSharedLocal = R"AS(
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

    SharedValue(int input) { value = input; }
    int get() const { return value; }
}

shared int sharedTwice(int value)
{
    return value * 2;
}

int main()
{
    SharedState state = SharedReady;
    SharedValue value(6);
    ISharedValue@ handle = @value;
    SharedOperation@ operation = @sharedTwice;
    int sum = int(state);
    for (int i = 0; i < 300000; i++)
        sum += handle.get() + operation(7);
    return sum;
}
)AS";

struct CaseDef {
    const char* name;
    const char* code;
    const char* providerName;
    const char* providerCode;
    bool bindImports;
};

const CaseDef kCases[] = {
    {"arith-int32", kArithInt32, nullptr, nullptr, false},
    {"arith-uint32", kArithUint32, nullptr, nullptr, false},
    {"arith-int64", kArithInt64, nullptr, nullptr, false},
    {"arith-float", kArithFloat, nullptr, nullptr, false},
    {"arith-double", kArithDouble, nullptr, nullptr, false},
    {"branch-switch", kBranchSwitch, nullptr, nullptr, false},
    {"calls-basic", kCallsBasic, nullptr, nullptr, false},
    {"calls-recursive", kCallsRecursive, nullptr, nullptr, false},
    {"calls-indirect", kCallsIndirect, nullptr, nullptr, false},
    {"class-methods", kClassMethods, nullptr, nullptr, false},
    {"class-lifetime", kClassLifetime, nullptr, nullptr, false},
    {"class-properties", kClassProperties, nullptr, nullptr, false},
    {"operators-class", kOperatorsClass, nullptr, nullptr, false},
    {"handles-refs", kHandlesRefs, nullptr, nullptr, false},
    {"string-ops", kStringOps, nullptr, nullptr, false},
    {"array-ops", kArrayOps, nullptr, nullptr, false},
    {"globals-ns", kGlobalsNs, nullptr, nullptr, false},
    {"types-mixed", kTypesMixed, nullptr, nullptr, false},
    {"sys-calls", kSysCalls, nullptr, nullptr, false},
    {"exceptions", kExceptions, nullptr, nullptr, false},
    {"imports", kImportsConsumer, "showcase_imports_provider", kImportsProvider, true},
    {"shared-modules", kSharedConsumer, "showcase_shared_provider", kSharedProvider, false},
    {"shared-local", kSharedLocal, nullptr, nullptr, false},
};

void MessageCallback(const asSMessageInfo* msg, void* param) {
    int* errs = static_cast<int*>(param);
    if (msg->type == asMSGTYPE_ERROR) {
        (*errs)++;
        std::fprintf(stderr, "  [msg] %s (%d,%d): %s\n", msg->section, msg->row, msg->col, msg->message);
    }
}

std::string Itos(int v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

std::string Ftos(float v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", (double)v);
    return buf;
}

std::string Dtos(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

int Add2(int a, int b) {
    return a + b;
}

float Mul2f(float a, float b) {
    return a * b;
}

void Accumulate(int& total, int v) {
    total += v;
}

void RaiseError() {
    asIScriptContext* ctx = asGetActiveContext();
    ctx->SetException("script error", true);
}

bool RegisterAll(asIScriptEngine* engine) {
    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    if (engine->RegisterGlobalFunction("string itos(int)", asFUNCTION(Itos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("string ftos(float)", asFUNCTION(Ftos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("string dtos(double)", asFUNCTION(Dtos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("int add2(int, int)", asFUNCTION(Add2), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("float mul2f(float, float)", asFUNCTION(Mul2f), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("void accumulate(int&out, int)", asFUNCTION(Accumulate), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("void RaiseError()", asFUNCTION(RaiseError), asCALL_CDECL) < 0) return false;
    return true;
}

asIScriptFunction* BuildCase(asIScriptEngine* engine, const CaseDef& def) {
    if (def.providerCode) {
        asIScriptModule* provider = engine->GetModule(def.providerName, asGM_ALWAYS_CREATE);
        if (!provider) return nullptr;
        if (provider->AddScriptSection(def.providerName, def.providerCode) < 0) return nullptr;
        if (provider->Build() < 0) return nullptr;
    }
    asIScriptModule* module = engine->GetModule(def.name, asGM_ALWAYS_CREATE);
    if (!module) return nullptr;
    if (module->AddScriptSection(def.name, def.code) < 0) return nullptr;
    if (module->Build() < 0) return nullptr;
    if (def.bindImports && module->BindAllImportedFunctions() < 0) return nullptr;
    return module->GetFunctionByName("main");
}

double Run(asIScriptEngine* engine, asIScriptFunction* function, asDWORD* result) {
    double best = std::numeric_limits<double>::max();
    for (int round = 0; round < 3; round++) {
        asIScriptContext* context = engine->CreateContext();
        if (!context || context->Prepare(function) < 0) return -1.0;
        auto begin = std::chrono::steady_clock::now();
        int state = context->Execute();
        auto end = std::chrono::steady_clock::now();
        if (state != asEXECUTION_FINISHED) {
            context->Release();
            return -1.0;
        }
        *result = context->GetReturnDWord();
        context->Release();
        double elapsed = std::chrono::duration<double, std::milli>(end - begin).count();
        best = std::min(best, elapsed);
    }
    return best;
}

bool ModuleHasJitFunctions(asIScriptModule* mod) {
    for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        asIScriptFunction* f = mod->GetFunctionByIndex(i);
        asUINT len = 0;
        asDWORD* bc = f->GetByteCode(&len);
        if (!bc) continue;
        asDWORD* p = bc;
        asDWORD* end = bc + len;
        while (p < end) {
            asEBCInstr op = static_cast<asEBCInstr>(*p & 0xFF);
            if (op == asBC_JitEntry && *(asPWORD*)(p + 1) != 0)
                return true;
            p += asBCTypeSize[asBCInfo[op].type];
        }
    }
    return false;
}

}

int main() {
    asIScriptEngine* interpreter = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    asIScriptEngine* jitEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!interpreter || !jitEngine) return 1;

    interpreter->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    jitEngine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

    int errs = 0;
    interpreter->SetMessageCallback(asFUNCTION(MessageCallback), &errs, asCALL_CDECL);
    jitEngine->SetMessageCallback(asFUNCTION(MessageCallback), &errs, asCALL_CDECL);

    void* jit = AsJitCreateEngine(jitEngine);
    if (!jit) return 1;

    if (!RegisterAll(interpreter) || !RegisterAll(jitEngine)) return 1;

    {
        asIScriptModule* probe = jitEngine->GetModule("showcase_probe", asGM_ALWAYS_CREATE);
        if (!probe || probe->AddScriptSection("probe", "int main() { return 1; }") < 0 ||
            probe->Build() < 0 || !ModuleHasJitFunctions(probe)) {
            std::printf("JIT probe failed\n");
            return 1;
        }
    }

    std::printf("%-18s %12s %12s %9s %s\n", "case", "interp ms", "jit ms", "speedup", "check");

    int failures = 0;
    double totalInterp = 0.0;
    double totalJit = 0.0;
    double logSum = 0.0;
    double minSpeedup = std::numeric_limits<double>::max();
    int measured = 0;

    for (size_t c = 0; c < sizeof(kCases) / sizeof(kCases[0]); c++) {
        const CaseDef& def = kCases[c];
        asIScriptFunction* interpFunc = BuildCase(interpreter, def);
        asIScriptFunction* jitFunc = BuildCase(jitEngine, def);
        if (!interpFunc || !jitFunc) {
            std::printf("%-18s build failed\n", def.name);
            failures++;
            continue;
        }
        asDWORD interpResult = 0;
        asDWORD jitResult = 0;
        double interpMs = Run(interpreter, interpFunc, &interpResult);
        double jitMs = Run(jitEngine, jitFunc, &jitResult);
        bool match = interpMs > 0.0 && jitMs > 0.0 && interpResult == jitResult;
        double speedup = match ? interpMs / jitMs : 0.0;
        std::printf("%-18s %12.3f %12.3f %8.2fx %s\n", def.name, interpMs, jitMs, speedup,
                    match ? "ok" : "MISMATCH");
        if (!match) {
            failures++;
            continue;
        }
        totalInterp += interpMs;
        totalJit += jitMs;
        logSum += std::log(speedup);
        minSpeedup = std::min(minSpeedup, speedup);
        measured++;
    }

    std::printf("%-18s %12.3f %12.3f %8.2fx\n", "TOTAL", totalInterp, totalJit,
                totalInterp / totalJit);
    if (measured > 0) {
        std::printf("cases=%d geomean=%.2fx min=%.2fx\n", measured,
                    std::exp(logSum / measured), minSpeedup);
    }

    jitEngine->Release();
    AsJitDestroyEngine(jit);
    interpreter->Release();

    bool pass = failures == 0 && measured == int(sizeof(kCases) / sizeof(kCases[0]));
    std::printf(pass ? "SHOWCASE PASSED\n" : "SHOWCASE FAILED\n");
    return pass ? 0 : 1;
}
