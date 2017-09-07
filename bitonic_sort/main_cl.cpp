/*----------------------------------------------------------------------------
*
* Author:   Liang Ma (liang-ma@polito.it)
*
*----------------------------------------------------------------------------
*/
#define __CL_ENABLE_EXCEPTIONS
#include "ML_cl.h"
#include "err_code.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include <unistd.h>
using namespace std;
#define ARRAY_SIZE (1<<EXP)
namespace Params 
{
	int platform = 0;								// -p
	int device = 0;									// -d
	char *kernel_name=NULL;     // -k
	char *binary_name=NULL;     // -b
	bool flagk = false, flagb = false;
	void usage(char* name)
	{
		cout<<"Usage: "<<name
			<<" -b opencl_binary_name"
			<<" -k kernel_name"
			<<" [-p platform]"
			<<" [-d device]"
			<<endl;
	}
	bool parse(int argc, char **argv){
		int opt;
		while((opt=getopt(argc,argv,"p:d:k:b:"))!=-1){
			switch(opt){
				case 'p':
					platform=atoi(optarg);
					break;
				case 'd':
					device=atoi(optarg);
					break;
				case 'k':
					kernel_name=optarg;
					flagk=true;
					break;
				case 'b':
					binary_name=optarg;
					flagb=true;
					break;
				default:
					usage(argv[0]);
					return false;
			}
		}
		return true;
	}
}
int main(int argc, char** argv)
{
	// parse arguments
	if(!Params::parse(argc, argv) || !Params::flagb || !Params::flagk)
		return EXIT_FAILURE;
	try
	{
		// load binary files
		ifstream ifstr(Params::binary_name);
		const string programString(istreambuf_iterator<char>(ifstr),
				(istreambuf_iterator<char>()));
		
		// create platform, device, program
		vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);
		cl_context_properties properties[] =
		{ CL_CONTEXT_PLATFORM,
			(cl_context_properties)(platforms[Params::platform])(),
			0 };
		cl::Context context(CL_DEVICE_TYPE_ACCELERATOR, properties);
		vector<cl::Device> devices=context.getInfo<CL_CONTEXT_DEVICES>();

		cl::Program::Binaries binaries(1, make_pair(programString.c_str(), programString.length()));
		cl::Program program(context,devices,binaries);
		try
		{
			program.build(devices);
		}
		catch (cl::Error err)
		{
			if (err.err() == CL_BUILD_PROGRAM_FAILURE)
			{
				string info;
				program.getBuildInfo(devices[Params::device],CL_PROGRAM_BUILD_LOG, &info);
				cout << info << endl;
				return EXIT_FAILURE;
			}
			else throw err;
		}

		cl::CommandQueue commandQueue(context, devices[Params::device]);
		
		
		// input and output parameters
		const int SIZE = ARRAY_SIZE;
		typedef int TYPE;
		vector<TYPE> h_a(SIZE);

		bool dir = false;

		for(int i=0;i<SIZE;i++)
		{
			h_a[i]=rand()%(100*ARRAY_SIZE);
		}
		// create buffer
		cl::Buffer d_a = cl::Buffer(context, h_a.begin(),h_a.end(),true);
	// create kernel	
		typedef cl::make_kernel<cl::Buffer,bool> kernelType;
		kernelType kernelFunctor = kernelType(program, Params::kernel_name);

		cl::EnqueueArgs enqueueArgs(commandQueue,cl::NDRange(1),cl::NDRange(1));
		cl::Event event = kernelFunctor(enqueueArgs,
				d_a,
				dir
				);

		commandQueue.finish();
		event.wait();
		
	
		// load result
		cl::copy(commandQueue, d_a, h_a.begin(), h_a.end());


		int err = 0;
		for (vector<TYPE>::iterator it = h_a.begin(); it!= h_a.end();it++)
		{
#ifdef DEBUG
			cout<<"array["<<it-h_a.begin()<<"]="<<*it<<endl;
#endif
			if(it+1 == h_a.end())
				break;
			if((*it > *(it + 1))!=dir){
				err++;
			}
		}
		cout<<"There is/are "<<err<<" error(s)."<<endl;
		if(err!=0)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}
	catch (cl::Error err)
	{
		cerr
			<< "Error:\t"
			<< err.what()
			<< "\t reason:"
			<< err_code(err)
			<< endl;

		return EXIT_FAILURE;
	}
}
