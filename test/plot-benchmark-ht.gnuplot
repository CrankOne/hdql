set terminal pdfcairo enhanced color size 32cm,24cm
set output "benchmark.pdf"

set multiplot layout 2,1 title "Container benchmark"

set logscale x
set logscale y
set grid

set title "Insertion time"
set xlabel "Number of entries"
set ylabel "Time [s]"

plot \
    'std_map.dat' using 1:2:3 with yerrorlines title 'std::map', \
    'std_unordered_map.dat' using 1:2:3 with yerrorlines title 'unordered map', \
    'hdql_ht.dat' using 1:2:3 with yerrorlines title 'hdql ht', \
    'hdql_vmap.dat' using 1:2:3 with yerrorlines title 'hdql vmap'

set title "Lookup time"
set xlabel "Number of entries"
set ylabel "Time [s]"

plot \
    'std_map.dat' using 1:4:5 with yerrorlines title 'std::map', \
    'std_unordered_map.dat' using 1:4:5 with yerrorlines title 'unordered map', \
    'hdql_ht.dat' using 1:4:5 with yerrorlines title 'hdql ht', \
    'hdql_vmap.dat' using 1:4:5 with yerrorlines title 'hdql vmap'

unset multiplot
set output
