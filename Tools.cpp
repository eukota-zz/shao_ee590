#include "CL/cl.h"

const cl_char symbols[] = { ' ', ',', '.', '?', '!', '\'', '"', '(', ')', ';', ':', '-', '_' };
const cl_int numOfSymbols = sizeof(symbols) / sizeof(cl_char);


const cl_int ALPHA_SIZE = 26 + 10 + numOfSymbols + 1;


cl_int idxForChar(cl_char ch)
{
	if (ch >= 'a' && ch <= 'z') {
		return ch - 'a';
	}
	if (ch >= 'A' && ch <= 'Z') {
		return ch - 'A';
	}
	if (ch >= '0' && ch <= '9') {
		return 26 + (ch - '0');
	}
	for (cl_int i = 0; i < numOfSymbols; i++) {
		if (symbols[i] == ch) {
			return 36 + i;
		}
	}
	return ALPHA_SIZE - 1;
}
