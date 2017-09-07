// Copyright 2014, Xilinx Inc.
// All rights reserved.

#include <getopt.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <vector>
#include <cassert>
#include "oclHelper.h"

static const int CENTER = 0;
static const int NORTH = 1;
static const int NORTH_WEST = 2;
static const int WEST = 3;
static const int DATA_ALIGNMENT = 32;
static const int N = 85;

void smithwaterman (int *matrix,
                    int *maxIndex,
                    const char *s1,
                    const char *s2,
                    const int N);


struct KernelHostData {
    int *mMatrix;
    char *mSequence1;
    char *mSequence2;
    int mLength;
    int mMaxIndex[DATA_ALIGNMENT/4];
    int mAlignedLength;
    std::string mKernelFile;

    void fillRandom() {
        static const char repo[] = "ATCG";
        mSequence1[0] = '-';
        mSequence2[0] = '-';
        int i = 1;
        std::srand(std::time(0));
        for (; i < mLength; i++) {
            const int index1 = std::rand() % (sizeof(repo) - 1);
            const int index2 = std::rand() % (sizeof(repo) - 1);
            mSequence1[i] = repo[index1];
            mSequence2[i] = repo[index2];
        }
        mSequence1[i] = '\0';
        mSequence2[i] = '\0';
    }

    void fillFixed() {
        std::strcpy(mSequence1, "-TAGGCAAGACCACTTTAGCATGGTCTACAACGCCTAGACCTTTGGCAAAGCAGATCGGCCCGCCCATCACTAGTGGGACTATCC");
        std::strcpy(mSequence2, "-TAATGGGAACACCTGCTGCAATCGGATCGTTGCAGCGGTAATGTGTCGGTATATGCGAGTAGGGTAATCCAAACGTCCCATTGC");
    }

    void fillZeros() {
        std::memset(mMatrix, 0, mLength * mLength * 4);
    }

    void initHelper(int length) {
        mLength = length + 1; // one extra space for first row and column with zeros
        mAlignedLength = mLength;
        int delta = mAlignedLength % DATA_ALIGNMENT;
        if (delta) {
            delta = DATA_ALIGNMENT - delta;
        }
        mAlignedLength += delta;
        mMatrix = new int[mAlignedLength * mAlignedLength];
        mSequence1 = new char[mAlignedLength + 1]; // extra spaces for '\0' at end
        mSequence2 = new char[mAlignedLength + 1]; // extra spaces for '\0' at end
        fillZeros();
    }

    void randomInit(int length) {
        initHelper(length);
        fillRandom();
    }

    void fixedInit() {
        initHelper(N - 1);
        fillFixed();
    }
};

struct KernelDeviceData {
    cl_mem mMatrix;
    cl_mem mSequence1;
    cl_mem mSequence2;
    cl_mem mMaxIndex;

    int init(KernelHostData &host, cl_context context) {
        cl_int err = 0;
        mMatrix = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, host.mAlignedLength * host.mAlignedLength * 4, host.mMatrix, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mSequence1 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.mAlignedLength, host.mSequence1, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mSequence2 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.mAlignedLength, host.mSequence2, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mMaxIndex = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(host.mMaxIndex), host.mMaxIndex, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }
        return 0;
    }
};

class Timer {
    time_t mTimeStart;
    time_t mTimeEnd;
public:
    Timer() {
        mTimeStart = std::time(0);
        mTimeEnd = mTimeStart;
    }
    double stop() {
        mTimeEnd = std::time(0);
        return std::difftime(mTimeEnd, mTimeStart);
    }
    void reset() {
        mTimeStart = time(0);
        mTimeEnd = mTimeStart;
    }
};

typedef std::pair<int, int> Position;

const static struct option long_options[] = {
    {"device",      required_argument, 0, 'd'},
    {"kernel",      required_argument, 0, 'k'},
    {"iteration",   optional_argument, 0, 'i'},
    {"length",      optional_argument, 0, 'l'},
    {"verbose",     no_argument,       0, 'v'},
    {"help",        no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s <options>\n";
    std::cout << "  -d <cpu|gpu|acc|soft>\n";
    std::cout << "  -k <kernel_file> \n";
    std::cout << "  -i <iteration_count>\n";
    std::cout << "  -l <sequence_length>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n";
}


static Position findMaximum(const KernelHostData& hostData)
{
    Position pos;
    pos.first = hostData.mMaxIndex[0] / hostData.mLength;
    pos.second = hostData.mMaxIndex[0] % hostData.mLength;
    return pos;
}


static Position findPrev(const KernelHostData& hostData, const Position& current)
{
    int val = hostData.mMatrix[current.first * hostData.mLength + current.second];
    val &= 0xFFFF0000;
    val >>= 16;
    switch (val) {
    case NORTH:
        return std::make_pair(current.first - 1, current.second);
    case WEST:
        return std::make_pair(current.first, current.second - 1);
    case NORTH_WEST:
        return std::make_pair(current.first - 1, current.second - 1);
    default:
        return std::make_pair(-1, -1);
    }
}


static void printMatrix(const KernelHostData& hostData)
{
    std::cout << "\nMatrix " << hostData.mLength << " x " << hostData.mLength << "\n";
    std::cout << " ";
    for (int i = 0; i < hostData.mLength; i++)
    {
        std::cout << " ";
        std::cout.width(2);
        std::cout << hostData.mSequence1[i];
        std::cout.width();
    }
    for (int index = 0; index < hostData.mLength * hostData.mLength; index++)
    {
        int i = index / hostData.mLength;
        int j = index % hostData.mLength;
        if (j == 0) {
            std::cout << "\n" << hostData.mSequence2[i];
        }
        std::cout << " ";
        std::cout.width(2);
        unsigned val = (unsigned)hostData.mMatrix[index];
        val &= 0x0000ffff;
        std::cout << val;
        std::cout.width();
    }
    std::cout << std::endl;

    std::cout << "\nTracePath " << hostData.mLength << " x " << hostData.mLength << "\n";
    std::cout << " ";
    for (int i = 0; i < hostData.mLength; i++)
    {
        std::cout << "  ";
        std::cout.width(2);
        std::cout << hostData.mSequence1[i];
        std::cout.width();
    }
    for (int index = 0; index < hostData.mLength * hostData.mLength; index++)
    {
        int i = index / hostData.mLength;
        int j = index % hostData.mLength;
        if (j == 0) {
            std::cout << "\n" << hostData.mSequence2[i];
        }

        std::cout << " ";
        const char *buf = 0;
        unsigned val = (unsigned)hostData.mMatrix[index];
        val &= 0xffff0000;
        val >>= 16;
        switch(val) {
        case NORTH:
            buf = "NN";
            break;
        case WEST:
            buf = "WW";
            break;
        case NORTH_WEST:
            buf = "NW";
            break;
        case CENTER:
            buf = "--";
            break;
        default:
            buf = "??";
            assert(false);
            break;
        }

        std::cout << " " << buf;
    }
    std::cout << std::endl;
}


static int printSimilarity(const KernelHostData& hostData)
{
    std::vector<char> alignment1, alignment2;
    Position current = findMaximum(hostData);
    Position prev = findPrev(hostData, current);

    while (current != prev) {
        if (hostData.mMatrix[current.first * hostData.mLength + current.second] == 0) {
            break;
        }

        if (prev.first == current.first) {
            alignment2.insert(alignment2.begin(), '-');
        } else {
            alignment2.insert(alignment2.begin(), hostData.mSequence2[current.first]);
        }

        if (prev.second == current.second) {
            alignment1.insert(alignment1.begin(), '-');
        } else {
            alignment1.insert(alignment1.begin(), hostData.mSequence1[current.second]);
        }

        current = prev;
        prev = findPrev(hostData, current);
    }

    if (alignment1.size() != alignment2.size()) {
        return -1;
    }
    if (alignment1.size() == 0)
        return -1;

    if (alignment2.size() == 0)
        return -1;

    if (alignment1.size() == 1)
        return -1;

    alignment1.push_back('\0');
    alignment2.push_back('\0');

    std::cout << "\nAlign sequence1: " << &alignment1[0] << "\n";
    std::cout << "Align sequence2: " << &alignment2[0] << "\n\n";
    return 0;
}

int runSoftware(KernelHostData &hostData, int iteration, double &delay)
{
    int *mMatrix = new int[hostData.mLength * hostData.mLength];
    char *mSequence1 = new char[hostData.mLength];
    char *mSequence2 = new char[hostData.mLength];
    int mLength = hostData.mLength;
    int mMaxIndex = 0;
    Timer timer;
    for (int i = 0; i < iteration; i++) {
        std::memcpy(mMatrix, hostData.mMatrix, hostData.mLength * hostData.mLength * 4);
        std::memcpy(mSequence1, hostData.mSequence1, hostData.mLength);
        std::memcpy(mSequence2, hostData.mSequence2, hostData.mLength);
        mLength = hostData.mLength;
        mMaxIndex = 0;
        smithwaterman(mMatrix, &mMaxIndex, mSequence1,
                      mSequence2, mLength);
    }
    std::memcpy(hostData.mMatrix, mMatrix, hostData.mLength * hostData.mLength * 4);
    hostData.mMaxIndex[0] = mMaxIndex;
    delay = timer.stop();
    return 0;
}

static int runOpenCL(KernelHostData &hostData, cl_device_type deviceType,
                     int iteration, double &delay, const char *target_device)
{
    size_t workGroupSize = 1;
    oclHardware hardware = getOclHardware(deviceType, target_device);
    if (!hardware.mQueue) {
        return -1;
    }

    oclSoftware software;
    std::memset(&software, 0, sizeof(oclSoftware));
    std::strcpy(software.mKernelName, "smithwaterman");
    std::strcpy(software.mFileName, hostData.mKernelFile.c_str());

    if (deviceType == CL_DEVICE_TYPE_GPU) {
        std::sprintf(software.mCompileOptions, "-DN=%d", hostData.mLength);
    }
    else if (deviceType == CL_DEVICE_TYPE_CPU) {
        std::sprintf(software.mCompileOptions, "-g -DN=%d", hostData.mLength);
    }

    getOclSoftware(software, hardware);

    KernelDeviceData deviceData;
    deviceData.init(hostData, hardware.mContext);

    int arg = 0;

    cl_int err = clSetKernelArg(software.mKernel, arg++, sizeof(cl_mem), &deviceData.mMatrix);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };

    err = clSetKernelArg(software.mKernel, arg++, sizeof(cl_mem), &deviceData.mMaxIndex);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, arg++, sizeof(cl_mem), &deviceData.mSequence1);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, arg++, sizeof(cl_mem), &deviceData.mSequence2);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };

    size_t globalSize[1] = {1};
    size_t *localSize = 0;

    std::cout << "Global size = " << globalSize[0] << "\n";

    if (deviceType == CL_DEVICE_TYPE_ACCELERATOR) {
        localSize = &workGroupSize;
        std::cout << "Local size = " << *localSize << "\n";
    }

    Timer timer;
    for(int i = 0; i < iteration; i++)
    {
        // Here we start measurings host time for kernel execution

        err = clEnqueueNDRangeKernel(hardware.mQueue, software.mKernel, 1, 0,
                                     globalSize, localSize, 0, 0, 0);

        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };

        err = clFinish(hardware.mQueue);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };

        err = clEnqueueReadBuffer(hardware.mQueue, deviceData.mMatrix, CL_TRUE, 0,
                                  hostData.mAlignedLength * hostData.mAlignedLength * 4, hostData.mMatrix, 0, 0, 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };

        err = clEnqueueReadBuffer(hardware.mQueue, deviceData.mMaxIndex, CL_TRUE, 0, sizeof(KernelHostData::mMaxIndex),
                                  hostData.mMaxIndex, 0, 0, 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };
    }
    delay = timer.stop();
    delay /= iteration;
    return 0;
}


int main(int argc, char** argv)
{
#if defined(SDA_PLATFORM) && !defined(TARGET_DEVICE)
    #define STR_VALUE(arg)      #arg
    #define GET_STRING(name) STR_VALUE(name)
    #define TARGET_DEVICE GET_STRING(SDA_PLATFORM)
#endif
    const char *target_device_name = TARGET_DEVICE;
    cl_device_type deviceType = CL_DEVICE_TYPE_ACCELERATOR;;
    int option_index = 0;
    std::string kernelFile("kernel.cl");
    int iteration = 1;
    int length = -1;
    bool verbose = false;
    // Commandline
    int c;
    while ((c = getopt_long(argc, argv, "d:k:i:l:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 'd':
            if (strcmp(optarg, "gpu") == 0)
                deviceType = CL_DEVICE_TYPE_GPU;
            else if (strcmp(optarg, "cpu") == 0)
                deviceType = CL_DEVICE_TYPE_CPU;
            else if (strcmp(optarg, "soft") == 0)
                deviceType = CL_DEVICE_TYPE_DEFAULT;
            else if (strcmp(optarg, "acc") != 0) {
                std::cout << "Incorrect platform specified\n";
                printHelp();
                return -1;
            }
            break;
        case 'k':
            kernelFile = optarg;
            break;
        case 'i':
            iteration = atoi(optarg);
            break;
        case 'l':
            length = atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'v':
            verbose = true;
            break;
        default:
            printHelp();
            return 1;
        }
    }

    KernelHostData hostData;
    if (length != -1) {
        hostData.randomInit(length);
    }
    else {
        hostData.fixedInit();
        length = N - 1;
    }
    hostData.mKernelFile = kernelFile;

    std::cout << "\nInput sequence1: " <<  hostData.mSequence1 + 1 << "\n";
    std::cout << "Input sequence2: " <<  hostData.mSequence2 + 1 << "\n\n";

    double delay = 0;
    if ((deviceType == CL_DEVICE_TYPE_DEFAULT) && runSoftware(hostData, iteration, delay)) {
        std::cout << "FAILED TEST\n";
        return 1;
    }
    else if ((deviceType != CL_DEVICE_TYPE_DEFAULT) && runOpenCL(hostData, deviceType, iteration, delay, target_device_name)) {
        std::cout << "FAILED TEST\n";
        return 1;
    }
    if (verbose) {
        printMatrix(hostData);
    }

    if (printSimilarity(hostData) != 0) {
        std::cout << "FAILED TEST\n";
        return 1;
    }
    std::cout << "OpenCL kernel time: " << delay << " sec\n";
    std::cout << "PASSED TEST\n";
    return 0;
}

// XSIP watermark, do not delete 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
