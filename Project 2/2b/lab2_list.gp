# general plot parameters
set terminal png
set datafile separator ","

set xtics

### List-1

set title "List-1: Throughput vs Number of Threads"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_1.png'

# grep out mutex and spin list operations
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'mutex (raw)' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     	title 'spin-lock (raw)' with linespoints lc rgb 'green'	 

### List-2

set title "List-2: Wait-for-lock Time vs Number of Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Wait-for-lock time (ns)"
set logscale y 10
set output 'lab2b_2.png'
# note that unsuccessful runs should have produced no output
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
       title 'wait-for-lock time' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
       title 'average time per operation' with linespoints lc rgb 'blue'
 
### List-3

set title "List-3: Successful Iterations vs Number of Threads"
set logscale x 2
set xrange [0.75:]
set xlabel "Threads"
set ylabel "successful iterations"
set logscale y 10
set output 'lab2b_3.png'
plot \
     "< grep 'list-id-none,' lab2b_list.csv" using ($2):($3) \
	with points lc rgb "red" title "no synchronization (w/o yield)", \
     "< grep 'list-id-s,' lab2b_list.csv" using ($2):($3) \
        with points lc rgb "green" title "spin-lock (w/o yield)", \
     "< grep 'list-id-m,' lab2b_list.csv" using ($2):($3) \
        with points lc rgb "blue" title "mutex (w/o yield)"

### List-4

set title "List-4: Throughput vs Number of Threads, Mutex Synchronized"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_4.png'

plot \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'red' title "1 list", \
     "< grep 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'blue' title "4 lists", \
     "< grep 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'green' title "8 lists", \
     "< grep 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'purple' title "16 lists"

### List-5

set title "List-5: Throughput vs Number of Threads, Spin-lock Synchronized"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_5.png'

plot \
     "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'red' title "1 list", \
     "< grep 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'blue' title "4 lists", \
     "< grep 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'green' title "8 lists", \
     "< grep 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        with linespoints lc rgb 'purple' title "16 lists"

