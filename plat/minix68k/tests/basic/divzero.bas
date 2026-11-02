10 rem A division by zero is an EM trap here (software floating point),
20 rem not a signal, and ON ERROR catches it as error 8 (B1). Caught twice
30 rem (the trap handler is set again after each), then not caught.
40 on error goto 100
50 z = 0
60 print 1/z
70 print "not caught"
80 end
100 print "caught"; err; erl
110 on error goto 200
120 print 2/z
130 print "not caught"
140 end
200 print "caught again"; err; erl
210 print 3/z
220 print "not caught"
230 end
