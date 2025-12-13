function f(x) { return 2 * g(x) }
function g(x) { return add(x, 1); }
function add(x, y) { console.log(Date.now()); greetings(); return x + y; }
function greetings() { return "hello, World!"; }
function times(a, n) {
    var sum = 0;
    for (let i = 0; n > i; i++) {
        sum += a;
    }
    return sum;
}

function fib(n) {
    if (n <= 0)
        return 0;
    else if (n == 1)
        return 1;
    return fib(n - 1) + fib(n - 2);
}

console.log("fib(10)=", fib(10));
console.log(times(2, 50));
console.log(f(1.5));



var log_str = "";

function log(str)
{
    log_str += str + ",";
}

function F(a, b, c)
{
    var x = 10;
    log("a="+a);
    function G(d) {
        function H() {
            log("d=" + d);
            log("x=" + x);
        }
        log("b=" + b);
        log("c=" + c);
        H();
    }
    G(4);
    return G;
}

var g1 = F(1, 2, 3);
g1(5);

console.log(log_str == "a=1,b=2,c=3,d=4,x=10,b=2,c=3,d=5,x=10,");

print(-10 * 200)
print(-1 / 0);
print(3 * -2147483646);

print(2 ** 7);
