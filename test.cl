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

// {
// var a = 2;
// fun tt(){
//   var a = 1;
//   print a;
// }
// tt();
// }

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
// print fib(5);
// print (clock() - start);

// 闭包
// var x = "global";
// fun outer() {
//   var y = "outer";
//   fun inner() {
//     y = x;
//     print y;
//   }
//   inner();
// }
// outer();

// fun makeClosure() {
//     var x = "local";
//     fun closure(){
//         print x;
//     }
//     return closure;
// }

// var closure = makeClosure();
// closure();

//! bug: undfined variable `value`
// == makeClosure ==
// 0000 line:100  opcode:OP_POP
// 0001 line:103  opcode:OP_CLOSURE       constant_idx:1 <fn closure>
// 0004   |     opcode:OP_CONSTANT      opcode_index:4 constant_index:17 "true"
// 0006   |     opcode:OP_ADD
// 0007   |     opcode:OP_RETURN
// 0008 line:105  opcode:OP_NIL
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

// var y = "123";
// print y;

// class Ani {}
// print Ani;
// print Ani();

// error case
// var obj = "not an instance";
// print obj.field;

// class Test {}
// var test = Test();
// test.foo = 1;
// test.bar = 2;
// print test.foo + test.bar;

// class Cat {
//     run() {
//         print 3;
//     }
// }
// print Cat;
// print Cat();

// class Brunch {
//     eggs() {
//         return 1;
//     }
// }

// var brunch = Brunch();
// var eggs = brunch.eggs;
// print eggs();

class Scone {
  init() { 
    this.foo = 2;
    fun bar(){
      print "not a method.";
    }
    this.bar = bar;
  }
  topping() {
    print "scone with " + first + " and " + second;
  }
  // bug !
  // topping(first , second) {
  //   print "scone with " + first + " and " + second;
  // }
  method() {
      // bug!
      // fun fn() {
      //     print this;
      // }
      // fn();
      return this;
  }
}

// var first = "berries";
// var second = "cream";
// var scone = Scone();
// scone.topping();
// print scone.foo;
// scone.bar();

class Child < Scone {
  test() {
    // bug!
    // opcode:OP_GET_GLOBAL    opcode_index:2 constant_index:1 "super"
    // undfined variable `super`.
    // super.topping();
    print 1;
  }
}
Child().test();