#include "ACproject.h"
#include <iostream>
using namespace std;

int main(int argc, char** argv) 
{
	std::cout << "This is an AC algorithm " << endl;
	std::cout<< "Enter 1 to run"<<  endl;
	int in = 0;
	std::cin >> in;

	if (in==1)
		run();

	return 0;
}