#define function(...) std::cout << "function" << std::endl; function(__VA_ARGS__);

import re

FILE = 'main.cpp'

all_functions = []

with open(FILE, 'r') as file_in:
	for line in file_in.readlines():
		for function in re.findall(r'xr[^(_/]*', line):
			if not function.endswith('\n') and function != 'xr':
				all_functions.append(function)

print(all_functions)

for function in all_functions:
	print(f'#define { function }(...) std::cout << "{ function }" << std::endl; { function }(__VA_ARGS__);')
