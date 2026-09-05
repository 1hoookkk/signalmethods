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
    Chord chord {};
    Words words {};
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

struct Item
{
    int face = -1, stub = -1;
    int group = 0;
    Vec3 p;
};

struct Spot
{
    int node = -1, edge = -1, face = -1, stub = -1;
    double t = 0.0, m = 0.0, q = 0.0;
    bool free = false;
    double x = 0.0, y = 0.0, z = 0.5;
    bool valid() const { return node >= 0 || edge >= 0 || face >= 0 || stub >= 0 || free; }
    bool operator== (const Spot& o) const { return node == o.node && edge == o.edge && face == o.face && stub == o.stub && t == o.t && m == o.m && q == o.q && free == o.free && x == o.x && y == o.y && z == o.z; }
};

struct Near { Item item; double weight; };

class Stitch
{
public:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Stub> stubs;
    std::vector<Vec3> centres;
    std::vector<Item> items;
    int floorCount = 7;

    bool loadJson (const juce::File& file);
    Words wordsOf (int node) const;
    std::array<Words, 4> cornersOf (int face) const;
    Words wordsOfItem (const Item& it) const;
    juce::String nameOfItem (const Item& it) const;
    std::vector<Near> nearest (const Vec3& at, int count, const std::array<bool, kGroups>& open) const;
    Morph soundAt (const Spot& s, const std::array<bool, kGroups>& open) const;
    Vec3 positionOf (const Spot& s) const;
    juce::String nameOf (const Spot& s, const std::array<bool, kGroups>& open) const;
    int addStub (const juce::String& name, const Words& words, int node, int floor);
    int nearestNode (const Words& words) const;
    Spot lerp (const Spot& a, const Spot& b, double f) const;
    void placeOnGrid();
    bool sharesNode (int faceA, int faceB) const;
    std::vector<int> neighbours (int face) const;
    static Vec3 gridPlace (const Words& words);
};
}
