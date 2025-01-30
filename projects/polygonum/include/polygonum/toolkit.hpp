#ifndef AUXILIARY_HPP
#define AUXILIARY_HPP

#include <array>
#include <chrono>

#include "polygonum/commons.hpp"

/*
  - Print data
  - MVP Matrix
  - Vertex sets
  - Maths
  - Timer
  - Algorithms
  - Data structures
*/


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

    for (int i = 0; i < length; i++)
        std::cout << ((abs(vec[i]) < 0.0001) ? 0 : vec[i]) << "  ";

    std::cout << std::endl;
}

/// Print the contents of a T[3] array. Useful for debugging
template<typename T>
void printVec(const T& vec, unsigned length)
{
	for (int i = 0; i < length; i++)
		std::cout << ((abs(vec[i]) < 0.0001) ? 0 : vec[i]) << "  ";

	std::cout << std::endl;
}

/// Print string (or any printable object)
template<typename T>
void printS(const T& vec) { std::cout << T << std::endl; }

struct Plane;
struct Frustum;
struct BoundingShape;
  struct Sphere;
  struct AABB;

// ADT for bounding shapes (usually, polygons) used for enveloping objects.
struct BoundingShape
{
	virtual bool isInFrustum(const Frustum& frustumPlanes) const = 0;
};

// Bounding point
struct Point : BoundingShape
{
	Point();
	Point(glm::vec3 point);

	bool isInFrustum(const Frustum& frustumPlanes) const override;   //!< Check if a sphere appears in a frustum. Used for frustum culling. True if sphere is inside or intersects the frustum; false otherwise.
	void setValues(glm::vec3 point);

	glm::vec3 point;
};

// Bounding sphere
struct Sphere : BoundingShape
{
	Sphere();
	Sphere(glm::vec3 center, float radius);

	bool isInFrustum(const Frustum& frustumPlanes) const override;   //!< Check if a sphere appears in a frustum. Used for frustum culling. True if sphere is inside or intersects the frustum; false otherwise.
	void setValues(glm::vec3 center, float radius);

	glm::vec3 center;
	float radius;
};

/// Axis Aligned Bounding Box (AABB).
struct AABB : BoundingShape
{
	AABB();
	AABB(glm::vec3 min, glm::vec3 max);

	bool isInFrustum(const Frustum& frustumPlanes) const override;   //!< Check if an AABB appears in a frustum. Used for frustum culling. True if AABB is inside or intersects the frustum; false otherwise.
	void setValues(glm::vec3 min, glm::vec3 max);
	void updateAABB(const std::vector<float>& pos, float stride);   //!< Given a set of points, resize current aabb so it can contain them too.
	glm::vec3 getMostNormalAlignedCorner(const glm::vec3& planeNormal) const;   //!< AABB corner that is the most aligned with the direction of the plane's normal vector.

	glm::vec3 min, max;
	bool isInitialized;
};

struct Plane
{
	Plane();
	Plane(glm::vec3 normal, float distance);

	float distanceToPoint(const glm::vec3& point) const;   //!< Get distance from a point to this plane.

	glm::vec3 normal;
	float dist;
};

/// Contains the 6 planes of a frustum, and can check if an object is inside/outside it.
struct Frustum
{
	std::array<Plane, 6> planes;   //!< right, left, top, bottom, near, far

	void setPlanes(const glm::mat4& view, const glm::mat4& proj);   //!< Get frustum planes from a View and Projection matrix.

	bool isInFrustum(const glm::vec3& point, float distBeyond = 0) const;   //!< Check if a point appears in a frustum, or within a distance beyond it. Used for frustum culling. 
	bool isInFrustum(const AABB& aabb) const;   //!< Check if an AABB appears in a frustum. Used for frustum culling. True if AABB is inside or intersects the frustum; false otherwise.
	bool isInFrustum(const Sphere& sphere) const;   //!< Check if a sphere appears in a frustum. Used for frustum culling. True if sphere is inside or intersects the frustum; false otherwise.
};


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


// Maths -----------------------------------------------------------------

extern double pi;
extern double e;

float getDist(const glm::vec3& a, const glm::vec3& b);

float getSqrDist(const glm::vec3& a, const glm::vec3& b);

glm::vec3 unitVec(glm::vec3& vec);

glm::vec3 reflect(const glm::vec3& lightRay, const glm::vec3& normal);

// Linear interpolation. Position between A and B located at ratio t [0,1]. Example: 3-4-5-6-7-8; Pos. at ratio 0.6 = 3 + (8 - 3) * 0.6 = 6
float lerp(float a, float b, float t);
glm::vec3 lerp(glm::vec3 a, glm::vec3 b, float t);

// Ratio. Ratio between A and B at position C. Example: 3-4-5-6-7-8; Ratio at 6 = (6-3)/(8-3) = 0.6
float ratio(float a, float b, float c);

/// Uses linear interpolation to get an aproximation of a base raised to a float exponent
float powLinInterp(float base, float exponent);

/// Get sign of a given value. 
template <typename T>
int sign(T val) { return (T(0) < val) - (val < T(0)); }

/// Power of an integer to the power to an unsigned. Returns 1 if exponent is 0.
int ipow(int base, unsigned exp);

/// If input == 0, output == 1; otherwise, output == 0.
int opposite01(int val);

/// Apend an integer digit to an integer.
uint64_t appendInt(uint64_t first, uint64_t second);

/// Get area of a sphere.
float getSphereArea(float radius);

/// Get modulus safely (if b == 0, this returns 0).
float safeMod(int a, int b);
float safeMod(float a, float b);
glm::vec3 safeMod(const glm::vec3& a, float b);

float getSlope(const glm::vec3& groundNormal, const glm::vec3& upNormal);


// Timer -----------------------------------------------------------------

/**
*   Class for getting delta time and counting. Useful for getting frame's delta time and counting frames.
*       <ul>
*           <li>Delta time: Delta time between the last 2 consecutive calls to updateTime.</li>
*           <li>Total delta time: Delta time between startTime and the last call to updateTime.</li>
*       </ul>
*/
class Timer
{
	std::chrono::high_resolution_clock::time_point startTime;   //!< Time at startTimer call
	std::chrono::high_resolution_clock::time_point currentTime;   //!< Time at last updateTime call
	std::chrono::high_resolution_clock::time_point prevTime;   //!< Time at second last updateTime call
	long double deltaTime;   //!< Delta time between two consecutive calls to updateTime.
	long double totalDeltaTime;   //!< Delta time between startTimer and updateTime.

public:
	Timer();   //!< Starts chronometer (calls startTimer())

	void startTimer();   //!< Restart chronometer (startTime)
	long double updateTime();   //!< Update time parameters with respect to currentTime. Returns deltaTime.
	long double reUpdateTime();   //!< Re-update time parameters as if updateTime was not called before. Returns deltaTime.

	long double getDeltaTime() const;
	long double getTotalDeltaTime() const;
	std::string getDate();   //!< Get a string with current date and time (example: Mon Jan 31 02:28:35 2022)
};

void sleep(int milliseconds);

void waitForFPS(Timer& timer, int maxFPS);


// Algorithms -----------------------------------------------------------------

/// Returns true (big endian) or false (little endian).
bool isBigEndian();

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


// Data structures -----------------------------------------------------------------

/// Generic Binary Search Tree Node (4 nodes).
template<typename T>
class QuadNode
{
public:
	//QuadNode() { };
	QuadNode(const T& element, QuadNode* a = nullptr, QuadNode* b = nullptr, QuadNode* c = nullptr, QuadNode* d = nullptr) : element(element), a(a), b(b), c(c), d(d) { };
	~QuadNode() { if (a) delete a; if (b) delete b; if (c) delete c; if (d) delete d; };

	void setElement(const T& newElement) { element = newElement; }
	void setA(QuadNode<T>* node) { a = node; }
	void setB(QuadNode<T>* node) { b = node; }
	void setC(QuadNode<T>* node) { c = node; }
	void setD(QuadNode<T>* node) { d = node; }

	T& getElement() { return element; }
	QuadNode<T>* getA() { return a; }
	QuadNode<T>* getB() { return b; }
	QuadNode<T>* getC() { return c; }
	QuadNode<T>* getD() { return d; }

	bool isLeaf() { return !(a || b || c || d); }	//!< Is leaf if all subnodes are null; otherwise, it's not. Full binary tree: Every node either has zero children [leaf node] or two children. All leaf nodes have an element associated. There are no nodes with only one child. Each internal node has exactly two children.
	//bool isLeaf_BST() { return (a); }	//!< For simple Binary Trees (BT).

private:
	// Ways to deal with keys and comparing records: (1) Key / value pairs (our choice), (2) Especial comparison method, (3) Passing in a comparator function.
	T element;
	QuadNode<T>* a, * b, * c, * d;
};


template<typename T, typename V, typename X>
void preorder(QuadNode<T>* root, V* visitor, X* external)
{
	if (root == nullptr) return;
	visitor(root, external);
	preorder(root->getA());
	preorder(root->getB());
	preorder(root->getC());
	preorder(root->getD());
}

template<typename T, typename V, typename X>
void postorder(QuadNode<T>* root, V* visitor, X* external)
{
	if (root == nullptr) return;
	postorder(root->getA());
	postorder(root->getB());
	postorder(root->getC());
	postorder(root->getD());
	visitor(root, external);
}

template<typename T, typename V, typename X>
void inorder(QuadNode<T>* root, V* visitor, X* external)
{
	if (root == nullptr) return;
	inorder(root->getA());
	visitor(root, external);
	inorder(root->getB());
	inorder(root->getC());
	inorder(root->getD());
}

template<typename T, typename X>
void getAllElements(QuadNode<T>* node, X* external)
{
	external->push_back(node);
}

template<typename T, typename X>
void getLeaves(QuadNode<T>* node, X* external)
{
	if (node->isLeaf())
		external->push_back(node);
}

template<typename T, typename X>
void getDepthElements(QuadNode<T>* node, X* external)
{
	//if(node->getElement().depth == std::get<1>(*external))
	//	std::get<0>(*external).push_back(node);
}

/// Data structure that stores pointers. When one of them is no longer used elsewhere, it's deleted from storage. The custom deleter used requires E to know its key and the PointersManager, which can be done by making E inherit from InterfaceForPointersManagerElements.
template<typename K, typename E>
class PointersManager
{
	std::unordered_map<K, std::weak_ptr<E>> elements;

public:
	PointersManager() { };
	~PointersManager() { };

	template<typename... Args>
	std::shared_ptr<E> emplace(K key, Args&&... args)   // Variadic template constructor. Arguments are forwarded to T's constructor.
	{
		std::shared_ptr<E> newElement(new E(std::forward<Args>(args)...), PointersManager::customDeleter);
		newElement->setValues(this, key);
		elements[key] = newElement;
		return newElement;
	}

	std::shared_ptr<E> get(K key) { return elements[key].lock(); }

	bool contains(K key) { return elements.find(key) != elements.end(); }

	size_t size() { return elements.size(); }

	static void customDeleter(E* elemPtr)
	{
		elemPtr->pointersManager->elements.erase(elemPtr->id);
		delete elemPtr;
	}
};

template<typename K, typename E>
class InterfaceForPointersManagerElements
{
public:
	virtual ~InterfaceForPointersManagerElements() { }

	void setValues(PointersManager<K, E>* pointersManager, K& key)
	{
		this->pointersManager = pointersManager;
		this->key = key;
	}

	PointersManager<K, E>* pointersManager;
	K key;
};

#endif