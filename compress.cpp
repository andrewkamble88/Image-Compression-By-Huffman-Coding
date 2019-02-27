# include "opencv2/highgui/highgui.hpp"
# include "opencv2/imgproc/imgproc.hpp"
# include <iostream>
# include <string>
# include <fstream>
# include <exception>
# include "opencv2/highgui/highgui.hpp"
# include "opencv2/imgproc/imgproc.hpp"
# include <iostream>
# include "Huffman.cpp"
using namespace std;
using namespace cv;

class Compressor
{
	Huffman h;
	Mat grayImage;//Original image converted to monochromatic
	Mat dctImage;//Image DCT quantized
	int height, heightDCT;
	int width, widthDCT;
	float C;//Compression Param
	int *zigZagMask; // A mask to zig-zag search
	string *shiftedBit;//Mask for shift bits in string
	unsigned int numberBitsShifted;
	Mat luminance;//Quantization Matrix for luminance
	priority_queue<node> frequenceTable;//Frequence table for Huffman Algorithm
	ofstream outfile; //To write in .compressed
	ifstream infile;  //To read from .compressed
	string codifiedImage;  //Image after huffman codification
	string fileName;		//Name of the file .compressed
	vector<float> codeTable;

	public:
	Compressor();
	virtual ~Compressor(){};
	void setParamCompression(char* imageName);
	void setParamDecompression(char *imageName);
	void forwardDCT();	// Forward transformation - DCT
	void inverseDCT();	// Inverse transformation - iDCT
	void displayImage(string imageName, Mat image);// Display all images
	void compress(char* imageName);//Execute Huffman to create a table of symbols and codify the image to a file
	void decompress(char* imageName);
	void searchFrequenceTable();// Search the frequency table and set it in huffman's codification.
	void readCodeTable();		// Read from .compressed the frequency table and set it in huffman's codification.

	void readCodifiedImage();	// Read the image from .compressed.
};


Compressor::Compressor()
 {
	//Mask to shiftting a bit in string
	static string bitsMask[10] = {"", "0", "00", "000", "0000", "00000", "000000", "0000000", "00000000"};
	shiftedBit = bitsMask;

	static int zzMask[64] = {
			  0,  1,  8, 16,  9,  2, 3,  10,
			  17, 24, 32, 25, 18, 11,  4,  5,
			  12, 19, 26, 33, 40, 48, 41, 34,
			  27, 20, 13,  6,  7, 14, 21, 28,
			  35, 42, 49, 56, 57, 50, 43, 36,
			  29, 22, 15, 23, 30, 37, 44, 51,
			  58, 59, 52, 45, 38, 31, 39, 46,
			  53, 60, 61, 54, 47, 55, 62, 63};
	zigZagMask = zzMask;

	//Create the luminance matrix for quantization
	luminance = (Mat_<float>(8, 8) <<
			16, 11, 10, 16, 24, 40, 51, 61,
			12, 12, 14, 19, 26, 58, 60, 55,
			14, 13, 16, 24, 40, 57, 69, 56,
			14, 17, 22, 29, 51, 87, 80, 62,
			18, 22, 37, 56, 68, 109, 103, 77,
			24, 35, 55, 64, 81, 104, 113, 92,
			49, 64, 78, 87, 103, 121, 120, 101,
			72, 92, 95, 98, 112, 100, 103, 99);

	width = 0;
	height = 0;
	widthDCT = 0;
	heightDCT = 0;
	C = 1;				//Compress rate - [1 Maximal - 8 Minimal]
	numberBitsShifted = 0;
}



void Compressor::setParamCompression(char* imageName){
	/// Load image in gray scale
	grayImage = imread( imageName, IMREAD_GRAYSCALE );

	//Check if image exist
	if( !grayImage.data ){
		cout << "\nCan't load the image. Please insert the image address." << endl;
		throw exception();
	}


	// Modify the image names to .compressed
	fileName.clear();
	fileName.append(imageName);
	fileName.erase(fileName.end()-4, fileName.end());
	fileName +=  ".compressed"; //sufix

	//Open output streams to .compressed
	outfile.open(fileName.c_str());

	// Transform the image resolutions to a resolution with size multiple of 8 by 8
	// Necessary to broke the image in 8 by 8 blocks in DCT step
	height = grayImage.size().height;
	width = grayImage.size().width;

	heightDCT = (height%8) ? (heightDCT = 8 - height%8) : 0;
	widthDCT = (width%8) ?  (widthDCT = 8 - width%8) : 0;

	if (heightDCT != 0){
		Mat hBlock = grayImage(Rect(0, height-heightDCT, width, heightDCT));
		vconcat(grayImage, hBlock, grayImage);
		height += heightDCT;
	}

	if (widthDCT != 0){
		Mat wBlock = grayImage(Rect(width-widthDCT, 0, widthDCT, height));
		hconcat(grayImage, wBlock, grayImage);
		width += widthDCT;
	}

	// Write the image's size in .compressed
	outfile << height << " " << width << endl;
	outfile << heightDCT << " " << widthDCT << endl;

}


// Forward transformation - DCT
void Compressor::forwardDCT(){

	dctImage = Mat_<uchar>(height*C/8, width*C/8);

	for (int i = 0; i < height; i += 8){
		for (int j = 0; j < width; j += 8){
				// Get a block 8x8 for each position on image
				Mat block8 = grayImage(Rect(j, i, 8, 8));
				// Convert block from 8 bits to 64 bits
				block8.convertTo(block8, CV_32FC1);
				// DCT
				dct(block8, block8);
				// Quantization step
				divide(block8, luminance, block8);
				// Adding 128 to the block
				add(block8, 128.0, block8);
				// Converting it back to unsigned char
				block8.convertTo(block8, CV_8UC1);
				// Copying the block back to the new dct image
				//block8.copyTo(dctImage(Rect(j, i, 8, 8)));
				Mat blockC = Mat_<uchar>(C, C);
				for (int k = 0; k < (int)C*C; k++)
				blockC.at<uchar>(k) = block8.at<uchar>(zigZagMask[k]);
				//cout << "Copyto\n";
				blockC.copyTo(dctImage(Rect(j*C/8, i*C/8, C, C)));


		}
	}
	dctImage.convertTo(dctImage, CV_8UC1);
}



void Compressor::inverseDCT(){
	grayImage = Mat_<uchar>(height, width);

	for (int i = 0; i < height*C/8; i+=C){
		for (int j = 0; j < width*C/8; j+=C){
			// Get a block 8x8 for each position on image
			Mat block = dctImage(Rect(j, i, C, C));

			// Convert block from 8 bits to 64 bits
			block.convertTo(block, CV_32FC1);

			// Subtracting the block by 128
			subtract(block, 128, block);

			Mat block8 = Mat::zeros(8, 8, CV_32FC1);
			for (int k = 0; k < (int)C*C; k++)
					block8.at<float>(zigZagMask[k]) = block.at<float>(k);


			// Quantization step
			multiply(block8, luminance, block8);

			// Inverse DCT
			idct(block8, block8);

			// Converting it back to unsigned char
			block8.convertTo(block8, CV_8UC1);

			// Copying the block back to the new dct image
			//cout << "Copyto\n";
			block8.copyTo(grayImage(Rect(8*j/C, 8*i/C, 8, 8)));
		}
	}

}


// Display all images
void Compressor::displayImage(string imageName, Mat image){
	/// Display
	namedWindow(imageName, CV_WINDOW_AUTOSIZE );
	imshow(imageName, image);
	waitKey(0);
}



// Compute the histogram matrix for dct image in planes
void Compressor::searchFrequenceTable(){
	int histSize = 256; /// Establish the number of bins

	float range[] = {0, 256}; /// Set the ranges
	const float* histRange = {range};

	bool uniform = true;
	bool accumulate = false;

	Mat histogram;//Histogram for DCT Image

	calcHist( &dctImage, 1, 0, Mat(), histogram, 1, &histSize, &histRange, uniform, accumulate);
	vector<float> f(histogram.begin<float>(), histogram.end<float>());//Transform mat to vector
	h.setFrequenceTable(f);
}



//Compressor step
void Compressor::compress(char* imageName){
	//Just for information
	cout << "\nCompressing...";

	cout << "\nSetting Parameter...";
	setParamCompression(imageName);

	cout << "\nTransforming Image...";
	forwardDCT();

	cout << "\nSearching Frequency Table...";
	searchFrequenceTable();

	cout << "\nComputing Symbol Table...";
	codeTable = h.getHuffmanCode();

	//Write the Frequency Table in the file
	cout << "\nPrinting Frequency Table...";
	for (unsigned short i = 0; i < codeTable.size(); i++){
		if (codeTable.at(i) == 0) continue;
		outfile << i  << " " << codeTable.at(i) << endl;
	}

	//transform image Mat to vector
	vector<int> inputFile;
 	inputFile.assign(dctImage.datastart, dctImage.dataend);

	//Codify image dct based on table of code from Huffman
	cout << "\nEncoding Image with Huffman...";
	codifiedImage = h.encode(inputFile);

	//Add 0's in the end of the file to complete a byte
	numberBitsShifted = 8 - codifiedImage.length()%8;
	codifiedImage += shiftedBit[numberBitsShifted];


	//End of the code table and the number of bits 0 shifted in the end of the file
	outfile << "#" << numberBitsShifted  << endl;

	cout << "\nPrinting Image...";
	size_t imSize = codifiedImage.size();
	for (size_t i = 0; i < imSize; i += 8){
		outfile << (uchar)strtol(codifiedImage.substr(i, 8).c_str(), 0, 2);
	}
	codifiedImage.clear();

	//Close the file .compressed
	outfile.close();

	//Just for information
	if (!codifiedImage.length())
		cout << "\nCompressing Successful!";
	else
		cout << "\nCompressing Failed!";

}


void Compressor::setParamDecompression(char* imageName){
	fileName.clear();
	fileName.append(imageName);

	if (fileName.substr(fileName.length()-11, fileName.length()) != ".compressed"){
		cout << "\nCan't load the image. Please insert the image address." << endl;
		throw exception();
	}
	fileName.erase(fileName.length()-11, fileName.length());
	fileName += ".cmp.png";

	infile.open(imageName);
	if (!infile.is_open()){
		cout << "\nCan't load the image. Please insert the image address." << endl;
				throw exception();
	}

	string size;//Get the original size of the image
	infile >> size;
	height = atoi(size.c_str());
	infile >> size;
	width = atoi(size.c_str());

	infile >> size;
	heightDCT = atoi(size.c_str());
	infile >> size;
	widthDCT = atoi(size.c_str());
}


// Read the symbol table
void Compressor::readCodeTable(){

	string ind, frequency;
	infile >> ind;
	while(ind[0] != '#'){
		infile >> frequency;
		h.setFrequenceTable(atoi(ind.c_str()), strtof(frequency.c_str(), 0));
		infile >> ind;
	}
	ind.erase(0,1);//remove '#'
	numberBitsShifted = atol(ind.c_str());
}


void Compressor::readCodifiedImage(){
	unsigned char c;
	unsigned char mask = 128;//1000 0000
	//codifiedImage.clear();

	int count = 0;

	infile >> noskipws;
	infile >> c;

	while(infile >> c){
		for (int i = 0; i < 8; i++){

			if ((c << i) & mask){
				codifiedImage.push_back('1');
			}
			else{
				codifiedImage.push_back('0');
			}
			count++;
		}
	}
	codifiedImage.erase(codifiedImage.length()-numberBitsShifted, numberBitsShifted);//remove the exceed bits
}



//Uncompress step
void Compressor::decompress(char* fileName){
	//Just for information
	cout << "\nUncompressing...";

	cout << "\nSetting Parameters...";
	setParamDecompression(fileName);

	cout << "\nReading Symbol Table ...";
	readCodeTable();

	cout << "\nsReading Codified Image ...";
	readCodifiedImage();

	cout << "\nDecoding Image with Huffman...";
	vector<int> dctFile = h.decode(codifiedImage);

	dctImage = Mat::zeros(height*C/8, width*C/8, CV_8UC1);

	cout << "\nTransforming Image...";
	for (int i = 0; i < height*C/8; i++){
		for (int j = 0; j < width*C/8; j++){
			dctImage.at<uchar>(i, j) = (uchar)dctFile.at(i*dctImage.step + j);
		}
	}
	cout << "\nProcessing Image...";
	inverseDCT();

	if (widthDCT != 0)
		grayImage = grayImage.colRange(0, width - widthDCT);

	if (heightDCT != 0)
		grayImage = grayImage.rowRange(0, height - heightDCT);

	//imwrite(this->fileName, grayImage);
	imwrite(this->fileName, grayImage);
	Mat testcolor=imread(this->fileName,CV_LOAD_IMAGE_COLOR);
	//imwrite("output.png", testcolor);
	cout << "\nImage Build Successfully!";
}


//Compressor::~Compressor() {}


void show_histogram(string const& name, Mat1b const& image)
{
    // Set histogram bins count
    int bins = 256;
    int histSize[] = {bins};
    // Set ranges for histogram bins
    float lranges[] = {0, 256};
    const float* ranges[] = {lranges};
    // create matrix for histogram
    Mat hist;
    int channels[] = {0};

    // create matrix for histogram visualization
    int const hist_height = 256;
    Mat3b hist_image = Mat3b::zeros(hist_height, bins);

    calcHist(&image, 1, channels, Mat(), hist, 1, histSize, ranges, true, false);

    double max_val=0;
    minMaxLoc(hist, 0, &max_val);

    // visualize each bin
    for(int b = 0; b < bins; b++) 
    {
        float const binVal = hist.at<float>(b);
        int   const height = cvRound(binVal*hist_height/max_val);
        line
            ( hist_image
            , Point(b, hist_height-height), Point(b, hist_height)
            , Scalar::all(255)
            );
    }
    imshow(name, hist_image);
}

