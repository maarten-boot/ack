10 rem ON ERROR with LINE INPUT at the end of stdin (B3, B4)
20 on error goto 100
30 line input a$
40 print "not reached"
50 end
100 print "caught";err;erl
110 error 5
