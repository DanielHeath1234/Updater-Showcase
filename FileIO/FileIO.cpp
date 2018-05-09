#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "include\headers\autoupdater\AutoUpdater.h"

// c++ standard library

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

// Takes user input and streams it into the file.
void takeInput(std::fstream* file) {
	string input;
	cout << "Please type in a word: ";
	cin >> input;
	*file << input;
}

int main() {

	// Showcase of autoupdater.
	auto Updater = new AutoUpdater(Version("1.0"),
		"https://raw.githubusercontent.com/DanielHeath1234/AIE-AutoUpdater/master/version", 
		"https://github.com/DanielHeath1234/AIE-AutoUpdater/archive/master.zip");
	delete Updater;

	std::fstream file;
	file.open("file.txt", std::ios_base::out);

	// Write input to file
	if (file.is_open()) { // Check if file is okay
		for (size_t i = 0; i < 4; i++) {
			takeInput(&file);
			file << endl;
		}
		//file << input; // Write input to file
	}
	else {
		std::cerr << "Cannot open file" << endl;
	}

	file.close();

	// Reading something from a file into our programs memory
	string fileContents;
	file.open("file.txt", std::ios_base::in);

	vector<string> fileStrings;

	if (file.is_open()) {
		//file >> fileContents;
		//cout << fileContents << endl;

		while (!file.eof()) { // Keep grabbing lines until eof is reached (End Of File).
			char fileLine[256];
			file.getline(fileLine, 256); // getLine grabs a single line from the file
			string fileLineString = fileLine; // Converting the charArray into a string
			fileStrings.push_back(fileLineString);
		}
	}
	else {
		// Throwing exceptions
		std::cerr << "Cannot open file" << endl;
	}

	file.close();

	for (size_t i = 0; i < fileStrings.size(); i++)
	{
		cout << fileStrings[i] << endl;
	}

	system("pause");
}