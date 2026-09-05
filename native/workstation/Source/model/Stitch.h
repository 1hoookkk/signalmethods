#pragma once

#include "Frame.h"
#include "Morph.h"
#include <array>
#include <vector>

namespace ws
{
struct Vec3 { double x = 0.0, y = 0.0, z = 0.0; };

struct Node
{
    int floor = 0;
    double datum = kDatumHz;
    std::vector<std::array<std::uint16_t, kWords>> rows;
    int faces = 0, members = 0;
    Vec3 p;
    int frame = -1;
};

struct Edge { int a = 0, b = 0, body = 0, floor = 0; int axis = 0; };

struct Face
{
    juce::String name;
    int floor = 0;
    double datum = kDatumHz;
    std::vector<int> nodes;
};

struct Stub
{
    juce::String name;
    int floor = 0;
    Words words {};
    int node = 0;
    Vec3 p;
    int frame = -1;
};

struct Spot
{
    int node = -1, edge = -1, face = -1, stub = -1;
    double t = 0.0, m = 0.0, q = 0.0;
    bool valid() const { return node >= 0 || edge >= 0 || face >= 0 || stub >= 0; }
    bool operator== (const Spot& o) const { return node == o.node && edge == o.edge && face == o.face && stub == o.stub && t == o.t && m == o.m && q == o.q; }
};

class Stitch
{
public:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Stub> stubs;
    double floorGap = 1.0;
    int floorCount = 7;
    std::vector<int> usedFloors;

    bool loadJson (const juce::File& file);
    Words wordsOf (int node) const;
    std::array<Words, 4> cornersOf (int face) const;
    Morph soundAt (const Spot& s) const;
    Vec3 positionOf (const Spot& s) const;
    double floorZ (int floor) const
    {
        for (int i = 0; i < (int) usedFloors.size(); ++i) if (usedFloors[(size_t) i] == floor) return i * floorGap;
        return usedFloors.empty() ? 0.0 : (double) (usedFloors.size() - 1) * floorGap;
    }
    double topZ() const { return usedFloors.empty() ? 0.0 : (double) (usedFloors.size() - 1) * floorGap; }
    juce::String nameOf (const Spot& s) const;
    int addStub (const juce::String& name, const Words& words, const Spot& near, int floor);
    Spot lerp (const Spot& a, const Spot& b, double f) const;
    int nearestNode (const Words& words) const;
    void placeOnGrid();
    static Vec3 gridPlace (const Words& words, double z);
};
}
