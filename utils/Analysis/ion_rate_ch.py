#!/usr/bin/python

# -*- coding: utf-8 -*-

"""Program to compute the cooling rate for CollideIon tests

There are two simple routines here.  The main routine that parses the
input command line and a plotting/parsing routine.

Examples:

	$ python ion_rate_ch -d 2 run2

Plots the temperature and cooling profile for the run with runtag
<run2> and therefore species file named <run2.species> using
polynomical fitting with degree 2.  To use a spline fit, specify the value of the smoothing parameter, i.e.

	$ python ion_rate_ch -s 10000000 run2

"""

import sys, getopt
import copy
import string
import numpy as np
import matplotlib.pyplot as pl
import scipy.interpolate as ip
import chianti.core as ch


def plot_data(filename, degree, smooth, tscale, tmax, dens):
    """Parse and plot the *.species output files generated by CollideIon 

    Parameters:

    filename (string): is the input datafile name

    degree (int): is the degree of the polynomial fitting function.
    If degree = 0, a spline fit is used

    smooth (real): is the smoothing parameter for the spline fit.

    """

    # Translation table to convert vertical bars and comments to spaces
    #
    trans = string.maketrans("#|", "  ")


    # Initialize data and header containers
    #
    tabl  = {}
    time  = []
    temp  = []
    etot  = []
    ncol  = 9
    head  = 2
    tail  = 2
    data  = {}

    # Species
    #
    spec  = ['H', 'H+', 'He', 'He+', 'He++']
    for v in spec: data[v] = {}

    # Read and parse the file
    #
    file  = open(filename)
    for line in file:
        if line.find('Time')>=0:    # Get the labels
            next = True
            labels = line.translate(trans).split()
            if line.find('W(') >= 0:
                ncol = 13
                head = 3
            if line.find('N(ie') >= 0:
                ncol = 16
                head = 3
            if line.find('EratC') >= 0 or line.find('Efrac') >= 0:
                tail = 12
        if line.find('[1]')>=0:     # Get the column indices
            toks = line.translate(trans).split()
            for i in range(head, len(toks)-tail):
                j = int(toks[i][1:-1]) - 1
                tabl[labels[j]] = i
                idx = (i-head) / ncol
                data[spec[idx]][labels[j]] = []
        if line.find('#')<0:        # Read the data lines
            toks = line.translate(trans).split()
            allZ = True             # Skip lines with zeros only
            for i in range(2, len(toks)):
                if float(toks[i])>0.0: 
                    allZ = False
                    break
            if not allZ:            
                # A non-zero line . . .  Make sure field counts are the
                # same (i.e. guard against the occasional badly written
                # output file
                if len(toks) == len(labels):
                    if float(toks[0]) <= tmax:
                        time.append(float(toks[0]))
                        temp.append(float(toks[1]))
                        etot.append(float(toks[-1]))
                        for i in range(head, len(toks)-tail):
                            idx = (i-head) / ncol
                            data[spec[idx]][labels[i]].append(float(toks[i]))
                else:
                    print "Bad line: toks=", len(toks), " labels=", len(labels)

    # Fields to plot
    #
    ekeys = ['E(ce)', 'E(ci)', 'E(ff)', 'E(rr)']                
    elabs = ['collide', 'ionize', 'free-free', 'recomb']

    tm = np.array(time)
    tp = np.array(temp)
    if degree>0:
        pf = np.polyfit(tm, tp, deg=degree)
        yf = np.polyval(pf, tm)
        pd = np.polyder(pf, m=1)
        zf = np.polyval(pd, tm)
    else:
        tck = ip.splrep(tm, tp, s=smooth)
        yf  = ip.splev(tm, tck)
        zf  = ip.splev(tm, tck, der=1)

    pl.subplot(2, 2, 1)
    pl.xlabel('Time (year)')
    pl.ylabel('Temperature')
    y = time
    for i in range(0, len(y)): y[i] *= tscale
    pl.plot(y, temp, '*', label='simulation')
    if degree>0:
        pl.plot(y, yf, '-', label='poly fit', linewidth=3)
    else:
        pl.plot(y, yf, '-', label='spline fit', linewidth=3)
    leg = pl.legend(loc='best',borderpad=0,labelspacing=0)
    leg.get_title().set_fontsize('6')
    pl.setp(pl.gca().get_legend().get_texts(), fontsize='12')

    pl.subplot(2, 2, 3)
    pl.xlabel('Time (year)')
    pl.ylabel('Slope')
    if degree>0:
        pl.plot(y, zf, '-', label='poly fit')
    else:
        pl.plot(y, zf, '-', label='spline fit')
    leg = pl.legend(loc='best',borderpad=0,labelspacing=0)
    leg.get_title().set_fontsize('6')
    pl.setp(pl.gca().get_legend().get_texts(), fontsize='12')

    Tmin = np.log10(yf.min())
    Tmax = np.log10(yf.max())
    delT = (Tmax - Tmin - 0.000001)/2000.0
    chT  = 10.0**np.arange(Tmin, Tmax, delT)
    rl   = ch.radLoss(chT, 1.0, minAbund=0.01)

    T_Ch = rl.RadLoss['temperature']
    R_Ch = rl.RadLoss['rate']
    k_B  = 1.3806504e-16
    yr5  = 365.25*24*3600*1e5
    R_Ch *= yr5/k_B * dens**2

    pl.subplot(1, 2, 2)
    pl.xlabel('Temperature')
    pl.ylabel('Slope')
    azf = -zf
    if degree>0:
        pl.plot(yf, azf, '-', label='poly fit')
    else:
        pl.plot(yf, azf, '-', label='spline fit')
    pl.plot(T_Ch, R_Ch, '-', label='LTE')
    pl.legend()

    pl.get_current_fig_manager().full_screen_toggle()
    pl.show()

def main(argv):
    """ Parse the command line and call the parsing and plotting routine """

    degree = 6
    smooth = 1
    tscale = 1.0e5
    tmax   = 1.0e10
    dens   = 0.1
    try:
        opts, args = getopt.getopt(argv,"hd:s:t:T:D:", ["deg=", "smooth=", "timescale=", "maxT=", "density="])
    except getopt.GetoptError:
        print sys.argv[0], '[-d <degree> | --deg=<degree> | -s <smooth> | --smooth=<smooth> | -t <timescale> | --timescale=<timescale> | -T <max time> | --maxT=<max time> | -D <density> | --density=<density>] <runtag>'
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print sys.argv[0], '[-d <degree> | --deg=<degree> | -s <smooth> | --smooth=<smooth> | -t <timescale> | --timescale=<timescale> | -T <max time> | --maxT=<max time>> | -D <density> | --density=<density>] <runtag>'
            sys.exit()
        elif opt in ("-d", "--deg"):
            degree = int(arg)
        elif opt in ("-s", "--smooth"):
            smooth = float(arg)
            degree = 0
        elif opt in ("-t", "--timescale"):
            tscale = float(arg)
        elif opt in ("-T", "--maxT"):
            tmax   = float(arg)
        elif opt in ("-D", "--density"):
            dens   = float(arg)

    suffix = ".ION_coll";
    if len(args)>0:
        filename = args[0] + suffix;
    else:
        filename = "run" + suffix;

    plot_data(filename, degree, smooth, tscale, tmax, dens)

if __name__ == "__main__":
   main(sys.argv[1:])
