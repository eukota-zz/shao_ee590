#include "Trie.h"
#include <vector>
#include "CL/cl.h"
#include <queue>
#include <string>
#include <iostream>

using namespace std;



node* trie(std::vector<std::string> patterns) 
{
	//create root node
	node* root = new node();
	root->value = "";
	root->isStop = false;

	//for loop for multi words
	//insert node for pattern
	for (cl_int i = 0; i < patterns.size(); i++) {
		string word = patterns[i];
		node* nodePtr = root;
		//for loop single word length of word
		//fill letters in node
		for (cl_int j = 0; j < word.length(); j++) {
			cl_char letter = word[j];
			if (!nodePtr->children[idxForChar(letter)]) {
				nodePtr->children[idxForChar(letter)] = new node();
				string v = nodePtr->value.append(1, letter);
				nodePtr->children[idxForChar(letter)]->value = v;
			}
			nodePtr = nodePtr->children[idxForChar(letter)]; //traversing down the tree
		}
		nodePtr->isStop = true;
		nodePtr->results.push_back(word);
	}

	return root;
}

void printTree(node* tree, string prefix)
{
	if (!tree) return;
	std::cout << prefix << tree->value << ";" << (tree->failure ? "F: " + tree->failure->value : "") << std::endl;
	std::cout << prefix << "R: ";
	for (int i = 0; i < tree->results.size(); i++) {
		std::cout << tree->results[i] << ",";
	}
	std::cout << std::endl;
	for (int i = 0; i < 50; i++) {
		if (tree->children[i]) {
			printTree(tree->children[i], prefix + "  ");
		}
	}
}


/**
* Use BFS to establish failure transactions.
*/
void defineFailures(node* tree) 
{
	if (!tree) return;
	queue<node*> q;
	tree->failure = NULL; // root node fails back to NULL
						  // First-level children fail back to root
						  // Push first-level children into queue to initialize queue state
	for (cl_char ch = 'a'; ch <= 'z'; ch++) {
		node* child = tree->children[idxForChar(ch)];
		if (child) {
			q.push(child);
			child->failure = tree;
		}
	}
	while (!q.empty()) {
		node* parent = q.front();
		for (cl_char ch = 'a'; ch <= 'z'; ch++) {
			node* child = parent->children[idxForChar(ch)];
			if (child) {
				q.push(child);
				// parent --ch--> child.
				// [parent's failure node] --ch--> [child's failure node]
				node* failureNode = parent->failure;
				while (failureNode && !failureNode->children[idxForChar(ch)]) failureNode = failureNode->failure;
				if (!failureNode) {
					child->failure = tree;
				}
				else {
					// If child's failure node is found, merge results from the failure node.
					child->failure = failureNode->children[idxForChar(ch)];
					child->results.insert(child->results.end(), child->failure->results.begin(), child->failure->results.end());
				}
			}
		}
		q.pop();
	}
}