set title "Fibonacci number time"
set xlabel "Fibonacci number"
set ylabel "time(ns)"
set terminal png enhanced font " Times_New_Roman,12 "
set output "fg_time_bignum.png"
set key left 
set grid

plot \
"time_bignum.txt" using 1:2 with linespoints linewidth 1.5 title "bignum\\\_decimal iterative", \
"time_bignum.txt" using 1:3 with linespoints linewidth 1.5 title "bignum\\\_bin iterative", \
"time_bignum.txt" using 1:4 with linespoints linewidth 1.5 title "bignum\\\_bin fast doubling", \
"time_bignum.txt" using 1:5 with linespoints linewidth 1.5 title "bignum\\\_bin fast doubling with clz", \
"time_bignum.txt" using 1:6 with linespoints linewidth 1.5 title "BIGNUM\\\_fast doubling with clz", \
