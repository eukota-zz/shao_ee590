

__kernel void pfac(__global char* inputBuffer, __global char* inputPatterns, __global int* numOfPatterns, __global int* outputIndex ){ 
	int idx = get_global_id(0);
	int locationOffset =idx*inputBuffer; ///position

		node* ptr = stateMachine;
	int len = strlen(text);
	for (int i = 0; i < len; i++) {
		char ch = text[i];

		// While there is no valid transaction for ch, switch to the failure transaction for the current state.
		while (ptr && !ptr->children[idxForChar(ch)]) {
			ptr = ptr->failure;
		}

		// Failing to NULL means there is no possible fail back for ch. Point back to root.
		if (!ptr) {
			ptr = stateMachine;
			continue;
		}

		// Valid fail-back node with a ch transaction found. Go to the corresponding child node.
		ptr = ptr->children[idxForChar(ch)];
		// If current node is a stop node, record all matches.
		if (ptr->results.size()) {
			for (int j = 0; j < ptr->results.size(); j++) {
				string pattern = ptr->results[j];
				if (result.find(pattern) == result.end()) {
					result[pattern] = {};
					result[pattern].push_back(locationOffset + i - pattern.length());
				}
				else {
					result[pattern].push_back(locationOffset + i - pattern.length());
				}
			}
		}
	}

}