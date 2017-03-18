#include "ACproject.h"
#include <iostream>
using namespace std;

int main(int argc, char** argv) 
{
	int in = 0;
	std::cout << "This is an AC algorithm " << endl;

	do {
		std::cout<< endl
			<< "Enter 1 to run sequential" << endl
			<< "Enter 2 to run parallel" << endl
			<< "(-1 to quit)" << endl;

		std::cin >> in;

		switch (in)
		{
		case 1:
			sequential();
			break;
		case 2:
			parallel();
			break;
		case -1:
			break;

		default:
			cout << "unknown command" << endl;
			break;
		}
	} while (in !=-1);
	return 0;
}