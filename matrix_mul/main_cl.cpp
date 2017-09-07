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
using namespace std;
#define problemSize (rank*rank)

int main(int argc, char** argv)
{
	ifstream ifstr(argv[1]);
	
	const string programString(istreambuf_iterator<char>(ifstr),
		(istreambuf_iterator<char>()));
	try
	{
		vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);
		vector<int> h_a(problemSize),h_b(problemSize),h_c(problemSize);

		for(int i=0;i<problemSize;i++)
		{
			h_a[i]=rand()%10;
			h_b[i]=rand()%10;
		}
		cl_context_properties properties[] =
		{ CL_CONTEXT_PLATFORM,
			(cl_context_properties)(platforms[0])(),
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
				program.getBuildInfo(devices[0],CL_PROGRAM_BUILD_LOG, &info);
				cout << info << endl;
				return EXIT_FAILURE;
			}
			else throw err;
		}

		cl::CommandQueue commandQueue(context, devices[0]);
		
		typedef cl::make_kernel<cl::Buffer,cl::Buffer,cl::Buffer> kernelType;
		kernelType kernelFunctor = kernelType(program, "mmult");

		cl::Buffer d_a = cl::Buffer(context, h_a.begin(),h_a.end(),true);
		cl::Buffer d_b = cl::Buffer(context, h_b.begin(),h_b.end(),true);
		cl::Buffer d_c = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(int)*h_c.size());
		
		cl::EnqueueArgs enqueueArgs(commandQueue,cl::NDRange(1),cl::NDRange(1));
		cl::Event event = kernelFunctor(enqueueArgs,
						d_a,
						d_b,
						d_c
						);

		commandQueue.finish();
		event.wait();
		cl::copy(commandQueue, d_c, h_c.begin(), h_c.end());


		int m=0, running = 0, ref[problemSize];
		for (int c=0;c<rank;c++){
			for (int r=0;r<rank;r++){
				running=0;
				for (int index=0; index<rank; index++) {
					int aIndex = r*rank + index;
					int bIndex = index*rank + c;
					running += h_a[aIndex] * h_b[bIndex];
//		std::cout<<"aIndex="<<aIndex<<" bIndex="<<bIndex<<'\t';
//		std::cout<<"dataA="<<cacheA[aIndex]<<" dataB="<<cacheB[bIndex]<<'\t';
//		std::cout<<"A="<<(*a)[aIndex]<<" B="<<(*b)[bIndex]<<" running="<<running<<std::endl;
      }
      ref[r*rank + c] = running;
			//      std::cout<<"C="<<output[r*rank + c] <<std::endl;

			}
		}
		int i = 0;
		for (vector<int>::iterator it = h_c.begin(); it != h_c.end();i++, it++)
		{
			if(ref[i] != *it){
				cout<<"i="<<i<<",kernel="<<*it<<",ref="<<ref[i]<<endl;
				m++;
			}
		}
		cout<<m<<endl;
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

	return EXIT_SUCCESS;
}
