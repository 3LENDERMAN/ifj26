import ifj26.*

def factorial(n: Int): Int =
    if n < 2 then
        return 1
    else
        return n * factorial(n - 1)

def compute(x: Int, y: Double): Double =
    val base = 10
    var result = 0.0
    if x > 0 then
        result = y + 2.5
    else
        result = y - 1.0
    
    val formatted = toStr(result)
    print("test of nested toStr: " + formatted)
    return result

// main function
@main
def main(): Unit =
    print("enter int:")
    val inputInt = readInt()
    
    print("enter decimal:")
    val inputDouble = readDouble()
    
    print("enter text:")
    val inputStr = readLine()

    val calc = 2 * (inputInt + 5) - 3
    print("operations: " + toStr(calc))

    // test for recursion and implicit conversion int-double
    val fact = factorial(inputInt)
    print("factorial: " + toStr(fact))

    // test of shadowing variables
    val finalRes = compute(inputInt, inputDouble)
    print("final result of compute: " + toStr(finalRes))

    // string handling
    val length = strLength(inputStr)
    print("str length: " + toStr(length))

    if length > 0 then
        val sub = subStr(inputStr, 0, 1)
        print("first char: " + sub)
        val charCode = ord(sub, 0)
        print("ASCII code: " + toStr(charCode))
    else
        print("string is empty")

    // while loop test
    var counter = 3
    while counter > 0 do
        print("countdown: " + toStr(counter))
        counter = counter - 1

    print("end of extended tests")
