// Project Aho Corasick String Matching Algorithm on GPU


#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <string> 
#include <fstream> 
#include <chrono>
#include <iostream>
#include <queue>
#include <vector>
#include <map>
#include "Trie.h"
#include "ocl_utils.h"
#include <malloc.h> 
#include "Tools.h"
#include <CL/cl.h> 
#include "Trie.h"


#define CL_USE_DEPRECATED_OPENCL_1_2_APIS 
#define SEPARATOR       ("----------------------------------------------------------------------\n") 
#define INTEL_PLATFORM  "Intel(R) OpenCL" 
#define BUF_SIZE 40000000


using namespace std;

cl_int err;                             // error code returned from api calls 
cl_platform_id   platform = NULL;		// platform id 
cl_device_id     device_id = NULL;		// compute device id  
cl_context       context = NULL;		// compute context 
cl_command_queue commands = NULL;		// compute device's queue 
cl_program       program = NULL;		// compute program 
cl_kernel        kernel = NULL;			// compute kernel 
cl_event		 prof_event = NULL;		// Profiling Event, measure the wait time

const char* textChunk;
const char* patternsHost;

cl_mem bufferBuffer; // Stream text
cl_mem bufferPatterns; // patterns

cl_mem bufferNumberofPatterns; // Number of Patterns output 
cl_mem bufferIndex; // Index number

double *run_time_sequential = NULL;
double *run_time_parallel = NULL;
float *elapsed = NULL;

cl_int datasize = 1000000;


node* constructStateMachine(const char** patterns, cl_int numOfPatterns) {
	vector<string> patternVector;
	for (cl_int i = 0; i < numOfPatterns; i++) {
		patternVector.push_back(string(patterns[i]));
	}
	node* stateMachine = trie(patternVector);
	defineFailures(stateMachine);
	return stateMachine;
}

/**
* Use an established state machine to scan the input text.
* Input params:
* - text: Input text to find matches in
* - stateMachine: Pointer to the starting/current state in the state machine
* - root: Root node of the state machine / trie tree. When no matches are possible, go back to the root node.
* - locationOffset: Offset value for reporting matching locations.
* - result: Map to store matching locations for patterns.
*/
void scanText(const char* text, node* stateMachine, cl_int locationOffset, map<string, vector<cl_int>> &result) {
	node* ptr = stateMachine;
	cl_int len = strlen(text);
	for (cl_int i = 0; i < len; i++) {
		cl_char ch = text[i];

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
			for (cl_int j = 0; j < ptr->results.size(); j++) {
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


// get platform id of Intel OpenCL platform 
cl_platform_id get_intel_platform()
{
	// Trying to get a handle to Intel's OpenCL platform using function 
	// 
	// cl_int clGetPlatformIDs (cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms) 
	// 
	// num_entries is the number of cl_platform_id entries that can be added to platforms. If platforms 
	// is not NULL, the num_entries must be greater than zero. 
	// platforms returns a list of OpenCL platforms found. The cl_platform_id values returned in platforms 
	// can be used to identify a specific OpenCL platform. If platforms argument is NULL, this argument is ignored. 
	// The number of OpenCL platforms returned is the minimum of the value specified by num_entries or the number of 
	// OpenCL platforms available. 
	// num_platforms returns the number of OpenCL platforms available. If num_platforms is NULL, this argument is ignored. 
	// 
	// Trying to identify one platform: 

	cl_platform_id platforms[10] = { NULL };
	cl_uint num_platforms = 0;

	cl_int err = clGetPlatformIDs(10, platforms, &num_platforms);

	if (err != CL_SUCCESS) {
		printf("Error: Failed to get a platform id!\n");
		return NULL;
	}

	size_t returned_size = 0;
	cl_char platform_name[1024] = { 0 }, platform_prof[1024] = { 0 }, platform_vers[1024] = { 0 }, platform_exts[1024] = { 0 };

	for (unsigned int ui = 0; ui < num_platforms; ++ui)
	{
		// Found one platform. Query specific information about the found platform using the function  
		// 
		// cl_int clGetPlatformInfo (cl_platform_id platform, cl_platform_info param_name, 
		//                           size_t param_value_size, void *param_value,  
		//                           size_t *param_value_size_ret) 
		// 
		// platform refers to the platform ID returned by clGetPlatformIDs or can be NULL. 
		// If platform is NULL, the behavior is implementation-defined. 
		// 
		// param_name is an enumeration constant that identifies the platform information being queried. 
		// We'll query the following information (for complete documentation, see Specification, page 30): 
		// 
		// CL_PLATFORM_NAME       -platform name string 
		// CL_PLATFORM_VERSION    -OpenCL version supported by the implementation 
		// CL_PLATFORM_PROFILE    -FULL_PROFILE if the implementation supports the OpenCL specification 
		//                        -EMBEDDED_PROFILE - if the implementation supports the OpenCL embedded profile (subset). 
		// CL_PLATFORM_EXTENSIONS -extension names supported by the platform 
		// 
		// param_value is a pointer to memory location where appropriate values for a given param_name will be returned. 
		// If param_value is NULL, it is ignored. 
		// 
		// param_value_size specifies the size in bytes of memory pointed to by param_value. 
		// param_value_size_ret returns the actual size in bytes of data being queried by param_value. 
		// 
		// Trying to query platform specific information... 

		err = clGetPlatformInfo(platforms[ui], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, &returned_size);
		err |= clGetPlatformInfo(platforms[ui], CL_PLATFORM_VERSION, sizeof(platform_vers), platform_vers, &returned_size);
		err |= clGetPlatformInfo(platforms[ui], CL_PLATFORM_PROFILE, sizeof(platform_prof), platform_prof, &returned_size);
		err |= clGetPlatformInfo(platforms[ui], CL_PLATFORM_EXTENSIONS, sizeof(platform_exts), platform_exts, &returned_size);

		if (err != CL_SUCCESS) {
			printf("Error: Failed to get platform info!\n");
			return NULL;
		}

		// check for Intel platform 
		if (!strcmp((char*)platform_name, INTEL_PLATFORM)) {
			printf("\nPlatform information: %d\n", ui);
			printf(SEPARATOR);
			printf("Platform name:       %s\n", (char *)platform_name);
			printf("Platform version:    %s\n", (char *)platform_vers);
			printf("Platform profile:    %s\n", (char *)platform_prof);
			printf("Platform extensions: %s\n", ((char)platform_exts[0] != '\0') ? (char *)platform_exts : "NONE");
			return platforms[ui];
		}
	}

	return NULL;
}

// read the kernel source code from a given file name 
char* read_source(const char *file_name)
{
	FILE *file;
	file = fopen(file_name, "rb");
	if (!file) {
		printf("Error: Failed to open file '%s'\n", file_name);
		return NULL;
	}

	if (fseek(file, 0, SEEK_END))
	{
		printf("Error: Failed to seek file '%s'\n", file_name);
		fclose(file);
		return NULL;
	}
	long size = ftell(file);
	if (size == 0)
	{
		printf("Error: Failed to check position on file '%s'\n", file_name);
		fclose(file);
		return NULL;
	}

	rewind(file);

	char *src = (char *)malloc(sizeof(char) * size + 1);
	if (!src)
	{
		printf("Error: Failed to allocate memory for file '%s'\n", file_name);
		fclose(file);
		return NULL;
	}
	printf("Reading file '%s' (size %ld bytes)\n", file_name, size);

	size_t res = fread(src, 1, sizeof(char) * size, file);
	if (res != sizeof(char) * size)
	{
		printf("Error: Failed to read file '%s'\n", file_name);
		fclose(file);
		free(src);
		return NULL;
	}

	src[size] = '\0'; // NULL terminated  
	fclose(file);

	return src;
};
// print the build log in case of failure 
void build_fail_log(cl_program program, cl_device_id device_id)
{
	cl_int err = CL_SUCCESS;
	size_t log_size = 0;

	err = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
	if (CL_SUCCESS != err)
	{
		printf("Error: Failed to read build log length...\n");
		return;
	}

	char* build_log = (char*)malloc(sizeof(char) * log_size + 1);
	if (NULL != build_log)
	{
		err = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, build_log, &log_size);
		if (CL_SUCCESS != err)
		{
			printf("Error: Failed to read build log...\n");
			free(build_log);
			return;
		}

		build_log[log_size] = '\0';    // mark end of message string 

		printf("Build Log:\n");
		puts(build_log);
		fflush(stdout);

		free(build_log);
	}
}

// read binary content 
int ReadBinaryFile(const std::string filename, char** data, bool isSVM)
{
	ifstream::pos_type file_size;
	int num_of_elements;
	// Openning bin file with pointer pointing to eof, in order to get file size 
	ifstream file(filename.c_str(), ios::in | ios::binary | ios::ate);
	if (!file.is_open())
	{
		throw string("Could not open file " + filename);
	}

	file_size = file.tellg();
	int mTotalSize = file_size;
	int buffer_size = mTotalSize;

	// Calculating total number of elements to be read 
	num_of_elements = mTotalSize;
	if (!isSVM)
	{
		*data = new char[num_of_elements];
	}
	// Moving file pointer to beginning, and reading file contents 
	file.seekg(0, ios::beg);
	file.read(*data, mTotalSize);
	file.close();
	return num_of_elements;
}

// Clear All Memory
void ClearAllMemory() {
	if (kernel != NULL) {
		clReleaseKernel(kernel);
	}
	if (program != NULL) {
		clReleaseProgram(program);
	}
	if (commands != NULL) {
		clReleaseCommandQueue(commands);

	}
	if (bufferBuffer != NULL) {
		clReleaseMemObject(bufferBuffer);
		clReleaseMemObject(bufferPatterns);
		clReleaseMemObject(bufferNumberofPatterns);
		clReleaseMemObject(bufferIndex);
	}

	if (context != NULL) {
		clReleaseContext(context);
	}
	if (prof_event != NULL) {
		clReleaseEvent(prof_event);
	}

}


vector<string> patterns;
ifstream patternInput("patterns.txt", ifstream::in);
ifstream fin("input.txt", ifstream::in);
cl_int threadNumber = 2; //change
cl_int chunkSize = 0;
//runs sequential code and then run parallel code


int sequential()
{
	//  Read patterns from patterns.txt; one pattern per line.

	
	string buffer;
	cl_int maxPatternLength = 0;


	//find largest pattern length
	while (!patternInput.eof()) {
		getline(patternInput, buffer);
		cl_int len = buffer.size();
		if (len > 0) {
			patterns.push_back(buffer);
			if (len > maxPatternLength) {
				maxPatternLength = len;
			}
		}
	}
	patternInput.close();

	const char** patternsPtr = new const char*[patterns.size()];
	for (cl_int i = 0; i < patterns.size(); i++) {
		patternsPtr[i] = patterns[i].c_str();
	}
	patternsHost = (char*)_aligned_malloc(patterns.size(), 4096);
	patternsHost = *patternsPtr;

	node* stateMachine = constructStateMachine(patternsPtr, patterns.size());


	ofstream fout("output sequential.txt", ifstream::out);
	ofstream fpout("output parallel.txt", ifstream::out);
	string input;
	while (!fin.eof()) {
		getline(fin, buffer);
		input += buffer + '\n';
	}
	fin.close();

	//fout << "Total length of input: " << input.size() << endl;
	//fout << "Longest pattern length: " << maxPatternLength << endl;

	map<string, vector<cl_int>> result;

	cl_int chunkSizeWithoutOverlap = (input.size() + threadNumber - 1) / threadNumber;
	chunkSize = chunkSizeWithoutOverlap + maxPatternLength - 1;
	textChunk = (char*)_aligned_malloc(chunkSize * sizeof(char), 4096);

	for (cl_int i = 0; i < threadNumber; i++) {
		cl_int offset = chunkSizeWithoutOverlap * i;

		string bufferChunk = input.substr(offset, chunkSize);

		//fout << "Chunk " << i << " at length " << bufferChunk.size() << ":" << endl << bufferChunk << endl;
		textChunk = bufferChunk.c_str();

		scanText(bufferChunk.c_str(), stateMachine, offset, result);

		// TODO pfac(bufferChunk.c_str(), patternsPtr, patterns.size(), /** output indicator */)

	}


	printf("Window API: running sequatial host code : \t%.2f ms", elapsed);
	fout << "Time need for running sequential code : " << elapsed << " milliseconds" << endl;


	// S - Output matching results
	for (map<string, vector<cl_int>>::iterator it = result.begin(); it != result.end(); it++) {
		fout << "Found " << it->second.size() << " occurrences of " << it->first << "; locations: ";
		for (cl_int i = 0; i < it->second.size(); i++) {
			if (i > 0) fout << ", ";
			fout << it->second[i];
		}
		fout << endl;
	}
	fout.close();

	return 0;

}


int parallel() {

	cl_ulong start_time, end_time;			// Profiling Event Start and end Time




	//———————————————————————————————————————————————————
	// STEP 1: Discover and initialize the platforms
	//———————————————————————————————————————————————————
	// get Intel OpenCL platform 
	platform = get_intel_platform();
	if (NULL == platform)
	{
		printf("Error: failed to found Intel platform...\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}


	//———————————————————————————————————————————————————
	// STEP 2: Discover and initialize the devices
	//———————————————————————————————————————————————————
	// Getting the compute device for the processor graphic (GPU) on our platform by function 
	printf("Selected device: GPU\n");
	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);

	char *deviceName = new char[1024];
	err |= clGetDeviceInfo(device_id, CL_DEVICE_NAME, 1024, deviceName, NULL);
	printf("Device name: %s\n", deviceName);
	if (CL_SUCCESS != err || NULL == device_id)
	{
		printf("Error: Failed to get device on this platform!\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}


	//———————————————————————————————————————————————————
	// STEP 3: Create a context
	//———————————————————————————————————————————————————
	// We have a compute device of required type! Next, create a compute context on it. 
	printf("\n");
	printf(SEPARATOR);
	printf("\nCreating a compute context for the required device\n");

	cl_context_properties properties[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform, '\0' };

	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
	if (CL_SUCCESS != err || NULL == context)
	{
		printf("Error: Failed to create a compute context!\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	// OpenCL objects such as memory, program and kernel objects are created using a context. 
	printf("\n");
	printf(SEPARATOR);
	printf("\nCreating a command queue\n");

	// Create Command Queue with Profiling enable
	commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
	if (CL_SUCCESS != err || NULL == commands)
	{
		LogError("Error: Failed to create a command queue! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	//———————————————————————————————————————————————————
	// STEP 5: Create device buffers
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("\nCreating Buffer\n");

	bufferBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, chunkSize*sizeof(char), NULL, &err);
	bufferPatterns = clCreateBuffer(context, CL_MEM_READ_ONLY, patterns.size()*sizeof(char), NULL, &err);
	bufferNumberofPatterns = clCreateBuffer(context, CL_MEM_READ_ONLY, datasize, NULL, &err);
	bufferIndex = clCreateBuffer(context, CL_MEM_READ_ONLY, datasize, NULL, &err);

	if (CL_SUCCESS != err)
	{
		LogError("Error: clCreateBuffer_Failed to create buffer! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	//———————————————————————————————————————————————————
	// STEP 6: Write host data to device buffers
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("\nWrite Buffer\n");
	int whatsize = sizeof(textChunk) * sizeof(char);
	err = clEnqueueWriteBuffer(commands, bufferBuffer, CL_FALSE, 0, whatsize, textChunk, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(commands, bufferPatterns, CL_FALSE, 0, sizeof(patternsHost)*sizeof(char), patternsHost, 0, NULL, NULL);

	if (CL_SUCCESS != err)
	{
		LogError("Error: clEnqueueWriteBuffer_Failed to write buffer! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	//———————————————————————————————————————————————————
	// STEP 7: Create and compile the program
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("\nCreate and compile\n");

	string newCLFileName = "PFAC.cl";
	char * kernel_source = read_source(newCLFileName.c_str());

	if (NULL == kernel_source)
	{
		printf("Error: Failed to read kernel source code from file name: %s!\n", newCLFileName.c_str());
		ClearAllMemory();
		return EXIT_FAILURE;
	}
	//printf("%s\n", kernel_source);
	program = clCreateProgramWithSource(context, 1, (const char **)&kernel_source, NULL, &err);
	free(kernel_source);
	if (CL_SUCCESS != err || NULL == program)
	{
		printf("Error: Failed to create compute program! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	err = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	if (CL_SUCCESS != err)
	{
		printf("Error: Failed to build program executable!\n");
		build_fail_log(program, device_id);
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	//———————————————————————————————————————————————————
	// STEP 8: Create the kernel
	//———————————————————————————————————————————————————
	// Create the compute program object for our context and load the source code from the source buffer 
	printf("\n");
	printf(SEPARATOR);
	printf("\nCreating the compute program from source\n");

	kernel = clCreateKernel(program, "PFAC", &err);
	if (CL_SUCCESS != err || NULL == kernel)
	{
		printf("Error: Failed to create compute kernel! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	//———————————————————————————————————————————————————
	// STEP 9: Set the kernel arguments
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("\nSet the kernel arguments\n");


	if (CL_SUCCESS != err)
	{
		LogError("Error: clSetKernelArg_Failed to Set Kernel Arg! Error %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}


	//———————————————————————————————————————————————————
	// STEP 10: Configure the work-item structure
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("Configure the work-item structure \n");

	// TODO: define NDRange
	int dim = 1;
	size_t global[] = { threadNumber };

	//———————————————————————————————————————————————————
	// STEP 11: Enqueue the kernel for execution
	//———————————————————————————————————————————————————
	printf("\n");
	printf(SEPARATOR);
	printf("Enqueue the kernel for execution \n");

	// Timing the clEnqueueNDRangeKernel call and timing information will be stored in timing_event
	err = clEnqueueNDRangeKernel(commands, kernel, dim, NULL, global, NULL, 0, NULL, &prof_event);
	if (CL_SUCCESS != err)
	{
		printf("Error: Failed to execute kernel! %s\n", TranslateOpenCLError(err));
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	err = clFinish(commands);

	if (err != CL_SUCCESS)
	{
		printf("Error: clEnqueueNDRangeKernel failed to finish\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}

	err = clWaitForEvents(1, &prof_event);
	if (err != CL_SUCCESS)
	{
		printf("Error: clWaitForEvents failed to finish\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}


	// Get Profiling Info 
	size_t return_bytes;
	err = clGetEventProfilingInfo(prof_event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &start_time, &return_bytes);
	err |= clGetEventProfilingInfo(prof_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end_time, &return_bytes);
	if (CL_SUCCESS != err)
	{
		printf("Error: clGetEventProfilingInfo Failed to get Event Profiling Info!\n");
		ClearAllMemory();
		return EXIT_FAILURE;
	}


	//———————————————————————————————————————————————————
	// STEP 12: Read the output buffer back to the host
	//———————————————————————————————————————————————————

	//Allocate space for input/output data
	cl_int* parNofPatterns = parNofPatterns = (cl_int*)_aligned_malloc(datasize, 4096);;
	cl_int* parIndex = parIndex = (cl_int*)_aligned_malloc(datasize, 4096);

	clEnqueueReadBuffer(commands, bufferNumberofPatterns, CL_TRUE, 0, datasize, parNofPatterns, 0, NULL, NULL);
	clEnqueueReadBuffer(commands, bufferIndex, CL_TRUE, 0, datasize, parIndex, 0, NULL, NULL);
	printf("\nRead output memory \n");
	printf(SEPARATOR);

	//———————————————————————————————————————————————————
	// STEP 13: Release OpenCL resources
	//———————————————————————————————————————————————————
	// release memory object and host memory
	ClearAllMemory();

	return 0;
}