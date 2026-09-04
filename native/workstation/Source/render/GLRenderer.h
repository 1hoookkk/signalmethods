#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Scene.h"
#include "../model/Frame.h"
#include <memory>

namespace ws
{
class GLRenderer
{
public:
    void create (juce::OpenGLContext& ctx);
    void destroy();
    void uploadWords (const std::vector<Frame>& frames);
    void draw (const FieldMesh& field, const std::vector<Batch>& batches, float width, float height, float scale);

private:
    std::unique_ptr<juce::OpenGLShaderProgram> marks, surface;
    unsigned int vbo = 0, tex = 0;
    int texRows = 0;
};
}
