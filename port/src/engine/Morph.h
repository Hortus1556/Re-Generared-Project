// Morphing 3D blobs (port of the C++ classes Morph and Shape, and the mesh
// part of ResourceModel / A3ICPODScene).
//
// A Shape is one mesh of a PowerVR .POD file (vertex positions and a
// triangle list; the game only uses those). A Morph is a list of shapes with
// key frame times; it plays through them, interpolating vertex positions
// between the current and next shape, and draws the result untextured in
// one flat colour. The poison blobs are Morphs.
#pragma once

#include <string>
#include <vector>

struct Color4 {
    float r = 1, g = 1, b = 1, a = 1;
};

class Shape {
public:
    Shape(const std::string& podFile, int meshIndex);  // Shape(NSString*, int, VECTOR4f)

    // Extents of the raw mesh (x and y), as ints like the original.
    void generateAABB();
    int maxX = 0, minX = 0, maxY = 0, minY = 0;

    int numVertex = 0;
    int components = 3;
    std::vector<float> vertices;           // numVertex * components
    std::vector<unsigned short> indices;   // triangles
};

class Morph {
public:
    // Two key shapes: name1/mesh1 at frame t1, name2/mesh2 at frame t2.
    Morph(const std::string& name1, int mesh1, int t1, const std::string& name2, int mesh2, int t2, Color4 color);
    ~Morph();
    Morph(const Morph&) = delete;
    Morph& operator=(const Morph&) = delete;

    void add(const std::string& name, int mesh, int time);
    // 0 loops; 1 plays to the end and holds; 2 plays once and rewinds.
    void setLoopingState(int s) { loopingState = s; }
    void setPosition(float x, float y, float z) {
        posX = x;
        posY = y;
        posZ = z;
    }
    void setOrientation(float o) { orientation = o; }
    void setScale(float x, float y, float z) {
        scaleX = x;
        scaleY = y;
        scaleZ = z;
    }
    void nextFrame();
    void draw();
    // Extents of the current shape, updated by draw().
    int getMaxX() const { return maxX; }
    int getMinX() const { return minX; }
    int getMaxY() const { return maxY; }
    int getMinY() const { return minY; }

private:
    std::vector<Shape*> shapes;
    std::vector<int> times;
    int frame = 0, current = 0;
    Color4 color;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    float posX = 0, posY = 0, posZ = 0;
    float orientation = 0;
    int loopingState = 0;
    bool finished = false;
    int maxX = 0, minX = 0, maxY = 0, minY = 0;
};
