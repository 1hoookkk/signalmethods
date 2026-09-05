#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Scene.h"
#include <memory>

namespace ws
{
class GLRenderer
{
public:
    void create (juce::OpenGLContext& ctx);
    void destroy();
    void draw (const std::vector<Batch>& batches, float width, float height, float scale);

private:
    std::unique_ptr<juce::OpenGLShaderProgram> marks;
    unsigned int vbo = 0;
};
}
