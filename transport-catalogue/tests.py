#!/usr/bin/env python3

import json
import subprocess
import os

def EXEC(filename):
	filename = os.getcwd() + '/tests/' + filename
	f = open(filename)
	bashCommand = os.getcwd() + '/build/sprint12'
	process = subprocess.Popen(bashCommand.split(), stdout=subprocess.PIPE, stdin=f)
	output, error = process.communicate()
	#print(output)
	#print(error)
	return json.loads(output.decode('utf-8'))

os.path.dirname(os.path.realpath(__file__))

files = [
	"1_input.json",
	"2_input.json",
	"3_input.json",
	"4_input.json"
]
exp = {
	"1_input.json": {
		4: 11.235,
		5: 24.21
	},
	"2_input.json": {
		5: 7.42,
		6: 11.44,
		7: 10.7,
		8: 8.56,
		9: 16.32,
		10: 12.04,
	},
	"3_input.json": {
		2: 29.26,
		3: 22,
		4: 0,
	},
	"4_input.json": {
		1607592307: 1596.93,
		1918086003: 1308.26,
		1705878213: 1290.62,
		1032258749: 1460.26,
		862750239: 1658.69,
		1465104363: 1650.87,
		8645483: 736.077,
		1513592365: 1523.08,
		141699407: 739.525,
		220233995: 661.784,
		875126660: 1402.8,
		1054087408: 505.545,
		1107201585: 1668.19,
		1054310039: 1170.31,
		2031874307: 1898.58,
		1732222142: 581.374,
		1355593539: 1581.67,
		1583266593: 1592.76,
		2068492255: 1107.63,
		1217157226: 1002.97,
		1429827679: 741.597,
	},
}

for filename in files:
	if filename not in exp:
		continue
	print(filename)
	cur_exp = exp[filename]
	out_json = EXEC(filename)
	for resp in out_json:
		if resp['request_id'] in cur_exp and 'total_time' in resp and resp['total_time'] != cur_exp[resp['request_id']]:
			print(" ! id=%d actual=%f expected=%f" % (resp['request_id'], resp['total_time'], cur_exp[resp['request_id']]))
