reset
set style function lines
set size   1.0, 1.0
set origin 0.0, 0.0
set multiplot layout 4, 1

# set border 0
set xtics out 
set ytics out
unset ytics

set key out 
set key bottom center
set key off

#set style data points
#set title "Allocation Addresses"
#set ytics off
plot \
     "/tmp/tm_alloc.log" using 1:2 title 'PTR' with points pointtype 7 pointsize .001
#replot
#set ytics on

#set title "Color Counts"
plot "/tmp/tm_alloc.log"  \
        using 1:3          title "WHITE"  with points pointtype 7 pointsize .001, \
     '' using 1:4          title "ECRU"   with points pointtype 7 pointsize .001, \
     '' using 1:5          title "GREY"   with points pointtype 7 pointsize .001, \
     '' using 1:6          title "BLACK"  with points pointtype 7 pointsize .001

plot "/tmp/tm_alloc.log" \
        using 1:7          title "TOTAL" with points pointtype 7 pointsize .001, \
     '' using 1:($7-$3)    title "INUSE" with points pointtype 7 pointsize .001

plot "/tmp/tm_alloc.log" \
        using 1:9          title "Free Blocks"      with points pointtype 7 pointsize .001, \
     '' using 1:8          title "Blocks"           with points pointtype 7 pointsize .001

#replot
unset multiplot

