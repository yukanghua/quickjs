function f(x) { return 2 * g(x) }
function g(x) { return add(x, 1); }
function add(x, y) { return x + y; }

console.log(f(1));
