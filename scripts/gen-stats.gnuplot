#!/usr/bin/env gnuplot

set terminal png size 1024,768
set output "/home/webapps/live/charts/p2k12-stats.png"

set xdata time
set timefmt "%s"
set xrange ["1333238400":]

set grid
unset border

set format x "%b %Y"

set title "P2K12 Statistics"

plot "/home/webapps/live/charts/data/purchases.txt" using 1:2 with lines title "Purchases", \
     "/home/webapps/live/charts/data/deficit.txt" using 1:2 with lines title "Inventory shrinkage"

###

reset

set terminal png size 1024,768
set output "/home/webapps/live/charts/stripe-stats.png"

set xdata time
set timefmt "%Y-%m-%d"
set yrange [0:]
set xrange ['2015-03-01':]
set autoscale xfix

set grid
unset border

set format x "%b %Y"

set title "Stripe payments 1 month moving average"
set ylabel "NOK/month"

plot "/home/webapps/live/charts/data/stripe-payments.txt" using 1:2 with lines notitle linecolor rgb "#ff0000" linewidth 2
