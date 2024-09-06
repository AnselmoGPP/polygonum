#ifndef AUXILIARY_HPP
#define AUXILIARY_HPP

#include <vector>
#include <list>

//#include "vertex.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE			// GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
//#define GLM_ENABLE_EXPERIMENTAL			// Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>		// Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
//#include <glm/gtx/hash.hpp>


// Type wrappers -----------------------------------------------------------------

#define stdvec1(T) std::vector<T>
#define stdvec2(T) std::vector<std::vector<T>>
#define stdvec3(T) std::vector<std::vector<std::vector<T>>>

// Print data -----------------------------------------------------------------

/// Print a variable number of arguments.
#define PRINT(...) printArgs(__VA_ARGS__)

/// Print file name, line, and function.
#define FILE_LINE_FUNC \
    std::cerr << __FILE__ << ", " << __LINE__ << ", " << __func__ << "()" << std::endl;

/// Helper: Print a single argument
template<typename T>
void printSingle(const T& t) { std::cout << t; }

/// Helper: Recursive function for printing many arguments
template<typename T, typename... Args>
void printArgs(const T& first, const Args&... args)
{
    printSingle(first);
    printArgs(args...);
}

/// Helper: Base case to stop recursion
void printArgs();


// MVP Matrix -----------------------------------------------------------------

/// Get a basic Model Matrix
glm::mat4 getModelMatrix();

/// Get a user-defined Model Matrix
glm::mat4 getModelMatrix(const glm::vec3& scale, const glm::vec4& rotQuat, const glm::vec3& translation);

/// Model matrix for Normals (it's really a mat3, but a mat4 is returned because it is easier to pass to shader since mat4 is aligned with 16 bytes). Normals are passed to fragment shader in world coordinates, so they have to be multiplied by the model matrix (MM) first (this MM should not include the translation part, so we just take the upper-left 3x3 part). However, non-uniform scaling can distort normals, so we have to create a specific MM especially tailored for normal vectors: mat3(transpose(inverse(model))) * vec3(normal).
glm::mat4 getModelMatrixForNormals(const glm::mat4& modelMatrix);

// View matrix
glm::mat4 getViewMatrix(glm::vec3& camPos, glm::vec3& front, glm::vec3& camUp);

// Projection matrix
glm::mat4 getProjMatrix(float fovy, float aspectRatio, float nearViewPlane, float farViewPlane);

/// Get a mat3 from a mat4
template<typename T>
glm::mat3 toMat3(const T &matrix)
{
    //const float* pSource = (const float*)glm::value_ptr(matrix);
    glm::mat3 result;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            result[i][j] = matrix[i][j];

    return result;
}

/// Print the contents of a glm::matX. Useful for debugging.
template<typename T>
void printMat(const T& matrix)
{
    //const float* pSource = (const float*)glm::value_ptr(matrix);
    glm::length_t length = matrix.length();
    float output;

    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < length; j++)
            std::cout << ((abs(matrix[i][j]) < 0.0001) ? 0 : matrix[i][j]) << "  ";
            //std::cout << ((abs(pSource[i * length + j]) < 0.0005) ? 0 : pSource[i * length + j]) << "  ";

        std::cout << std::endl;
    }
}

/// Print the contents of a glm::vecX. Useful for debugging
template<typename T>
void printVec(const T& vec)
{
    //const float* pSource = (const float*)glm::value_ptr(matrix);
    glm::length_t length = vec.length();
    float output;

    for (int i = 0; i < length; i++)
        std::cout << ((abs(vec[i]) < 0.0001) ? 0 : vec[i]) << "  ";

    std::cout << std::endl;
}

/// Print string (or any printable object)
template<typename T>
void printS(const T& vec) { std::cout << T << std::endl; }

// Vertex sets -----------------------------------------------------------------

/// Get the XYZ axis as 3 RGB lines
size_t getAxis(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float lengthFromCenter, float colorIntensity);
size_t getLongAxis(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float lengthFromCenter, float colorIntensity);

/// Get a set of lines that form a grid
size_t getGrid(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, int stepSize, size_t stepsPerSide, float height, glm::vec3 color);

/// DELETE (Local space) Get a VertexPT of a square (vertSize x horSize), its indices, and number of vertices (4). Used for draws that use MVP matrix (example: sun).
size_t getQuad(std::vector<float>& destVertex, std::vector<uint16_t>& destIndices, float vertSize, float horSize, float zValue);

/// DELETE (NDC space) Get a VertexPT of a square (vertSize x horSize), its indices, and number of vertices (4). Used for draws that doesn't use MVP matrix (example: reticule).
size_t getPlaneNDC(std::vector<float>& vertexDestination, std::vector<uint16_t>& indicesDestination, float vertSize, float horSize);

/// Get vertex data (NDC space vertices & UVs coordinates) and indices of a screen quad. Used for draws that doesn't use MVP matrix (example: reticule or postprocessing effect).
void getScreenQuad(std::vector<float>& vertices, std::vector<uint16_t>& indices, float radius, float zValue);

// Quads
extern std::vector<float> v_YZquad;
extern std::vector<uint16_t> i_quad;

/// Box
extern std::vector<float> v_skybox;
extern std::vector<uint16_t> i_skybox;
extern std::vector<float> v_skybox2;
extern std::vector<uint16_t> i_skybox2;
extern std::vector<float> v_cube;
extern std::vector<uint16_t> i_cube;

/// Get a set of vertex representing a grid square plane centered at the origin at the XY plane. 
class SqrMesh
{
public:
    SqrMesh(size_t sideCount, float sideLength);

    std::vector<float> vertices;
    std::vector<uint16_t> indices;
    size_t sideCount;               //!< Number of vertex per side
    float sideLength;               //!< Lenght of a square side
    size_t vertexCount;             //!< Total number of vertices

    static float sideFromRadius(float radius);  //!< Get the side of a square fitting inside a circle
};


// Others -----------------------------------------------------------------

extern double pi;
extern double e;

float getDist(const glm::vec3& a, const glm::vec3& b);

float getSqrDist(const glm::vec3& a, const glm::vec3& b);

glm::vec3 reflect(const glm::vec3& lightRay, const glm::vec3& normal);

// Linear interpolation. Position between A and B located at ratio t [0,1]
float lerp(float a, float b, float t);
glm::vec3 lerp(glm::vec3 a, glm::vec3 b, float t);

/// Uses linear interpolation to get an aproximation of a base raised to a float exponent
float powLinInterp(float base, float exponent);

/// This class checks if argument X (float) is bigger than argument Y (float). But if it is true once, then it will be false in all the next calls. This is useful for executing something once only after X time (used for testing in graphicsUpdate()). Example: obj.ifBigger(time, 5);
class ifOnce
{
	std::vector<float> checked;

public:
	bool ifBigger(float a, float b);
};

/// Icosahedron data (vertices, colors, indices, normals)
struct Icosahedron
{
    Icosahedron(float multiplier = 1.f);

    const int numIndicesx3  = 20 * 3;
    const int numVerticesx3 = 12 * 3;
    const int numColorsx4   = 12 * 4;

    static float vertices   [12 * 3];
    static float colors     [12 * 4];
    static unsigned indices [20 * 3];
    static float normals    [12 * 3];

    static std::vector<float> icos;     // Vertices & colors (without alpha)
    static std::vector<float> index;    // Indices
};

/// Returns true (big endian) or false (little endian).
bool isBigEndian();

/// QuickSort algorithm template (using Hoare partition scheme) for element types that support > and < comparison operators.
template <typename T>
class Quicksort_Hoare
{
	// This function takes middle element as pivot, places the pivot element at its correct position in sorted array, and places all smaller elements to left of pivot and all greater to right.
	int partition(T arr[], int low, int high)
	{
		T pivot = arr[((high - low) / 2) + low];	// The value in the middle of the array

		int i = low - 1;
		int j = high + 1;

		while (1)
		{
			do i++;
			while (arr[i] < pivot);

			do j--;
			while (arr[j] > pivot);

			if (i >= j) return j;

			swap(arr[i], arr[j]);
		}
	}

	void swap(T& a, T& b)
	{
		T temp = a;
		a = b;
		b = temp;
	}

public:
	// QuickSort implementation. Parameters: arr[] (array to be sorted), low (starting index), high (ending index).
	void quickSort(T arr[], int low, int high)
	{
		if (low >= 0 && high >= 0 && low < high)
		{
			int p = partition(arr, low, high);
			quickSort(arr, low, p);				// Note: the pivot is now included
			quickSort(arr, p + 1, high);
		}
	}
};

/// QuickSort algorithm (using Hoare partition scheme) for glm::vec3 (positions) that compares distances to camPos.
class Quicksort_distVec3
{
    // Sorting
	void quickSort(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high);
	std::vector<glm::vec3>::iterator partition(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high);

    // Swap
	void swap(glm::vec3& a, glm::vec3& b);
	glm::vec3 temp;

	// Comparison
	bool isFurther(const glm::vec3& pos1, const glm::vec3& pos2);		//!< Equivalent to (a > b) (if pos1 is further than pos2 to camPos, it's true; false otherwise)
    bool isCloser(const glm::vec3& pos1, const glm::vec3& pos2);		//!< Equivalent to (a < b) (if pos1 is closer than pos2 to camPos, it's true; false otherwise))
    glm::vec3 camPos;
	glm::vec3 vec1;
	glm::vec3 vec2;
	float sqrDist1;
	float sqrDist2;

    // Testing
    int pos(std::vector<glm::vec3>::iterator iter);                     //!< Get vector index
    std::vector<glm::vec3>::iterator begin;

public:
	void sort(std::vector<glm::vec3>::iterator low, std::vector<glm::vec3>::iterator high, const glm::vec3& camPos);
};

/// QuickSort algorithm (using Hoare partition scheme). It only sorts a vector<int> (index values) based on a vector<vec3> (positions). During comparisons, the underlying positions are taken to get distances, which are compared to camPos.
class Quicksort_distVec3_index
{
    // Sorting
    void quickSort(std::vector<int>& idx, int low, int high);
    int partition(std::vector<int>& idx, int low, int high);
    std::vector<glm::vec3>* pos;

    // Swap
    void swap(int& a, int& b);
    int temp;

    // Comparison
    bool isFurther(int idx1, int idx2);		//!< Equivalent to (a > b) (if pos1 is further than pos2 to camPos, it's true; false otherwise)
    bool isCloser(int idx1, int idx2);		//!< Equivalent to (a < b) (if pos1 is closer than pos2 to camPos, it's true; false otherwise))
    glm::vec3 camPos;
    glm::vec3 vec1;
    glm::vec3 vec2;
    float sqrDist1;
    float sqrDist2;

public:
    void sort(std::vector<glm::vec3>& positions, std::vector<int>& indexVals, const glm::vec3& camPos, int low, int high);
};

float safeMod(int a, int b);
float safeMod(float a, float b);
glm::vec3 safeMod(const glm::vec3& a, float b);

void printMessage(const char* message);

#endif