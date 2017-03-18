

__kernel void pfac(__global char* inputBuffer, __global char* inputPatterns, __global int* numOfPatterns, __global int* outputIndex ){ 
	int idx = get_global_id(0);
	int locationOffset =inputBuffer[idx]; ///position
	
	numOfPatterns[idx] = 2 ; 
	outputIndex[idx] = locationOffset;

}