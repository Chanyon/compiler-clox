// {
//   var a = 3;
//   fun foo(b) {
//     print b;
//   }
//   foo(1);
// }

var a = 2;
fun outer() {
  var x = 1;
  fun inner() {
    print a;
  }
  inner();
}
outer();
