var i = 0;
while(i < 3) {
  if(i == 2) {
    //
    break;
  }
  print "hello -----------------------";
  i = i + 1;
  var j = 0;
  while(j < 3) {
    if(j == 2) {
      //
      break;
    }
    print "hello -----------------------";
    j = j + 1;
  }
}

//
for(var j = 0; j < 3; j = j + 1) {
  if(j < 2) {
    print "for statement -----------------------";
  } else {
    break;
  }
  for(var j = 0; j < 3; j = j + 1) {
    if(j < 2) {
      print "for statement ----------------------";
    } else {
      break;
    }
  }
}
