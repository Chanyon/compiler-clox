// if(1 != 1 ){
//     print true;
// } else { print 2; }

// if( 1 == 1 ) {
//     print true;
// } else {}
//
// var i = 1;
// while(i < 2) {
//     var a = 6;
//     i=i+1;
//     print "while statement";
//     while(1 != 1){

//     }
// }

// for(;;){
//     print true;
// }

// for(var a = 0;;) {}

// for(var a = 1; a < 2;){
//     print a;
//     a = a + 1;
// }
//
// for(var j = 0; j < 2; j = j+1) {
//     if(j == 1) {
//         print "for statement";
//     }
// }
//
// print 1;

// fun foo (d){
//     print d;
// }

// foo(1);
// print 2;

// fun a() { b(3); }
// fun b(a){
//     print a;
// }
// a();

// fun foo(a) {
//     return a;
// }
// print foo(1);

// fun fib(n) {
//     if(n < 2) {
//         return n;
//     }
//     return fib(n - 2) + fib(n - 1);
// }

// var start = clock();
// print fib(3);
// print (clock() - start);

// 闭包
// var x = "global";
// fun outer() {
//   var x = "outer";
//   fun inner() {
//     print x;
//   }
//   inner();
// }
// outer();

// undfined variable `x`.
// error: [line 81] in closure()
// fun makeClosure() {
//     var x = "local";
//     fun closure(){
//         print x;
//     }
//     return closure;
// }

// var closure = makeClosure();
// closure();

// fun makeClosure(value) {
//   fun closure() {
//     print value;
//   }
//   return closure;
// }

// var doughnut = makeClosure("doughnut");
// var bagel = makeClosure("bagel");
// doughnut();
// bagel();

// fun outer() {
//   var a = 1;
//   var b = 2;
//   fun middle() {
//     var c = 3;
//     var d = 4;
//     fun inner() {
//       print a + c + b + d;
//     }
//     inner();
//   }
//   middle();
// }
// outer();

// fun outer() {
//   var x = "outer";
//   fun inner(){
//     print x;
//   }
//   inner();
// }
// outer();