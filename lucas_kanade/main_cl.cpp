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
#include "param.h"
using namespace std;


namespace Params 
{
	int platform = 0;								// -p
	int device = 0;									// -d
	char *kernel_name=NULL;     // -k
	char *binary_name=NULL;     // -b
	char *output_name=NULL;
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
		while((opt=getopt(argc,argv,"p:d:k:f:b:"))!=-1){
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
				case 'f':
					output_name=optarg;
					flagb=true;
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
		
		// create platform, device, program and kernel
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
		
		//typedef cl::make_kernel<cl::Buffer,cl::Buffer,cl::Buffer,
		//				cl::Buffer,cl::Buffer,cl::Buffer,cl::Buffer> kernelType;
		typedef cl::make_kernel<cl::Buffer,cl::Buffer,cl::Buffer> kernelType;
		kernelType kernelFunctor = kernelType(program, Params::kernel_name);

		// input and output parameters
		vector<unsigned char> h_im11(ARRAY_SIZE),h_im21(ARRAY_SIZE);
		vector<float>h_out(ARRAY_SIZE * 2);
		vector<float> h_coefx(ARRAY_SIZE),h_coefy(ARRAY_SIZE),h_movx(ARRAY_SIZE),h_movy(ARRAY_SIZE);

		for(int i=0;i<ARRAY_SIZE;i++)
		{
			h_im11[i]=i;//rand()%256;
			h_im21[i]=i+1;//rand()%256;
			h_coefx[i]=(rand()%97)/97.0;
			h_coefy[i]=(rand()%97)/97.0;
			h_movx[i]=(rand()%97)/97.0;
			h_movy[i]=(rand()%97)/97.0;
		}
		
		// create buffer
		cl::Buffer d_im11 = cl::Buffer(context, h_im11.begin(),h_im11.end(),true);
		cl::Buffer d_im21 = cl::Buffer(context, h_im21.begin(),h_im21.end(),true);
		cl::Buffer d_coefx = cl::Buffer(context, h_coefx.begin(),h_coefx.end(),true);
		cl::Buffer d_coefy = cl::Buffer(context, h_coefy.begin(),h_coefy.end(),true);
		cl::Buffer d_movx = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*h_movx.size());
		cl::Buffer d_movy = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*h_movy.size());
		cl::Buffer d_out = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*h_out.size());
		
		cl::EnqueueArgs enqueueArgs(commandQueue,cl::NDRange(1),cl::NDRange(1));
		cout<<"begin the kernel."<<endl;
		cl::Event event = kernelFunctor(enqueueArgs,
				d_im11,
				d_im21,
				//d_coefx,
				//d_coefy,
				d_out//,
				//d_movx,
				//d_movy
				);

		commandQueue.finish();
		event.wait();
		
		// load result
		cl::copy(commandQueue, d_out, h_out.begin(), h_out.end());
		ofstream outfile;
		outfile.open(Params::output_name);
		for(vector<float>::iterator it=h_out.begin();it!=h_out.end();it++)
			outfile<<*it<<'\t';
		outfile.close();
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
