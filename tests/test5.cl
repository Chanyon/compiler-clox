var i = 0;
while (i < 3) {
  i = i + 1;
  if (i < 2) {
    continue;
  }
  var j = 0;
while (j < 3) {
  j = j + 1;
  if (j < 2) {
    continue;
  }
  print "j";
}
  print "i";
  // i = i + 1; (continue跳转情况：判断分配表达式变量名，如果与while比较变量相等，记录跳转位置，比较位置：如果con大于记录跳至loopStart,记录con位置,然后修正)
}