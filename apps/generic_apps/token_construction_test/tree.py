#!/usr/bin/env python3

import sys
import re
import copy
import bisect
import glob
import os
import os.path

# nodes = [
# 	{
# 		't': time,
# 		id => { 'parent': id, 'filter': num, 'root': id, 'childs': [...] },
# 		id => { 'parent': id, 'filter': num, 'root': id, 'childs': [...] },
# 		id => { 'parent': id, 'filter': num, 'root': id, 'childs': [...] },
# 	},
# 	...
# ]
nodes = []

re_begin_iteration = re.compile(r'-+ BEGIN ITERATION (\d+)')
re_treestate = re.compile(r'@(\d+) p(\d+) d(\d+) rt(\d+) c\d+ t(\d+) nn\d+ ln\d+')
re_act = re.compile(r'@(\d+) (/?)ACT t(\d+)')
re_onoff = re.compile(r'@(-?\d+) (on|off) t(\d+)')
#re_kv = re.compile(r'([A-Za-z_]+) *[= ] *([0-9a-fA-Fx.-]+)')

tmin = None
tmax = None


class tview(object):
	def __init__(self, v):
		self.v = v
	def __item__(self, i): return self.v[i]['t']
	def __getitem__(self, i): return self.v[i]['t']
	def __len__(self): return self.v.__len__()
	def index(self, v):
		for i, x in enumerate(self.v):
			if v == x['t']: return i
		return -1

def insert_state(d):
	global nodes
	
	if tmax is not None and d['t'] > tmax: return
	
	tv = tview(nodes)
	pos = bisect.bisect(tv, d['t'])
	
	if pos < 0 or pos >= len(nodes): bef = { 't': 0 }
	else:
		bef = nodes[pos]
	
	if bef['t'] == d['t']:
		bef.update(d)
		
	else:
		#bef = dict(bef)
		#bef.update(d)
		if pos == -1:
			nodes.append(d)
		else:
			nodes.insert(pos, d)

def parse(f):
	global nodes
	global tmin
	global tmax
	
	t = 0
	for line in f:
		line = line.decode('latin1')
		#print(line)
		
		#m = re.search(re_begin_iteration, line)
		#if m is not None:
			#t = int(m.group(1)) * 1000
			#continue
		
		m = re.search(re_treestate, line)
		if m is not None:
			addr, parent, distance, root, t_ = m.groups()
			t = int(t_)
			d = { 't': t }
			d[addr] = { 'root': root, 'filter': '', 'parent': parent }
			insert_state(d)
			continue
		
		m = re.search(re_act, line)
		if m is not None:
			addr, slash, t_ = m.groups()
			t = int(t_)
			d = { 't': t }
			d[addr] = { 'active': (slash != '/') }
			insert_state(d)
			continue
		
		m = re.search(re_onoff, line)
		if m is not None:
			addr, onoff, t_ = m.groups()
			t = int(t_)
			addr = str(int(addr) + 65536) if int(addr) < 0 else addr
			d = { 't': t }
			d[addr] = { 'on': (onoff == 'on') }
			insert_state(d)
			continue
		
		#if ' L' not in line:
			#print("not treestate: " + line)
			
		
def compress_filter(s):
	#return s.replace('00000000', ':').replace('0000', '.')
	s_orig = s
	
	k = 4
	s_new = ''
	for offs in range(0, len(s), k):
		if s[offs:offs + k] == '0' * k:
			s_new += '.'
		else:
			s_new += s[offs:offs + k]
	s = s_new
			
	s = s.replace('..', ':')
	#s = s.replace('::', '#')
	
	#print("compress filter: {} -> {}".format(s_orig, s))
	return s

def print_dot(d, out):
	out.write('digraph G { rankdir=TB; \n')
	for name, v in d.items():
		if name == 't': continue
		
		
		#print("------------ " + str(v))
		
		color = 'white'
		if v.get('active', False): color = 'green'
		elif v.get('on', True): color = 'red'
		
		out.write('  {} [label="{}\\nroot: {}\\n{}",style="filled",fillcolor={}] ;\n'.format(
			name, name, v.get('root', '???'), compress_filter(v.get('filter', '')), color
		))
		out.write('  {} -> {} ;\n'.format(name, v.get('parent', name)))
	out.write('}\n')

def generate_all(directory):
	global nodes
	global tmin
	global tmax
	last_t = None
	count = 0
	dct = {}
	
	def update_dct(d):
		for k, v in d.items():
			if k == 't':
				dct['t'] = v
				continue
				
			if k in dct:
				dct[k].update(v)
			else:
				dct[k] = v
	
	for d in nodes:
		t = int(d['t'])
		if t == last_t:
			count += 1
		else:
			count = 0
		last_t = t
		
		#dct.update(d)
		update_dct(d)
		
		print("t: {} tmin: {} tmax: {}".format(t, tmin, tmax))
		if tmin is not None and t < tmin: continue
		if tmax is not None and t > tmax: continue
		
		#fn = '{}/t{:08d}_{:04d}.dot'.format(directory, t, count)
		fn = '{}/t{:08d}.dot'.format(directory, t)
		print(fn)
		f = open(fn, 'w')
		print_dot(dct, f)
		f.close()


def test_compress():
	l = """
	0000000002040000401000000000444200000000000002000000004002000020
	0000000000000000000000000000040000000000000000000000000002000000
	0000000002000000000000000000440000000000000000000000000000000000
	0000000000000000001000000000040000000000000000000000000000000000
	""".split()

	for x in l:
		compress_filter(x)

if len(sys.argv) < 2:
	print("Usage: {} DIRNAME [TMIN] [TMAX]".format(sys.argv[0]))
	sys.exit(1)

d = sys.argv[1]

if len(sys.argv) > 2:
	tmin = int(sys.argv[2])
	
if len(sys.argv) > 3:
	tmax = int(sys.argv[3])


print("parsing... tmin={} tmax={}".format(tmin, tmax))

if os.path.isdir(d):
	for fn in glob.glob(d + '/**/output.txt'):
		print(fn)
		parse(open(fn, 'rb'))
else:
	print(d)
	parse(open(d, 'rb'))
	
	
#parse(open('ttyUSB1_2.log', 'rb'))
#parse(open('ttyUSB2_2.log', 'rb'))


print("found {} tree states".format(len(nodes)))
generate_all('dot')
print_dot(nodes[-1], sys.stdout)
			
