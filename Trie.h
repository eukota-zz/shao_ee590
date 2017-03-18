#pragma once

#include <vector>
#include <CL/cl.h>
#include "Tools.h"



struct node {

	node* children[50];
	cl_bool isStop;
	node* failure;
	std::string value;
	std::vector<std::string> results;
};

void defineFailures(node* tree);
node* trie(std::vector<std::string> patterns);
void printTree(node* tree, std::string prefix="");