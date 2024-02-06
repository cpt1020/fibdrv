set title "Fibonacci number time"
set xlabel "Fibonacci number"
set ylabel "time(ns)"
set terminal png enhanced font " Times_New_Roman,12 "
set output "fg_time.png"
set key left 
set grid

plot \
"time.txt" using 1:2 with linespoints linewidth 2 title "iterative", \
"time.txt" using 1:3 with linespoints linewidth 2 title "fast doubling", \
"time.txt" using 1:4 with linespoints linewidth 2 title "fast doubling with clz", \
