var i = 0;
while (i < 3) : (i = i + 1) {
  if (i < 2) {
    continue;
  }
  print "hello -----------------------";

  var j= 0;
  while (j < 3) : (j = j + 1) {
    if (j < 2) {
      continue;
    }
    print "hello -----------------------";
  }
}

for(var j = 0; j < 3; j = j + 1) {
    if(j < 2) {
      continue;
    }
    print "for statement -----------------------";
}