var i = 0;
while(i < 3) {
  if(i == 1) {
    //! bug
    break;
  }
  print "hello";
  i = i + 1;
}

//! bug
for(var j = 0; j < 3; j = j + 1) {
  if(j < 2) {
    print "for statement";
  } else {
    break;
  }
}