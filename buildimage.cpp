# include <iostream>
#include "compress.cpp"

int main(int argc, char** argv)
{

	Compressor d;
	d.decompress(argv[1]);
      	return 0;
}


