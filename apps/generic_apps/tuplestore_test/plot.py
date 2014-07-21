#!/usr/bin/env python3

# vim: set ts=4 sw=4 expandtab foldenable fdm=indent:
import sys
#sys.settrace()

import re
import matplotlib.pyplot as plt
import csv
from matplotlib import rc
import sys
import os
import os.path
import gzip
import itertools
import math
import matplotlib.ticker
import glob

sys.path.append('/home/henning/bin')
from experiment_utils import Situation, Repeat, ExperimentModel, t_average, band_stop

PLOT_ENERGY = True

#rc('font',**{'family':'serif','serif':['Palatino'], 'size': 6})
rc('font', family='serif',serif=['Palatino'], size=8)
rc('text', usetex=True)

EXP_DIR = 'experiments/'

# avg over 64 measurements,
# 3000 measurements per sec.
# measurement interval is in seconds

MEASUREMENT_INTERVAL = 64.0 / 3000.0

# mA = 
CURRENT_FACTOR = 70.0 / 4095.0

# mA * 3300mV = microJoule (uJ)
CURRENT_DISPLAY_FACTOR = 3300.0
# s -> ms
TIME_DISPLAY_FACTOR = 1000.0
TIME_DISPLAY_FACTOR_FIND = 1000.0


BASELINE_ENERGY = 0 # will be auto-calculated # 0.568206918430233

# Gateway hostname -> DB hostname
gateway_to_db = {
    # Convention: gateway nodes are odd, db nodes are even
    'inode015': 'inode016',
    'inode007': 'inode010',
    'inode011': 'inode014',
    'inode009': 'inode008',
    'inode019': 'inode018',
    'inode017': 'inode020',
    'inode023': 'inode022',
    'inode025': 'inode024',
    'inode027': 'inode026',
    'inode029': 'inode028',
    'inode031': 'inode030',
}

blacklist = []

# blacklist TS experiments before 24883, as they
# had the broken LE mode enabled
blacklist += [ {'job': str(i), 'database': 'tuplestore'} for i in range(24814, 24833) ]

# completely blacklisted jobs:
# sed -n "s/^\\s*{ 'job': '\([0-9]\+\)' }.*$/\1/p" < plot.py
#
blacklist += [
    { 'job': '24814', 'mode': 'find', 'database': 'antelope' },
    { 'job': '24814', 'inode_db': 'inode014', '_tmax': 3000 },
    { 'job': '24814', 'inode_db': 'inode010'},

    { 'job': '24815', 'mode': 'find', 'database': 'antelope' },
    { 'job': '24815', 'inode_db': 'inode010' },
    { 'job': '24815', 'inode_db': 'inode016', '_tmax': 3000 },
    { 'job': '24815', 'inode_db': 'inode014', '_valid': [(4000, 8000), (10000, 16000)] },
    { 'job': '24817', 'mode': 'find', 'database': 'antelope' },
    { 'job': '24817', 'inode_db': 'inode008', '_valid': [(0, 3000)] },
    { 'job': '24817', 'inode_db': 'inode014', '_valid': [(0, 1000), (4000, 8000)] },
    { 'job': '24817', 'inode_db': 'inode016', '_valid': [(0, 3000), (6000, 8000)] },
    { 'job': '24818', 'mode': 'find', 'database': 'antelope' },
    { 'job': '24818', 'inode_db': 'inode014', '_tmin': 2000 },
    { 'job': '24818', 'inode_db': 'inode008', '_tmax': 5000 },
    { 'job': '24819', 'inode_db': 'inode010' },
    { 'job': '24819', 'inode_db': 'inode016' },
    { 'job': '24819', 'inode_db': 'inode008', '_tmin': 2010 },
    { 'job': '24819', 'inode_db': 'inode014', '_tmin': 240, '_tmax': 3000 },
    { 'job': '24821', 'inode_db': 'inode010' }, # empty
    { 'job': '24821', 'inode_db': 'inode016' }, # some sub-runs are actually ok
    { 'job': '24821', 'inode_db': 'inode008' }, # some sub-runs are actually ok
    { 'job': '24822', 'inode_db': 'inode014' }, # empty
    { 'job': '24822', 'inode_db': 'inode016' }, # empty
    { 'job': '24822', 'inode_db': 'inode008' }, # 1 run only
    { 'job': '24822', 'inode_db': 'inode010', '_tmax': 600 }, # 1 run only
    { 'job': '24823' },
    { 'job': '24824' },
    { 'job': '24825', 'inode_db': 'inode016' },
    #{ 'job': '24824', 'inode_db': 'inode008' },
    { 'job': '24825', 'inode_db': 'inode010' },
    { 'job': '24841' },
    { 'job': '24842' },

    { 'job': '24857' }, # 1@once first teenylime experiments, different parameters
    { 'job': '24858' }, # 5@once ...
    { 'job': '24859' },
    { 'job': '24860', '_tmax': 20.0 },
    { 'job': '24861' }, # debug was on
    { 'job': '24862', '_tmax': 10.0 },
    # 24863: teeny insert, 4@once -- 3rd insert seems off / non-existing
    { 'job': '24864', '_tmax': 20.0 }, # teeny find, 1@once, 12 contained
    # 24865: teeny insert, 4@once -- 3rd insert seems off /nonexisting
    { 'job': '24865', '_tmax': 20.0 },
    { 'job': '24867', 'inode_db': 'inode010' }, # teeny erase
    { 'job': '24867', 'inode_db': 'inode014' }, # teeny erase
    { 'job': '24867', 'inode_db': 'inode016' }, # teeny erase
    { 'job': '24867', 'inode_db': 'inode008', '_tmax': 20.0 }, # teeny erase
    { 'job': '24868', 'inode_db': 'inode016' }, # teeny erase
    { 'job': '24868', 'inode_db': 'inode010' }, # teeny erase
    { 'job': '24868', '_tmax': 20.0, '_alpha': .04 }, # teeny erase
    { 'job': '24871' }, # 24871: ts erase, aborted
    { 'job': '24872' }, # 24872: ts erase, failed
    { 'job': '24873' }, # 24873: ts erase, failed
    { 'job': '24874' }, # 24873: ts erase, failed
    { 'job': '24875' }, # 24873: ts erase, failed
    { 'job': '24877', 'inode_db': 'inode014', '_tmin': 780, '_tmax': 900 }, # ts erase
    { 'job': '24877', 'inode_db': 'inode008', '_tmin': 790, '_tmax': 900 }, # ts erase
    { 'job': '24877', 'inode_db': 'inode010'}, # ts erase
    { 'job': '24877', 'inode_db': 'inode016'}, # ts erase
    { 'job': '24879', '_tmin': 780, '_tmax': 900 }, # ts erase
    { 'job': '24880', 'inode_db': 'inode010', '_tmin': 760, '_tmax': 1000, }, # ts erase
    { 'job': '24880', 'inode_db': 'inode016'}, # ts erase
    { 'job': '24880', 'inode_db': 'inode008', '_tmin': 786, '_tmax': 1000, }, # ts erase
    { 'job': '24880', 'inode_db': 'inode014'}, # ts erase
    { 'job': '24882', 'inode_db': 'inode014'}, # ts find
    { 'job': '24882', 'inode_db': 'inode010'}, # ts find
    { 'job': '24882' }, # only blacklisted because it has FINDS_AT_ONCE=1
    # 24882: ts find FINDS_AT_ONCE=1
    { 'job': '24883', 'inode_db': 'inode008'},
    { 'job': '24884', 'inode_db': 'inode008'},
    { 'job': '24885', 'inode_db': 'inode008'},
    { 'job': '24885', 'inode_db': 'inode010'}, #, '_tmin': 530, '_threshold': 1.3},
    { 'job': '24885', 'inode_db': 'inode014', '_tmin': 545, '_threshold': 2.0, '_alpha': .04},
    { 'job': '24885', 'inode_db': 'inode016', '_tmin': 520, '_threshold': 1.5, '_alpha': .04},
    { 'job': '24886', 'inode_db': 'inode010'},
    { 'job': '24886', 'inode_db': 'inode014'},
    { 'job': '24886', 'inode_db': 'inode016'},
    { 'job': '24886', 'inode_db': 'inode008', '_tmin': 655, '_threshold': 2.0, '_alpha': .04},
    { 'job': '24887', 'inode_db': 'inode016'},
    { 'job': '24887', 'inode_db': 'inode008', '_tmin': 740, '_threshold': 2.0, '_alpha': .04},
    { 'job': '24887', 'inode_db': 'inode014', '_tmin': 740, '_threshold': 2.0, '_alpha': .04},
    { 'job': '24887', 'inode_db': 'inode010', '_tmin': 740, '_threshold': 2.0, '_alpha': .04},
    { 'job': '24898' }, # ts ins with set_vector that checks for presence first

    { 'job': '24899' }, # did several code changes / fixes in this period
    { 'job': '24900' }, 
    { 'job': '24901' }, 
    { 'job': '24902' }, 
    { 'job': '24903' }, 
    { 'job': '24904' }, 
    { 'job': '24905' }, 
    { 'job': '24906' }, 
    { 'job': '24907' }, 

    { 'job': '24909', 'inode_db': 'inode016' }, 
    { 'job': '24910', 'inode_db': 'inode008', '_tmax': 425 }, 
    { 'job': '24910', 'inode_db': 'inode014', '_tmax': 500 }, 
    { 'job': '24910', 'inode_db': 'inode016', '_tmax': 800 }, 
    { 'job': '24911' },

    # debugged & improved ts static dict
    # 24990 & 14991 ts erase
    { 'job': '24990', 'inode_db': 'inode008', '_tmin': 610, '_tmax': 665,   '_mode': 'state', '_threshold': 1.5, '_alpha': .04 },
    { 'job': '24990', 'inode_db': 'inode010', '_tmin': 610, '_tmax': 658,   '_mode': 'state', '_threshold': 1.5, '_alpha': .04 },
    { 'job': '24990', 'inode_db': 'inode014', '_tmin': 610, '_tmax': 662,   '_mode': 'state', '_threshold': 1.5, '_alpha': .04 },
    { 'job': '24990', 'inode_db': 'inode016', '_tmin': 600, '_tmax': 652.4, '_mode': 'state', '_threshold': 1.5, '_alpha': .04 },

    { 'job': '24991', 'inode_db': 'inode008', '_tmin': 590, '_tmax': 645,   '_mode': 'state', '_threshold': 1.5, '_alpha': .04 },
    { 'job': '24991', 'inode_db': 'inode010', '_tmin': 590, '_tmax': 644.2, '_mode': 'state', '_threshold': 1.6, '_alpha': .04 },
    { 'job': '24991', 'inode_db': 'inode014', '_tmin': 595, '_tmax': 644,   '_threshold': 1.75, '_alpha': .04 },
    { 'job': '24991', 'inode_db': 'inode016', '_tmin': 585, '_tmax': 635.5, '_mode': 'state', '_threshold': 1.2, '_alpha': .04 },
    # 24992 ts insert
    # 24993 ts insert
    { 'job': '24994' }, # 24994 ts find that still had debug msgs in start_find()
    # 24995 ts find
    
    { 'job': '24768' },
    { 'job': '24776' },
    { 'job': '24775' },
    { 'job': '24826' },
    { 'job': '24827' },
    { 'job': '24828' },
    { 'job': '24829' },
    { 'job': '24832' },
    { 'job': '24833' },
    { 'job': '24834' },
    { 'job': '24835' },
    { 'job': '24836' },
    { 'job': '24838' },
    { 'job': '24839' },
    { 'job': '24840' },
    { 'job': '24843' },
    { 'job': '24844' },
    { 'job': '24849' },
    { 'job': '24850' },
    { 'job': '24851' },
    { 'job': '24851', 'inode_db': 'inode016' },
    { 'job': '24851', 'inode_db': 'inode010' },
    { 'job': '24854' },
    { 'job': '24854' },
    { 'job': '24860' },

    { 'job': '25012' }, # ts erase, energy measurement not turned on

    # Here discovered bugs in bitmap allocator and static dict
    # that make all former ts/erase measurements invalid!
    # (see according filter below)

    { 'job': '25064', 'inode_db': 'inode010', '_tmin': 735 },
    { 'job': '25064', 'inode_db': 'inode014', '_tmin': 740 },
    { 'job': '25064', 'inode_db': 'inode008', '_tmin': 725 },
    { 'job': '25064', 'inode_db': 'inode016' },
    #{ 'job': '25065', '_tmin': 0, '_alpha': .02 },
    { 'job': '25066', 'inode_db': 'inode010', '_tmin': 520 },
    { 'job': '25066', 'inode_db': 'inode014', '_tmin': 520 },
    { 'job': '25066', 'inode_db': 'inode008', '_tmin': 520 },
    { 'job': '25066', 'inode_db': 'inode016' },
    { 'job': '25085', 'inode_db': 'inode016' },

    { 'job': '25103', 'inode_db': 'inode010', '_tmin': 590 },
    { 'job': '25103', 'inode_db': 'inode008' },
    { 'job': '25103', 'inode_db': 'inode014' },
    { 'job': '25103', 'inode_db': 'inode016' },

    { 'job': '25104'   , 'inode_db': 'inode010'   , '_tmin': 496 , '_tmax': 541.5 , '_mode': 'state' , '_threshold': 1.25 , '_alpha': 1.0 } , 
    { 'job': '25104'   , 'inode_db': 'inode014'   , '_tmin': 495 , '_tmax': 541   , '_mode': 'state' , '_threshold': 1.5  , '_alpha': 1.0 } , 
    { 'job': '25104'   , 'inode_db': 'inode008'   , '_tmin': 490 , '_tmax': 540   , '_mode': 'state' , '_threshold': 1.4  , '_alpha': 1.0 } , 
    { 'job': '25104'   , 'inode_db': 'inode016' } , 

    { 'job': '25123' } , 
    { 'job': '25125' } , 
    { 'job': '25126' } , 
    { 'job': '25143'   , 'inode_db': 'inode008'   , '_tmin': 435 , '_tmax': 445   , '_mode': 'state' , '_threshold': 1.1  , '_alpha': 1.0 } , 
    { 'job': '25143'   , 'inode_db': 'inode014'   , '_tmin': 435 , '_tmax': 444   , '_mode': 'state' , '_threshold': 1.1  , '_alpha': 1.0 } , 
    { 'job': '25143'   , 'inode_db': 'inode010'   , '_tmin': 440 , '_tmax': 452   , '_mode': 'state' , '_threshold': 1.1  , '_alpha': 1.0 } , 

    { 'job': '25144'   , 'inode_db': 'inode008'   , '_tmin': 381 , '_tmax': 391   , '_mode': 'state' , '_threshold': 1.1  , '_alpha': 1.0 } , 
    { 'job': '25144'   , 'inode_db': 'inode014'   , '_tmin': 385 , '_tmax': 397.5 , '_mode': 'state' , '_threshold': 1.1  , '_alpha': 1.0 } , 
    { 'job': '25144', 'inode_db': 'inode010' }, # '_tmin': 390, '_tmax': 403.5, '_mode': 'state', '_threshold': 1.3, '_alpha': .02 },

    { 'job': '25145', '_tmin': 290, '_tmax': 310, '_alpha': 1.0 }, # ts/tree erase
    { 'job': '25146' } , 
    { 'job': '25164', 'inode_db': 'inode008', '_tmin': 465, '_tmax': 487.5, '_alpha': 1.0 },
    { 'job': '25164', 'inode_db': 'inode014', '_tmin': 465, '_tmax': 486, '_alpha': 1.0 },

    { 'job': '25186', '_tmax': 420 },

    # ts tree erase
    { 'job': '25193' }, # debug run
    { 'job': '25196' }, # debug run
    { 'job': '25197', 'inode_db': 'inode008', '_tmin': 330, '_tmax': 350, '_alpha': 1.0 },
    { 'job': '25197', 'inode_db': 'inode010' }, # too noisy
    { 'job': '25197', 'inode_db': 'inode014', '_tmin': 340, '_tmax': 360, '_alpha': 1.0 },
    { 'job': '25197', 'inode_db': 'inode016', '_tmin': 320, '_tmax': 340, '_alpha': 1.0 },

    # ts avl erase
    { 'job': '25209', 'inode_db': 'inode010', }, #'_tmin': 500, '_alpha': .04 }, noisy
    { 'job': '25209', 'inode_db': 'inode030', }, #'_tmin': 310, '_alpha': 1.0 }, # noisy?
    { 'job': '25209', 'inode_db': 'inode008', '_tmin': 320, '_alpha': 1.0 },
    { 'job': '25209', 'inode_db': 'inode020', }, #'_tmin': 320, '_alpha': 1.0 }, # seems noisy

    { 'job': '25210', 'inode_db': 'inode010', '_tmin': 315, '_alpha': 1.0 },
    { 'job': '25210', 'inode_db': 'inode008', '_tmin': 315, '_alpha': 1.0 },
    { 'job': '25210', 'inode_db': 'inode020', }, # '_tmin': 305, '_alpha': 1.0 }, # noisy
    { 'job': '25210', 'inode_db': 'inode030', }, # '_tmin': 310, '_alpha': 1.0 }, # noisy
    { 'job': '25215', 'inode_db': 'inode008', '_tmin': 410 }, 
    { 'job': '25215', 'inode_db': 'inode010', '_tmin': 410 }, 
    { 'job': '25215', 'inode_db': 'inode020', '_tmin': 410 }, 
    { 'job': '25215', 'inode_db': 'inode030', '_tmin': 410 }, 

    { 'job': '26355_00', 'inode_db': 'inode018', '_tmin': 620, '_alpha': 0.5 },
    { 'job': '26361', 'inode_db': 'inode018', '_tmin': 600, '_alpha': .01 },

    #{ 'job': '26349': 'inode_db': 'inode008', '_tmin': 435 },
]


teenylime_runs = set(
    [str(x) for x in range(24857, 24871)]
)

subsample_runs = set([
    '24877', # ts erase
    '24879', # ts erase
    '24880', # ts erase

    '24882', # ts find (1)
    '24885', # ts erase
    '24886', # ts erase
    '24887', # ts erase

    #'24818', # ts find (10)
    #'24819', # ts find (10)

    '24990', # ts erase
    '24991', # ts erase

    '25064', # ts erase
    '25066', # ts erase

    '25103', # ts erase
    '25104', # ts erase

    #'25125', # ts/tree erase
    '25143', # ts/tree erase
    '25144', # ts/tree erase
    '25145', # ts/tree erase
    '25164', # ts/avl erase

    '25197', # ts/tree erase
    '25209', # ts/avl erase
    '25210', # ts/avl erase
    '25215', # ts/prescilla erase
    #'26349', # ts/chopper find
    #'26351',
    '26355',
    '26361',
])

TEENYLIME_INSERT_AT_ONCE = 4

# There should actually 3, but it seems the 3 doest cost any measurable energy
# for some reason (although according to the one-at-a-time experiments 12
# tuples should fit)
TEENYLIME_INSERT_CALLS = 2

FINDS_AT_ONCE = 10
TEENYLIME_FINDS_AT_ONCE = 1
TEENYLIME_ERASES_AT_ONCE = 1

# ( Experiment class , { 'ts': [...], 'vs': [...], 'cls': cls } )
experiments = []


def get_style(k):
    c = 'black'
    l = '-'

    if k.database == 'tuplestore':
        #c = '#ddccaa'
        c = 'black'
        l = '-'
        if k.ts_dict == 'tree':
            c = '#88bbbb'
            l = '-'
        elif k.ts_dict == 'avl':
            c = '#bbaa88'
            l = '-'
    elif k.database == 'teeny':
        c = '#dd7777'
        l = '-'

    return {
        'plot': {
            'linestyle': l,
            'color': c,
        },
        'box': {
            'linestyle': l,
            'color': c,
        }
    }

def style_box(bp, k):
    s = get_style(k)['box']
    for key in ('boxes', 'whiskers', 'fliers', 'caps', 'medians'):
        plt.setp(bp[key], **s)
    plt.setp(bp['fliers'], marker='+')

def main():

    def mklabel(k):
        if k.database == 'tuplestore':
            return tex('ts-' + k.ts_container + '-' + k.ts_dict)
        return tex(k.database)

    def select_exp(k):
        if k.database == 'antelope': return False

        # Ignore experiments on old TS/static dict code
        #if k.database == 'tuplestore' and int(k.job) < 25064:
        if k.database == 'tuplestore' and int(k.job.split('_')[0]) < 25085:
            return False

        if k.database == 'tuplestore' and k.ts_dict == 'tree' and int(k.job.split('_')[0]) <= 25143:
            return False

        if k.database == 'tuplestore' and k.ts_dict == 'prescilla':
            return False

        # Ignore ERASE experiments on buggy TS/static dict and bitmap
        # allocator code
        #if k.database == 'tuplestore' and k.mode == 'erase' and int(k.job) < 25064:
            #return False
        return True

    process_directories(
        [x.strip('/') for x in sys.argv[1:]],
        select_exp
        #lambda k: k.database != 'antelope'
        #lambda k: k.mode == 'find' #and k.database != 'antelope'

         #filter
        #lambda k: k.mode == 'find' or k.mode == 'erase' # and k.database != 'teeny') #or k.mode == 'insert'
        #lambda k: k.mode == 'insert'
    )
    
    #fs = (12, 5)
    #fs = (4, 3)
    fs = (8, 3)
    #fs = (4 * 0.8, 3 * 0.8)
    #fs = None
    
    fig_i_e = plt.figure(figsize=fs)
    ax_i_e = plt.subplot(111)
    fig_i_t = plt.figure(figsize=fs)
    ax_i_t = plt.subplot(111)

    fig_f_e = plt.figure(figsize=fs)
    ax_f_e = plt.subplot(111)
    fig_f_t = plt.figure(figsize=fs)
    ax_f_t = plt.subplot(111)

    fig_e_e = plt.figure(figsize=fs)
    ax_e_e = plt.subplot(111)
    fig_e_t = plt.figure(figsize=fs)
    ax_e_t = plt.subplot(111)

    #for ax in (ax_f_e, ax_f_t, ax_i_e, ax_i_t, ax_e_e, ax_e_t):
        #ax.set_yscale('log')
        #ax.yaxis.set_major_formatter(matplotlib.ticker.ScalarFormatter())

    shift_i = 0
    shift_f = 0
    shift_e = 0
    #l_f = 1
    #l_e = 1
    for k, exp in experiments: #.items():

        es = [[y * CURRENT_DISPLAY_FACTOR for y in x] for x in exp.energy]

        if k.mode == 'find':
            ts = []
            for v in exp.time:
                t_ = []
                for w in v: t_.append(w * TIME_DISPLAY_FACTOR_FIND)
                ts.append(t_)
        else:
            ts = [[y * TIME_DISPLAY_FACTOR for y in x] for x in exp.time]

        if k.mode == 'insert':
            pos_e = exp.tuplecounts[:len(exp.energy)]
            pos_t = exp.tuplecounts[:len(exp.time)]

            # If for some weird reason we have more data points than
            # sent out packets, put the rest at pos 100 so we notice something
            # is up (but still can see most of the values)
            pos_e += [100] * (len(exp.energy) - len(pos_e))
            pos_t += [100] * (len(exp.time) - len(pos_t))

            pos_e, es = cleanse(pos_e, es)
            pos_t, ts = cleanse(pos_t, ts)

            pos_e = [x for x in pos_e if x <= k.ntuples]
            pos_t = [x for x in pos_t if x <= k.ntuples]
            es = es[:len(pos_e)]
            ts = ts[:len(pos_t)]

            w = 2.5 if k.database == 'antelope' else 3.5
            if len(es):
                bp = ax_i_e.boxplot(es, positions=pos_e, widths=w)
                style_box(bp, k)
                ax_i_e.plot(pos_e, [median(x) for x in es], label=mklabel(k), **get_style(k)['plot'])
                #ax_i_e.plot(pos_e, [median(x) for x in es],  label=mklabel(k))

            if len(ts):
                bp = ax_i_t.boxplot(ts, positions=pos_t, widths=w)
                style_box(bp, k)

                #ax_i_t.plot(pos_t, [median(x) for x in ts],style[k.database]['ls'], label=mklabel(k))
                ax_i_t.plot(pos_t, [median(x) for x in ts], label=mklabel(k), **get_style(k)['plot'])

            shift_i -= 1

        elif k.mode == 'find':
            if k.database == 'teeny':
                pos_e = [12] #* len(exp.energy)
                pos_t = [12] #* len(exp.time)
                es = [flatten(es)]
                ts = [flatten(ts)]
            else:
                pos_e = exp.tuplecounts[:len(exp.energy)]
                pos_t = exp.tuplecounts[:len(exp.time)]

                # If for some weird reason we have more data points than
                # sent out packets, put the rest at pos 100 so we notice something
                # is up (but still can see most of the values)
                pos_e += [100] * (len(exp.energy) - len(pos_e))
                pos_t += [100] * (len(exp.time) - len(pos_t))

            pos_e, es = cleanse(pos_e, es)
            pos_t, ts = cleanse(pos_t, ts)

            pos_e = [x for x in pos_e if x <= k.ntuples]
            pos_t = [x for x in pos_t if x <= k.ntuples]
            es = es[:len(pos_e)]
            ts = ts[:len(pos_t)]

            #print(k.mode, k.database, [len(x) for x in es])
            #print(pos_e, [sum(x)/len(x) for x in es])

            if len(es):
                bp = ax_f_e.boxplot(es, positions=pos_e, widths=3)
                style_box(bp, k)
                ax_f_e.plot(pos_e, [median(x) for x in es], label=mklabel(k), **get_style(k)['plot'])

            if len(ts):
                bp = ax_f_t.boxplot(ts, positions=pos_t, widths=3)
                style_box(bp, k)
                ax_f_t.plot(pos_t, [median(x) for x in ts], label=mklabel(k), **get_style(k)['plot'])

            shift_f -= 1

        elif k.mode == 'erase':
            #if k.database == 'antelope': continue
            if k.database == 'teeny':
                k.ntuples = 12

            pos_e = [x for x in exp.tuplecounts[:len(exp.energy)]]
            pos_t = [x for x in exp.tuplecounts[:len(exp.time)]]

            # If for some weird reason we have more data points than
            # sent out packets, put the rest at pos 100 so we notice something
            # is up (but still can see most of the values)
            pos_e += [100] * (len(exp.energy) - len(pos_e))
            pos_t += [100] * (len(exp.time) - len(pos_t))

            pos_e, es = cleanse(pos_e, es)
            pos_t, ts = cleanse(pos_t, ts)

            # positions count from 0 upwards, actually what we want is count
            # downwards from k.ntuples, fix that here
            pos_e = [k.ntuples - x for x in pos_e]
            pos_t = [k.ntuples - x for x in pos_t]

            #print("runs {} {} {}".format(k.mode, mklabel(k), [len(x) for x in es]))
            #print(pos_e, [median(x) for x in es])

            if len(es):
                bp = ax_e_e.boxplot(es, positions=pos_e)
                style_box(bp, k)

                ax_e_e.plot(pos_e, [median(x) for x in es], label=mklabel(k), **get_style(k)['plot'])

            if len(ts):
                bp = ax_e_t.boxplot(ts, positions=pos_t)
                style_box(bp, k)

                ax_e_t.plot(pos_t, [median(x) for x in ts], label=mklabel(k), **get_style(k)['plot'])

            shift_e -= 1
            
        print("runs {} {} {}".format(k.mode, mklabel(k), [len(x) for x in es]))
        #print(k.mode, k.database, k.ts_dict, [len(x) for x in es], 'at', pos_e)


    ax_i_e.set_xticks(range(0,100,5))
    ax_i_e.set_xlim((0, 75))
    #ax_i_e.set_ylim((0, 55))
    ax_i_e.set_xlabel(r"\#tuples inserted")
    ax_i_e.set_ylabel(r"$\mu J$ / insert")

    ax_i_t.set_xticks(range(0,100,5))
    ax_i_t.set_xlim((0, 75))
    ax_i_t.set_ylim((0, 12))
    ax_i_t.set_xlabel(r"\#tuples inserted")
    ax_i_t.set_ylabel(r"ms / insert")

    ax_f_e.set_xticks(range(0,100,5))
    ax_f_e.set_xlim((0, 75))
    ax_f_e.set_ylim((0, 25))
    ax_f_e.set_xlabel(r"\#tuples stored")
    ax_f_e.set_ylabel(r"$\mu J$ / find")

    ax_f_t.set_xticks(range(0,100,5))
    ax_f_t.set_xlim((0, 75))
    ax_f_t.set_ylim((0, 18))
    ax_f_t.set_xlabel(r"\#tuples stored")
    ax_f_t.set_ylabel(r"ms / find")
    
    ax_e_e.set_xticks(range(0,100,5))
    ax_e_e.set_xlim((0, 75))
    #ax_e_e.set_ylim((0, 250))
    ax_e_e.set_ylim((0, 70))
    ax_e_e.set_xlabel(r"\#tuples in store")
    ax_e_e.set_ylabel(r"$\mu J$ / erase")

    ax_e_t.set_xticks(range(0,100,5))
    ax_e_t.set_xlim((0, 75))
    ax_e_t.set_ylim((0, 25))
    ax_e_t.set_xlabel(r"\#tuples erased")
    ax_e_t.set_ylabel(r"ms / erase")

    ax_i_e.grid()
    ax_i_t.grid()
    ax_f_e.grid()
    ax_f_t.grid()
    ax_e_e.grid()
    ax_e_t.grid()
    #ax_i_e.legend()
    #ax_i_t.legend()
    #ax_f_e.legend(loc='lower right')
    #ax_f_t.legend()
    #ax_e_e.legend()
    #ax_e_t.legend(loc='lower right')

    fig_i_e.savefig('pdf_out/energies_insert.pdf', bbox_inches='tight', pad_inches=0.1)
    fig_i_t.savefig('pdf_out/times_insert.pdf', bbox_inches='tight',pad_inches=0.1)
    fig_f_e.savefig('pdf_out/energies_find.pdf', bbox_inches='tight',pad_inches=0.1)
    fig_f_t.savefig('pdf_out/times_find.pdf', bbox_inches='tight',pad_inches=0.1)
    fig_e_e.savefig('pdf_out/energies_erase.pdf', bbox_inches='tight',pad_inches=0.1)
    fig_e_t.savefig('pdf_out/times_erase.pdf', bbox_inches='tight',pad_inches=0.1)

def process_directories(dirs,f=lambda x: True):
    for d in dirs:
        process_directory(d, f)

def process_directory(d, f=lambda x: True):
    d = str(d).strip('/')

    dirname = EXP_DIR + d.split('_')[0]
    teenylime = (d in teenylime_runs or dirname in teenylime_runs)
    subsample = (d in subsample_runs or dirname in subsample_runs)

    print()
    print("*** iMinds exp #{} {} ***".format(d, '(teenylime mode)' if teenylime else ''))

    #
    # Blacklisting & Filtering of experiments
    #

    # blacklist by db inode id
    bl = {}
    all_invalid = True
    for gw, db in gateway_to_db.items():
        fn_gwinfo = dirname + '/' + gw + '/' + gw + '.vars'
        fn_gwout = dirname + '/' + gw + '/output.txt'
        if not os.path.exists(fn_gwinfo):
            print("{} not found, ignoring that area.".format(fn_gwinfo))
            continue

        v = read_vars(fn_gwinfo)
        cls = ExperimentClass(v)
        cls.job = d

        if cls.database == 'teeny' and not teenylime:
            print("  ({}: {}/{} {} ignored: not a teenylime run)".format(db, cls.database, cls.mode, cls.dataset))
            bl[db] = { 'inode_db': db }
            continue

        if not f(cls):
            print("  ({}: {}/{} {} ignored by filter)".format(db, cls.database, cls.mode, cls.dataset))
            bl[db] = { 'inode_db': db }
            continue

        for b in blacklist:
            for k, x in b.items():
                if k.startswith('_'): continue
                if hasattr(cls, k) and (getattr(cls, k) != x and getattr(cls, k).split('_')[0] != x and getattr(cls, k) != x.split('_')[0]):
                    # does not match this blacklist entry
                    all_invalid = False
                    break
            else:
                bl[db] = b
                if has_valid(b):
                    all_invalid = False
                else:
                    print("  ({}: {}/{} {} ignored by blacklist)".format(db, cls.database, cls.mode, cls.dataset))
                break

    if all_invalid:
        print("  (ignoring {} completely)".format(d))
        return

    # Now actually read energy values
    # we only need subsamples for teenylime as they only insert one value at a
    # time
    #

    es = []
    
    # if there are files named 12345_00.csv, we have splitted csvs
    #splitted_csvs = glob.glob(EXP_DIR + d + '_*')
    #if splitted_csvs:
        #print("  using {} splitted chunks".format(len(splitted_csvs)))
        #for fn in splitted_csvs[1:2]:
            #energy = read_energy(fn, bl, use_subsamples=teenylime or subsample, alpha=(.05 if (teenylime or subsample) else 1.0))
            #procenergy(energy, d, gw, db, bl, teenylime)
            ##es.append(energy)

    #else:
    for ext in ('.csv.gz', '.csv'):
        fn = EXP_DIR + d + ext
        if os.path.exists(fn):
            energy = read_energy(fn, bl, use_subsamples=teenylime or subsample, alpha=(.05 if (teenylime or subsample) else 1.0))
            #es.append(energy)
            procenergy(energy, d, gw, db, bl, teenylime)
            break

def procenergy(energy, d, gw, db, bl, teenylime):
    print("x")

    dirname = EXP_DIR + d.split('_')[0]
    #
    # Process the energy measurements of each node
    #
    for gw, db in gateway_to_db.items():
        print("y")
        
        # Read in some metadata and decide whether we want to look at this at
        # all
        
        if db in bl and not has_valid(bl[db]): continue
        fn_gwinfo = dirname + '/' + gw + '/' + gw + '.vars'
        fn_gwout = dirname + '/' + gw + '/output.txt'
        if not os.path.exists(fn_gwinfo):
            print("{} not found, ignoring that area.".format(fn_gwinfo))
            continue

        print()
        print("processing: {} -> {}".format(gw, db))

        v = read_vars(fn_gwinfo)
        cls = ExperimentClass(v)
        cls.job = d
        #if not f(cls):
            #print("({} ignored by filter)".format(db))
            #continue

        print("  {}{}/{} {}".format(cls.database, '.' + cls.ts_dict if cls.database == 'tuplestore' else '', cls.mode, cls.dataset))

        #
        # Add experiment object,
        # find tuplecounts, i.e. values for the x-axis
        #

        exp = add_experiment(cls)
        if teenylime:
            if v['mode'] == 'insert':
                tc = [TEENYLIME_INSERT_AT_ONCE] * 50 # range(20)
            elif v['mode'] == 'find':
                tc = [TEENYLIME_FINDS_AT_ONCE] * 50 # range(20)
            else:
                tc = [TEENYLIME_ERASES_AT_ONCE] * 50 # range(20)
        else:
            if v['mode'] == 'erase' and v['database'] == 'tuplestore':
                tc = [1] * 100
            else:

                tc = parse_tuple_counts(open(fn_gwout, 'r', encoding='latin1'), d)
                if not tc:
                    print("  (!) no tuplecounts found in {}, using default".format(fn_gwout))
                    tc = [7, 6, 8, 11, 11, 10, 11, 9]
        exp.set_tuplecounts(tc)
        mid = inode_to_mote_id(db)
        if mid not in energy:
            print("  (!) no energy values for {}. got values for {}".format(mid, str(energy.keys())))
            continue

        #
        # Process energy measurements into runs_t and runs_e
        #
        
        delta = None
        if teenylime:
            runs_t, runs_e, runs_ot = process_energy_teenylime(energy[mid], v['mode'], lbl=db, tmin=bl.get(db, {}).get('_tmin', 0))
            delta = 1.0

            if v['mode'] == 'insert':
                runs_t = [r[:TEENYLIME_INSERT_CALLS] for r in runs_t]
                runs_e = [r[:TEENYLIME_INSERT_CALLS] for r in runs_e]

        else:
            if v['mode'] == 'erase':
                runs_t, runs_e, runs_ot = process_energy_ts_erase(energy[mid], v['mode'], lbl=db, tmin=bl.get(db, {}).get('_tmin', 0),
maxvalues=v['ntuples'],bl=bl)
                delta = 0.6
            else:
                runs_t, runs_e, runs_ot = process_energy(energy[mid], v['mode'], lbl=db, tmin=bl.get(db, {}).get('_tmin', 0))
                delta = 60.0

        assert delta is not None

        #
        # Fix observation times for plausibility
        #
        #for rt, re, rot in zip(runs_t, runs_e, runs_ot):
        for i in range(len(runs_ot)):
            rt = runs_t[i]
            re = runs_e[i]
            rot = runs_ot[i]
            runs_t[i], runs_e[i], _ = plausible_observation_times(rt, re, rot, delta)


        #
        # Add the extracted measurements to the experiment object
        #

        runs_count = 0
        for j, (ts, es) in enumerate(zip(runs_t, runs_e)):
            runs_count += 1
            print("  adding run {} of {} with {} entries".format(j, len(runs_e), len(es)))
            #print("  ts={} es={}".format(ts, es))
            for i, (t, e) in enumerate(zip(ts, es)):
                if t is not None and e is not None: #t != 0 or e != 0:
                    exp.add_measurement(i, t, e)
                else:
                    print("  skipping i={} t={} e={}".format(i, t, e))
                    #pass
        print("  processed {} experiment runs.".format(runs_count))

#
# Reading data
#

def read_vars(fn):
    r = {}
    for line in open(fn, 'r'):
        k, v = line.split('=')
        k = k.strip()
        v = v.strip()
        k = k.lower()
        if v.startswith('"'):
            r[k] = v[1:-1]
        else:
            r[k] = int(v)
    if 'database' not in r:
        r['database'] = 'db'

    return r

def read_energy(fn, bl, use_subsamples=False, alpha=0.05):
    r = {}

    print("    {}".format(fn))

    f = None
    if fn.endswith('.csv.gz'):
        f = gzip.open(fn, 'rt', encoding='latin1')
    else:
        f = open(fn, 'r')

    #if os.path.exists(fn + '.csv.gz'):
        #f = gzip.open(fn + '.csv.gz', 'rt', encoding='latin1')
    #else:
        #f = open(fn + '.csv', 'r')

    #reader = csv.DictReader(f, delimiter=';', quotechar='"')
    reader = csv.DictReader(f, delimiter='\t', quotechar='"')
    #t = {}
    t = {}
    tmax = 0
    rownum = 0
    for row in reader:
        #print(rownum)
        rownum += 1
        #print("    {}".format(row))
        mote_id = row['motelabMoteID']
        if mote_id not in r: r[mote_id] = {'ts':[], 'vs':[]}
        b = bl.get(mote_id_to_inode_id(mote_id))

        def add_sample(v, delta):
            nonlocal tmax
            t[mote_id] = t.get(mote_id, -delta) + delta
            tmax = max(tmax, t[mote_id])
    #r[mote_id]['ts'][-1] + MEASUREMENT_INTERVAL if len(r[mote_id]['ts']) else 0
            if b is not None and not valid(b, t.get(mote_id, 0)):
                return
                #print("    aborting reading of {} at {}".format(fn,
                    #t.get(mote_id, 0)))
                #return False
            r[mote_id]['ts'].append(t[mote_id])

            a = alpha
            if b is not None and '_alpha' in b:
                a = b['_alpha']
            assert 0.0 <= a <= 1.0

            r[mote_id]['vs'].append(a * v + (1.0 - a) * (r[mote_id]['vs'][-1] if len(r[mote_id]['vs']) else v))

        if use_subsamples:
            for i in range(64):
                s = 'sample_{}'.format(i)
                add_sample(float(row[s]) * CURRENT_FACTOR, MEASUREMENT_INTERVAL / 64.0)
        else:
            add_sample(float(row['avg']) * CURRENT_FACTOR, MEASUREMENT_INTERVAL)

    print("    {} contained {}s of measurements".format(fn, tmax))
    return r

def parse_tuple_counts(f, expid):
    r = []
    for line in f:
        m = re.search(r'sent ([0-9]+) tuples chk', line)
        if m is not None:
            if int(expid) >= 24638:
                # starting from exp 24638 tuple counts are reported
                # incrementally
                r.append(int(m.groups()[0]) - (sum(r) if len(r) else 0))
            else:
                r.append(int(m.groups()[0]))
    return r

#
# Processing data
#

def process_energy_ts_erase(d, mode, lbl='', tmin=0, tmax=None, maxvalues=200, bl =None):
    ts = d['ts']
    vs = d['vs']
    ts, vs = zip(*t_average(zip(ts, vs), delta_t = 0.0136))
    #ts, vs = zip(*band_stop(zip(ts, vs), T = 0.0136))

    fig_energy(ts, vs, lbl)

    em = ExperimentModel(
            #Situation('idle', max = 1.2, d_min=5.0),
            Situation('start', max = 1.2),
            Repeat(
                Situation('prep', min=1.05, d_min=0.01, d_max=0.1),
                Situation('between', max=1.1, d_min=0.05, d_max=0.2),
                Situation('exp', min=1.05, d_min=0.01, d_max=0.1),
                Situation('after', max=1.1, d_min=0.4, d_max=0.6),
            )
        )

    #ts, vs = zip(*t_average(zip(ts, vs), delta_t = 0.1))

    tsums = []
    esums = []
    ots = []

    baseline = 0
    for m in em.match(ts, vs):
        print("---- found: {} {}...{}".format(m.situation.name, m.t0, m.t0 + m.d))
        if m.situation.name == 'exp':
            print('-- {} {}-{} -> {} - {}'.format(m.situation.name, m.t0, m.t0+m.d, m.v_sum, baseline))
            ots.append(m.t0)
            tsums.append(m.d)
            esums.append(m.v_sum - baseline * m.d)
        elif m.situation.name in ('idle', 'start', 'between', 'after'):
            print("---- setting baseline: ", m.situation.name, m.v_average)
            baseline = m.v_average


    return [tsums], [esums], [ots]


def process_energy_ts_erase_(d, mode, lbl='', tmin=0, tmax=None,maxvalues=200,bl=None):
    ts = d['ts']
    vs = d['vs']
    fig_energy(ts, vs, lbl)

    MEASUREMENT_UP = 1.75
    MEASUREMENT_DOWN = 1.25
    MODE = 'value'

    if bl and '_threshold' in bl:
        MEASUREMENT_UP = bl['_threshold']
        MEASUREMENT_DOWN = bl['_threshold']

    if bl and '_mode' in bl:
        MODE = bl['_mode']

    class State:
        def __init__(self, s): self.s = s
        def __str__(self): return self.s
    idle_low = State('idle_low')
    idle_between = State('idle_between')
    preparation = State('preparation')
    measurement = State('measurement')

    sums_e = []
    sums_t = []
    runs_e = []
    runs_t = []
    runs_ot = []
    ots = []

    thigh = 0
    t0 = 0
    tprev = 0
    esum = 0
    t = 0
    v = 0

    #esums_i = []
    #esums_f = []
    #tsums_i = []
    #tsums_f = []

    BASELINE_ENERGY_TEENYLIME = 0 # 0.08
    baseline_estimate = 0
    baseline_estimate_n = 0

    state = idle_low

    def change_state(s):
        nonlocal state
        nonlocal thigh
        if s is not state:
            print("{}: {} -> {} v={}".format(t, state, s, v))
            state = s

    for t, v in zip(ts, vs):
        if t < tmin: continue
        if tmax is not None and t > tmax:
            print("{} > tmax".format(t))
            break

        #print(t, v)

        if state is idle_low:
            if v > MEASUREMENT_UP:
                if mode == 'value':
                    change_state(measurement)
                    t0 = t
                    esum = (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
                else:
                    change_state(preparation)
            #else:
            elif v < MEASUREMENT_DOWN:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        elif state is preparation:
            if v < MEASUREMENT_DOWN:
                change_state(idle_between)

        elif state is idle_between:
            if v > MEASUREMENT_UP:
                change_state(measurement)
                t0 = t
                esum = (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
            #else:
            elif v < MEASUREMENT_DOWN:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        elif state is measurement:
            if v < MEASUREMENT_DOWN:
                #print("adding ({}, {})".format(t-t0, esum))
                print("    t={} esum={}".format(t, esum))
                ots.append(t)
                sums_t.append(t - t0)
                sums_e.append(esum)
                change_state(idle_low)
            else:
                esum += (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
        tprev = t


    print("  baseline estimate: {}".format(baseline_estimate))

    for i in range(len(sums_t)):
        if sums_e[i] is not None and sums_t[i] is not None:
            sums_e[i] -= sums_t[i] * baseline_estimate

    runs_t.append(sums_t)
    runs_e.append(sums_e)
    runs_ot.append(ots)
    return runs_t, runs_e, runs_ot

def process_energy_teenylime(d, mode, lbl='', tmin=0, tmax=None):
    ts = d['ts']
    vs = d['vs']
    fig_energy(ts, vs, lbl)

    if mode == 'find' or mode == 'erase':
        MEASUREMENT_UP = 0.4
        MEASUREMENT_DOWN = 0.25
    else:
        MEASUREMENT_UP = 1.0
        MEASUREMENT_DOWN = 0.8

    delta_t = 0.8

    class State:
        def __init__(self, s): self.s = s
        def __str__(self): return self.s
    idle_low = State('low')
    measurement = State('measurement')

    sums_e = []
    sums_t = []
    ots = []
    runs_e = []
    runs_t = []
    runs_ot = []

    thigh = 0
    t0 = 0
    tprev = 0
    esum = 0
    t = 0
    v = 0

    #esums_i = []
    #esums_f = []
    #tsums_i = []
    #tsums_f = []

    BASELINE_ENERGY_TEENYLIME = 0 # 0.08
    baseline_estimate = 0
    baseline_estimate_n = 0

    state = idle_low

    def change_state(s):
        nonlocal state
        nonlocal thigh
        if s is not state:
            #print("{}: {} -> {} v={}".format(t, state, s, v))
            state = s

    for t, v in zip(ts, vs):
        if t < tmin: continue
        if tmax is not None and t > tmax:
            print("{} > tmax".format(t))
            break

        assert t >= tprev

        if state is idle_low:
            if v > MEASUREMENT_UP:

                change_state(measurement)
                #if mode == 'insert':
                t0 = t
                esum = (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
                #tprev = t
            else:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        elif state is measurement:
            if v < MEASUREMENT_DOWN:
                print("    --- t={} dt={} esum={}".format(t, t - t0, esum))
                change_state(idle_low)
                #if mode == 'insert':
                #esum += (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
                ots.append(t)
                sums_t.append((t - t0) / TEENYLIME_FINDS_AT_ONCE)
                if mode == 'find':
                    esum /= TEENYLIME_FINDS_AT_ONCE
                sums_e.append(esum)
            else:
                esum += (t - tprev) * (v - BASELINE_ENERGY_TEENYLIME)
        tprev = t


    print("  baseline estimate (teenylime): {}".format(baseline_estimate))

    for i in range(len(sums_t)):
        if sums_e[i] is not None and sums_t[i] is not None:
            #assert sums_t[i] >= 0
            sums_e[i] -= sums_t[i] * baseline_estimate

    runs_t.append(sums_t)
    runs_e.append(sums_e)
    runs_ot.append(ots)
    return runs_t, runs_e, runs_ot

def process_energy(d, mode, lbl='', tmin=0):
    ts = d['ts']
    vs = d['vs']
    fig_energy(ts, vs, lbl)

    # setup as follows:
    #
    # (1) first, start in high-load mode ( >= HIGH ),
    # (2) then, itll drop down to idle ( <= IDLE )
    # (3) then itll go above MEASUREMENT
    # (4) then fall below IDLE
    # then back to 1

    HIGH = 7.0
    RADIO = 1.5
    MEASUREMENT = 1.5
    MEASUREMENT_FIND = 1.2

    class State:
        def __init__(self, s): self.s = s
        def __str__(self): return self.s

    idle_high = State('high')

    idle_before = State('idle before')
    insert = State('insert')
    idle_between = State('idle between')
    find = State('find')
    idle_after = State('idle_after')

    state = idle_high

    # unit: mA * s = mC
    #energy_sums_insert = []
    #energy_sums_find = []
    #time_sums_insert = []
    #time_sums_find = []
    sums_e = []
    sums_t = []
    ots = []
    runs_e = []
    runs_t = []
    runs_ot = []

    thigh = 0
    t0 = 0
    tprev = 0
    esum = 0
    t = 0

    #esums_i = []
    #esums_f = []
    #tsums_i = []
    #tsums_f = []

    baseline_estimate = 0
    baseline_estimate_n = 0

    def change_state(s):
        nonlocal state
        nonlocal thigh
        if s is not state:
            #print("    {}: {} -> {}".format(t, state, s))
            if s is idle_high:
                thigh = t
            state = s

    for t, v in zip(ts, vs):
        if t < tmin: continue
        assert t >= tprev

        if state is idle_high:
            if v < RADIO:
                if t - thigh > 500:
                    #print("  reboot t={} thigh={}".format(t, thigh))
                    # reboot
                    runs_e.append(sums_e)
                    runs_t.append(sums_t)
                    runs_ot.append(ots)
                    sums_e = []
                    sums_t = []
                    ots = []
                    
                    #esums_i.append(energy_sums_insert)
                    #esums_f.append(energy_sums_find)
                    #tsums_i.append(time_sums_insert)
                    #tsums_f.append(time_sums_find)
                    #energy_sums_insert = []
                    #energy_sums_find = []
                    #time_sums_insert = []
                    #time_sums_find = []
                #else:
                    #print("  t-tigh={} t={}".format(t-thigh, t))
                change_state(idle_before)

        elif state is idle_before:
            if v > MEASUREMENT:
                change_state(insert)
                if mode == 'insert':
                    t0 = t
                    esum = (t - tprev) * (v - BASELINE_ENERGY)
                    #tprev = t
            else:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        elif state is insert:
            if v < MEASUREMENT:
                change_state(idle_between)
                if mode == 'insert':
                    print("    t={} esum={}".format(t, esum))
                    #esum += (t - tprev) * (v - BASELINE_ENERGY)
                    sums_t.append(t - t0)
                    sums_e.append(esum)
                    ots.append(t)
            else:
                #print(v, t)
                #assert v < HIGH
                if v >= HIGH:
                    print("    insert abort high t={}".format(t))
                    sums_e.append(None)
                    sums_t.append(None)
                    ots.append(t)
                    change_state(idle_high)
                    #thigh = t
                elif mode == 'insert':
                    esum += (t - tprev) * (v - BASELINE_ENERGY)
                    #tprev = t

        elif state is idle_between:
            if v > HIGH:
                #print("  find measurement skipped high at {}".format(t))
                if mode == 'find':
                    sums_e.append(None)
                    sums_t.append(None)
                    ots.append(t)
                change_state(idle_high)
                #thigh = t
            elif v > MEASUREMENT_FIND:
                change_state(find)
                if mode == 'find':
                    t0 = t
                    #tprev = t
                    #esum = 0
                    esum = (t - tprev) * (v - BASELINE_ENERGY)
                    #print(t,tprev,v, BASELINE_ENERGY)
                    assert esum >= 0
            else:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        elif state is find:
            if v < MEASUREMENT:
                change_state(idle_after)
                if mode == 'find':
                    #print("    find measurement at {} - {} e {}".format(t0, t, esum))
                    print("    t={} dt={} esum={}".format(t, t - t0, esum))
                    #esum += (t - tprev) * (v - BASELINE_ENERGY)
                    sums_t.append((t - t0) / FINDS_AT_ONCE)
                    sums_e.append(esum / FINDS_AT_ONCE)
                    ots.append(t)
            elif v > HIGH:
                #print("  find measurement aborted high at {} t0={} esum={}".format(t, t0, esum - BASELINE_ENERGY))
                # what we thought was a find measurement was actually a
                # rising edge for the high idle state,
                # seems there was no (measurable) find process, record a
                # 0-measurement
                change_state(idle_high)
                #thigh = t
                if mode == 'find':
                    print("    find abort high t0={} t={}".format(t0, t))
                    sums_t.append(None)
                    sums_e.append(None)
                    ots.append(t)
            else:
                if mode == 'find':
                    esum += (t - tprev) * (v - BASELINE_ENERGY)
                    #print(t,tprev,v, BASELINE_ENERGY)
                    assert esum >= 0
                    #tprev = t

        elif state is idle_after:
            if v > HIGH:
                change_state(idle_high)
                #thigh = t
            elif v < MEASUREMENT:
                baseline_estimate *= baseline_estimate_n / (baseline_estimate_n + 1.0)
                baseline_estimate_n += 1.0
                baseline_estimate += v / baseline_estimate_n 

        tprev = t


    print("  baseline estimate: {}".format(baseline_estimate))

    #state = "H"
    #oldstate = "H"
    #for t, v in zip(ts, vs):
        #if state == "H" and v < IDLE:
            #state = "I0"
        #elif state == "I1" and v > HIGH:
            #state = "H"
        #elif state == "I0" and v > MEASUREMENT:
            #state = "M"
            #t0 = t
            #tprev = t
            #esum = 0
        #elif state == "M" and v < IDLE:
            #state = "I1"
            #time_sums.append(t - t0)
            #energy_sums.append(esum)
        #elif state == "M":
            ##assert v < HIGH
            #esum += (t - tprev) * v
            #tprev = t
    
        #if state != oldstate:
            #print("{}: {} ({} -> {})".format(t, v, oldstate, state))
            #oldstate = state

    #esums_i.append(energy_sums_insert)
    #esums_f.append(energy_sums_find)
    #tsums_i.append(time_sums_insert)
    #tsums_f.append(time_sums_find)
    runs_t.append(sums_t)
    runs_e.append(sums_e)
    runs_ot.append(ots)

    for rt, re in zip(runs_t, runs_e):
        for i in range(len(rt)):
            if re[i] is not None and rt[i] is not None:
                print(re[i], rt[i], baseline_estimate)
                if mode == 'find':
                    re[i] -= rt[i] * baseline_estimate #/ FINDS_AT_ONCE
                else:
                    re[i] -= rt[i] * baseline_estimate
                #print(re[i], rt[i], baseline_estimate)
                #assert re[i] >= 0

    return (runs_t, runs_e, runs_ot)

#
# Plotting
#

def fig_energy(ts, vs, n):
    if not PLOT_ENERGY:
        return
    fig = plt.figure(figsize=(12,5))
    ax = plt.subplot(111)
    
    #ax.set_xticks(range(250, 311, 2))
    #ax.set_yticks(frange(0, 3, 0.2))

    #ax.set_xlim((388.06, 388.1))
    ax.set_xlim((600, 660))
    #ax.set_xlim((1210, 1220))
    ax.set_ylim((0, 2.5))
    #ax.set_ylim((.5, 2.5))
    ax.grid()

    ax.plot(ts, vs, 'k-')
    #fig.show()
    fig.savefig('energy_{}.pdf'.format(n), bbox_inches='tight', pad_inches=.1)
    plt.close(fig)

def plot_experiment(n, ax, **kwargs):
    global fig
    ts, vs = parse_energy(open('{}.csv'.format(n), 'r'))
    fig_energy(ts, vs, n)
    #tc = parse_tuple_counts(open('{}/inode001/output.txt'.format(n), 'r', encoding='latin1'), int(n))
    d = find_tuple_spikes(ts, vs)
    
    tc = [7, 6, 8, 11, 11, 10, 11, 9]
    #ax = plt.subplot(111)
    for e in range(len(d['e_insert'])):
        es = d['e_insert'][e]
        fs = d['e_find'][e]
        fs = d['e_find'][e]

        print(d['e_insert'][e])
        print(d['t_insert'][e])

        #ax.plot(cum(tc[:len(es)]), ([x/y for x, y in zip(es, tc)]), 'o-', **kwargs) 
        ax.plot(cum(tc[:len(fs)]), ([x/y for x, y in zip(fs, tc)]), 'x-', **kwargs) 
        ax.plot(cum(tc[:len(fs)]), ([x/y for x, y in zip(d['t_find'][e], tc)]), 'x-', **kwargs) 


#class plot_broken_y:
    #def __init__(self, break_start, break_end):
        #self.break_start = break_start
        #self.break_end = break_end
        #self.f, (self.ax, self.ax2) = plt.subplots(2, 1, sharex=True)
        #self.ax

    #def plot(self, *args, **kws):
        #self.ax.plot(*args, **kws)
        #self.ax2.plot(*args, **kws)
        
    


#
# Experiment registry
#

class ExperimentClass:
    def __init__(self, d):
        self.ts_container = 'vector_static'
        self.ts_dict = 'chopper'
        self.__dict__.update(d)

    def __eq__(self, other):
        return (
            self.dataset == other.dataset and
            self.mode == other.mode and
            self.debug == other.debug and
            self.database == other.database and (
                self.database != 'tuplestore' or (
                    self.ts_dict == other.ts_dict and
                    self.ts_container == other.ts_container
                )
            )
        )

    def reprname(self):
        return self.database + '_' + self.dataset.replace('.rdf', '') + '_' + self.mode + (' DBG' if self.debug else '')

    def __hash__(self):
        return hash(self.dataset) ^ hash(self.mode) ^ hash(self.debug) ^ hash(self.database)

class Experiment:
    def __init__(self):
        self.time = []
        self.energy = []
        self.tuplecounts = []
        self.key = None

    def add_measurement(self, i, t, e):
        while len(self.time) < i + 1:
            self.energy.append([])
            self.time.append([])


        if self.key.mode == 'find':
            #assert e >= 0
            self.time[i].append(t)
            self.energy[i].append(e)
        else:
            while len(self.tuplecounts) < i + 1:
                self.tuplecounts.append(100)
            #print(i, len(self.time), len(self.tuplecounts))
            diff = (self.tuplecounts[i] - (self.tuplecounts[i - 1] if i > 0 else 0))
            if diff <= 0:
                print("  (!) warning: tc[{}]={} tc[{}]={} setting diff to 1".format(i, self.tuplecounts[i], (i-1), self.tuplecounts[i - 1] if i > 0 else 0))
                diff = 1
            self.time[i].append(t / diff)
            #assert e >= 0
            self.energy[i].append(e / diff)

    def set_tuplecounts(self, tcs):
        if len(self.tuplecounts) > len(tcs):
            return
        self.tuplecounts = cum(tcs)

def add_experiment(cls):
    global experiments

    for k, v in experiments:
        if k == cls:
            return v

    v = Experiment()
    v.key = cls
    experiments.append((cls, v));
    return v
    #if cls not in experiments:
        #experiments[cls] = Experiment()
        #experiments[cls].key = cls
    #return experiments[cls]

#
# Blacklisting etc..
#
     
def has_valid(b):
    """
    >>> has_valid({ '_valid': [] })
    False
    >>> has_valid({ })
    False
    >>> has_valid({ '_tmax': 0 })
    False

    >>> has_valid({ '_valid': [(100, 200), (350, 400)] })
    True
    >>> has_valid({ '_tmax': 100 })
    True
    >>> has_valid({ '_tmin': 100 })
    True
    >>> has_valid({ '_tmax': 0, '_tmin': 50 })
    True
    """

    if '_valid' in b:
        return len(b['_valid']) > 0
    elif '_tmin' in b: return True
    elif '_tmax' in b: return b['_tmax'] != 0
    return False

def valid(b, t):
    """
    >>> valid({ '_valid': [] }, 123)
    False
    >>> valid({ }, 123)
    False
    >>> valid({ '_tmax': 0 }, 123)
    False

    >>> valid({ '_valid': [(100, 200), (350, 400)] }, 100)
    True
    >>> valid({ '_valid': [(100, 200), (350, 400)] }, 101)
    True
    >>> valid({ '_valid': [(100, 200), (350, 400)] }, 357.2)
    True
    >>> valid({ '_valid': [(100, 200), (350, 400)] }, 200.01)
    False
    >>> valid({ '_tmax': 100 }, 99)
    True
    >>> valid({ '_tmin': 100 }, 100.123)
    True
    >>> valid({ '_tmax': 0, '_tmin': 50 }, 55.678)
    False
    """
    if '_valid' in b:
        for a, b in b['_valid']:
            if t >= a and t <= b: return True
        return False
    elif '_tmin' in b:
        if '_tmax' in b and b['_tmax'] is not None:
            return t >= b['_tmin'] and t <= b['_tmax']
        return t >= b['_tmin']
    elif '_tmax' in b:
        return b['_tmax'] is None or b['_tmax'] >= t
    return False

#
# Utility functions
#

def cleanse(l1, l2):
    """
    Given two lists l1 and l2 return two sublists l1' and l2' such that
    whenever an element at position i in either l1 or l2 is None or the empty
    list, data from that
    position at l1 or l2 will not be present in the returned lists.

    Note that specifically considers only None and [], not other values that
    evaluate to false like 0, False or the empty string.

    As a side effect it ensures the returned lists are cut to the same length

    >>> cleanse([1, 2, None, 4, [], 6], [None, 200, 300, 400, 500, 600, 70, 88])
    ([2, 4, 6], [200, 400, 600])

    >>> cleanse([1, [None, None, 2, None, 3]], [[None, 100], [200, 300]])
    ([1, [2, 3]], [[100], [200, 300]])

    """

    generator = type((x for x in []))

    r1 = []
    r2 = []
    for x, y in zip(l1, l2):
        if isinstance(x, list) or isinstance(x, generator):
            x = list(filter(lambda x: x is not None and x != [], x))
        if isinstance(y, list) or isinstance(y, generator):
            y = list(filter(lambda z: z is not None and z != [], y))

        if x is not None and x != [] and y is not None and y != []:
            r1.append(x)
            r2.append(y)
    return r1, r2

def plausible_observation_times(es, ts, ots, d):
    """
    >>> es = range(10)
    >>> ts = range(10)
    >>> ots = [ 1.0, 2.01, 2.98, 4.7, 5.7, 6.77, 7.89, 9.6, 10.01, 10.99 ]
    >>> e2, t2, ot2 = plausible_observation_times(es, ts, ots, 1.0)
    >>> ot2
    [1.0, 2.01, 2.98, 3.98, 4.7, 5.7, 6.77, 7.89, 8.89, 9.6, 10.99]
    >>> e2
    [0, 1, 2, None, 3, 4, 5, 6, None, 7, 9]
    >>> t2 == e2
    True

    >>> es = ts = (0, 1)
    >>> ots = [ 0.0, 5.0 ]
    >>> es2, ts2, ots2 = plausible_observation_times(es, ts, ots, 1.0)
    >>> es2
    [0, None, None, None, None, 1]
    >>> ts2 == es2
    True
    >>> ots2
    [0.0, 1.0, 2.0, 3.0, 4.0, 5.0]
    """

    dmin = d / 2.0
    dmax = d * 1.5

    es2 = []
    ts2 = []
    ots2 = []

    tprev = None
    for (e, t, ot) in zip(es, ts, ots):
        if tprev is not None and (ot - tprev) < dmin:
            # too close, skip this entry
            pass
        elif tprev is not None and (ot - tprev) > dmax:
            for f in frange(tprev + d, ot - d/2, d):
                # too far, insert a new entry
                ots2.append(f)
                ts2.append(None)
                es2.append(None)
            ots2.append(ot)
            ts2.append(t)
            es2.append(e)
            tprev = ot
        else:
            ots2.append(ot)
            ts2.append(t)
            es2.append(e)
            tprev = ot

    return es2, ts2, ots2
            

def frange(a, b, step):
    """
    >>> frange(0.1, 4.7, 1.0)
    [0.1, 1.1, 2.1, 3.1, 4.1]
    """
    return [a + x * step for x in range(int(math.ceil((b - a) / step)))]

def cum(l):
    s = 0
    r = []
    for v in l:
        s += v
        r.append(s)
    return r

def median(l):
    if len(l) == 0: return None
    elif len(l) == 1: return l[0]
    #print("{} -> {}".format(l, sorted(l)[int(len(l) / 2)]))
    sl = sorted(l)
    if len(l) % 2:
        return sorted(l)[int(len(l) / 2)]
    return (sl[int(len(l) / 2)] + sl[int(len(l) / 2 - 1)]) / 2.0

def inode_to_mote_id(s):
    """
    >>> inode_to_mote_id('inode123')
    '10123'
    >>> inode_to_mote_id('inode001')
    '10001'
    """
    return '10' + s[5:]

def mote_id_to_inode_id(s):
    """
    >>> mote_id_to_inode_id('10123')
    'inode123'
    >>> mote_id_to_inode_id('10001')
    'inode001'
    """
    return 'inode' + s[2:]


def flatten(l):
    return list(itertools.chain.from_iterable(l))

def tex(s):
    escape = ('_', '/')
    for e in escape:
        s = s.replace(e, '\\' + e)
    return s




    ## incontextsensing
    ##tc = [7, 6, 8, 11, 11, 10, 11, 9]

    ##ax = plt.subplot(111)
    #print("XXX", energy_sums)
    #print("YYY", tc)
    #ax.plot(cum(tc[:len(energy_sums)]), ([x/y for x, y in zip(energy_sums, tc)]), 'o-',
            #**kwargs) 
    #ax.plot(cum(tc[:len(time_sums)]), ([x/y for x, y in zip(time_sums, tc)]), 'x-',
            #**kwargs)
    #ax.legend()

#ts, vs = parse_energy(open('tuplestore_24526_1242.csv', 'r'))
#ts, vs = parse_energy(open('tuplestore_24528_1242.csv', 'r'))
#fig_energy(ts, vs)
#print()

#fig = plt.figure()
#ax = plt.subplot(111)

#plot_experiment(24539, ax, label='antelope 39')

## unbaltreedict, list_dynamic container
##plot_experiment(24529, ax, label='ts')

## ts staticdict(100, 15), staticvector(76)
##plot_experiment(24578, ax, label='ts')

## antelope with more realistic config?
##plot_experiment(24579, ax, label='antelope2')

## ts staticdict(100, 15), staticvector(76), strncmp_etc_builtin
##plot_experiment(24580, ax, label='ts100')

## ts staticdict(200, 15), staticvector(76), strncmp_etc_builtin
##plot_experiment(24582, ax, label='ts200a')
## ts staticdict(200, 15), staticvector(76), strncmp_etc_builtin
##plot_experiment(24583, ax, label='ts200b')

# ts insert incontextsensing staticdict(200, 15), staticvector(76), strncmp_etc_builtin
#plot_experiment(24638, ax, label='ts200')
# ts insert incontextsensing staticdict(100, 15), staticvector(76), strncmp_etc_builtin
#plot_experiment(24639, ax, label='ts100')

## ts insert incontextsensing staticdict-le(100, 15), staticvector(76), strncmp_etc_builtin
#plot_experiment(24643, ax, label='ts100le 43')
#plot_experiment(24644, ax, label='ts100le 44')
#plot_experiment(24645, ax, label='ts100le 45')
##plot_experiment(24646, ax, label='ts100le 46')
#plot_experiment(24648, ax, label='ts100le 48')
#plot_experiment(24649, ax, label='ts100le 49')

#plot_experiment(24641, ax, label='antelope 41')

#plot_experiment(24650, ax, label='antelope 50')
#plot_experiment(24651, ax, label='antelope 51')
#plot_experiment(24652, ax, label='antelope 52')

#plot_experiment(24778, ax, label='antelope')

#fig.savefig('energy_inserts.pdf')

if __name__ == '__main__':
    main()


