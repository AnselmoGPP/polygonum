
#include <fstream>
#include <iostream>

#include "commons.hpp"

//std::vector< std::function<glm::mat4(float)> > room_MM{ /*room1_MM, room2_MM, room3_MM, room4_MM*/ };

// Read a file called <filename> and save its content in <destination>. Since a std::vector<char> is used, it may save garbage values at the end of the string.
void readFile(const char* filename, std::vector<char>& destination)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);	// Open file. // ate: Start reading at the end of the file  /  binary: Read file as binary file (avoid text transformations)
	if (!file.is_open())
		throw std::runtime_error("Failed to open file!");
	
	size_t fileSize = 0;
	fileSize = (size_t)file.tellg();
	destination.resize(fileSize);					// Allocate the buffer

	file.seekg(0);
	file.read(destination.data(), fileSize);		// Read data

	file.close();									// Close file
}

// Read a file called <filename> and save its content in <destination>.
void readFile(const char* filename, std::string& destination)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);	// Open file. // ate: Start reading at the end of the file  /  binary: Read file as binary file (avoid text transformations)
	if (!file.is_open())
		throw std::runtime_error("Failed to open file!");

	size_t fileSize = 0;
	fileSize = (size_t)file.tellg();
	destination.resize(fileSize);					// Allocate the buffer

	file.seekg(0);
	file.read(destination.data(), fileSize);		// Read data
	
	file.close();									// Close file
}

void copyCString(const char*& destination, const char* source)
{
	size_t siz = strlen(source) + 1;
	char* address = new char[siz];
	strncpy(address, source, siz);
	destination = address;
}
