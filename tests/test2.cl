// {
//   var a = 3;
//   fun foo(b) {
//     print b;
//   }
//   foo(1);
// }

// var a = 2;
// fun outer() {
//   var x = 1;
//   fun inner() {
//     print a;
//   }
//   inner();
// }
// outer();

var globalSet;
var globalGet;

fun main() {
  var a = "initial";

  fun set() { a = "updated"; }
  fun get() { print a; }

  globalSet = set;
  globalGet = get;
}

main();
globalSet();
globalGet();

var start = clock();
var i = 0;
while(i < 2) {
  i = i+1;
  main();
  globalGet();
}
var end = clock();
print end - start;