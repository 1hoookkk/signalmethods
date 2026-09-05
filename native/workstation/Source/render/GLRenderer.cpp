#include "GLRenderer.h"
#include "../ui/Style.h"

namespace ws
{
namespace
{
constexpr int kFirstGlyph = 32, kGlyphCount = 95;
constexpr unsigned int kBgra = 0x80E1, kUnpackRowLength = 0x0CF2, kClampToEdge = 0x812F, kProgramPointSize = 0x8642;

const char* const kVS =
    "attribute vec2 pos; attribute vec4 col; attribute float size; uniform vec2 vp; varying vec4 vCol;"
    "void main() { vec2 n = pos / vp * 2.0 - 1.0; gl_Position = vec4 (n.x, -n.y, 0.0, 1.0); gl_PointSize = size; vCol = col; }";

const char* const kFS =
    "varying vec4 vCol; uniform float isRound;"
    "void main() { if (isRound > 0.5) { vec2 c = gl_PointCoord - vec2 (0.5, 0.5); if (dot (c, c) > 0.25) discard; } gl_FragColor = vCol; }";

const char* const kTexVS =
    "attribute vec2 pos; attribute vec2 uv; attribute vec4 col; uniform vec2 vp; varying vec2 vUv; varying vec4 vCol;"
    "void main() { vec2 n = pos / vp * 2.0 - 1.0; gl_Position = vec4 (n.x, -n.y, 0.0, 1.0); vUv = uv; vCol = col; }";

const char* const kTexFS =
    "varying vec2 vUv; varying vec4 vCol; uniform sampler2D tex; uniform float coverage;"
    "void main() { vec4 t = texture2D (tex, vUv); gl_FragColor = coverage > 0.5 ? vec4 (vCol.rgb, vCol.a * t.a) : vec4 (t.rgb, t.a); }";

int glyphIndex (juce::juce_wchar c) { return c >= kFirstGlyph && c < kFirstGlyph + kGlyphCount ? (int) c - kFirstGlyph : (int) '?' - kFirstGlyph; }
}

void GLRenderer::create (juce::OpenGLContext& ctx)
{
    using namespace juce::gl;
    marks = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    marks->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (kVS));
    marks->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (kFS));
    marks->link();
    textured = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    textured->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (kTexVS));
    textured->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (kTexFS));
    textured->link();
    glGenBuffers (1, &vbo);
    glGenTextures (1, &atlas);
    glGenTextures (1, &picture);
    atlasScale = 0.0f;
    pictureSource = {};
}

void GLRenderer::destroy()
{
    using namespace juce::gl;
    if (vbo != 0) glDeleteBuffers (1, &vbo);
    if (atlas != 0) glDeleteTextures (1, &atlas);
    if (picture != 0) glDeleteTextures (1, &picture);
    vbo = atlas = picture = 0;
    marks.reset();
    textured.reset();
}

void GLRenderer::upload (unsigned int tex, const juce::Image& source)
{
    using namespace juce::gl;
    const auto img = source.convertedToFormat (juce::Image::ARGB);
    juce::Image::BitmapData bits (img, juce::Image::BitmapData::readOnly);
    glBindTexture (GL_TEXTURE_2D, tex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei (kUnpackRowLength, bits.lineStride / 4);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, bits.width, bits.height, 0, kBgra, GL_UNSIGNED_BYTE, bits.data);
    glPixelStorei (kUnpackRowLength, 0);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) kClampToEdge);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) kClampToEdge);
    glBindTexture (GL_TEXTURE_2D, 0);
}

void GLRenderer::buildAtlas (float size, float scale)
{
    const auto font = mono (size * scale);
    cellW = std::ceil (juce::GlyphArrangement::getStringWidth (font, "M")) + 1.0f;
    cellH = std::ceil (font.getHeight()) + 2.0f;
    atlasW = (int) cellW * kGlyphCount;
    atlasH = (int) cellH;
    juce::Image img (juce::Image::ARGB, atlasW, atlasH, true);
    {
        juce::Graphics g (img);
        g.setFont (font);
        g.setColour (juce::Colours::white);
        for (int i = 0; i < kGlyphCount; ++i)
            g.drawText (juce::String::charToString ((juce::juce_wchar) (kFirstGlyph + i)), juce::Rectangle<int> ((int) (i * cellW), 0, (int) cellW, (int) cellH), juce::Justification::centred, false);
    }
    upload (atlas, img);
    atlasScale = scale;
    atlasSize = size;
}

void GLRenderer::draw (const std::vector<Batch>& batches, float width, float height, float scale, juce::Colour ground)
{
    using namespace juce::gl;
    juce::OpenGLHelpers::clear (ground);
    if (marks == nullptr || marks->getProgramID() == 0 || textured == nullptr || textured->getProgramID() == 0) return;
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (kProgramPointSize);
    glDisable (GL_DEPTH_TEST);
    for (const auto& b : batches)
    {
        if (b.kind == Batch::text) drawText (b, width, height, scale);
        else if (b.kind == Batch::image) drawImage (b, width, height, scale);
        else if (! b.v.empty())
        {
            marks->use();
            juce::OpenGLShaderProgram::Uniform (*marks, "vp").set (width * scale, height * scale);
            drawSolid (b, scale);
        }
    }
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void GLRenderer::drawSolid (const Batch& b, float scale)
{
    using namespace juce::gl;
    std::vector<Vertex> scaled;
    if (b.kind == Batch::rects)
    {
        for (size_t i = 0; i + 1 < b.v.size(); i += 2)
        {
            const Vertex a = b.v[i], c = b.v[i + 1];
            const Vertex tl { a.x * scale, a.y * scale, a.r, a.g, a.b, a.a, 0.0f }, br { c.x * scale, c.y * scale, a.r, a.g, a.b, a.a, 0.0f };
            const Vertex tr { br.x, tl.y, a.r, a.g, a.b, a.a, 0.0f }, bl { tl.x, br.y, a.r, a.g, a.b, a.a, 0.0f };
            scaled.insert (scaled.end(), { tl, tr, br, tl, br, bl });
        }
    }
    else
    {
        scaled = b.v;
        for (auto& x : scaled) { x.x *= scale; x.y *= scale; x.size *= scale; }
    }
    if (scaled.empty()) return;
    const juce::OpenGLShaderProgram::Uniform isRound (*marks, "isRound");
    const juce::OpenGLShaderProgram::Attribute pos (*marks, "pos"), col (*marks, "col"), size (*marks, "size");
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (scaled.size() * sizeof (Vertex)), scaled.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer ((GLuint) pos.attributeID, 2, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) 0);
    glEnableVertexAttribArray ((GLuint) pos.attributeID);
    glVertexAttribPointer ((GLuint) col.attributeID, 4, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) (2 * sizeof (float)));
    glEnableVertexAttribArray ((GLuint) col.attributeID);
    glVertexAttribPointer ((GLuint) size.attributeID, 1, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) (6 * sizeof (float)));
    glEnableVertexAttribArray ((GLuint) size.attributeID);
    isRound.set (b.round ? 1.0f : 0.0f);
    if (b.kind == Batch::points) glDrawArrays (GL_POINTS, 0, (GLsizei) scaled.size());
    else if (b.kind == Batch::rects) glDrawArrays (GL_TRIANGLES, 0, (GLsizei) scaled.size());
    else
    {
        glLineWidth (scaled[0].size);
        glDrawArrays (b.kind == Batch::lines ? GL_LINES : GL_LINE_STRIP, 0, (GLsizei) scaled.size());
    }
    glDisableVertexAttribArray ((GLuint) pos.attributeID);
    glDisableVertexAttribArray ((GLuint) col.attributeID);
    glDisableVertexAttribArray ((GLuint) size.attributeID);
}

void GLRenderer::drawTextured (const std::vector<TexVertex>& quads, unsigned int tex, bool coverage, float width, float height, float scale)
{
    using namespace juce::gl;
    if (quads.empty()) return;
    textured->use();
    juce::OpenGLShaderProgram::Uniform (*textured, "vp").set (width * scale, height * scale);
    juce::OpenGLShaderProgram::Uniform (*textured, "coverage").set (coverage ? 1.0f : 0.0f);
    juce::OpenGLShaderProgram::Uniform (*textured, "tex").set (0);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, tex);
    const juce::OpenGLShaderProgram::Attribute pos (*textured, "pos"), uv (*textured, "uv"), col (*textured, "col");
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (quads.size() * sizeof (TexVertex)), quads.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer ((GLuint) pos.attributeID, 2, GL_FLOAT, GL_FALSE, sizeof (TexVertex), (void*) 0);
    glEnableVertexAttribArray ((GLuint) pos.attributeID);
    glVertexAttribPointer ((GLuint) uv.attributeID, 2, GL_FLOAT, GL_FALSE, sizeof (TexVertex), (void*) (2 * sizeof (float)));
    glEnableVertexAttribArray ((GLuint) uv.attributeID);
    glVertexAttribPointer ((GLuint) col.attributeID, 4, GL_FLOAT, GL_FALSE, sizeof (TexVertex), (void*) (4 * sizeof (float)));
    glEnableVertexAttribArray ((GLuint) col.attributeID);
    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) quads.size());
    glDisableVertexAttribArray ((GLuint) pos.attributeID);
    glDisableVertexAttribArray ((GLuint) uv.attributeID);
    glDisableVertexAttribArray ((GLuint) col.attributeID);
    glBindTexture (GL_TEXTURE_2D, 0);
}

void GLRenderer::drawText (const Batch& b, float width, float height, float scale)
{
    std::vector<TexVertex> quads;
    for (const auto& t : b.texts)
    {
        if (atlasScale != scale || atlasSize != t.size) buildAtlas (t.size, scale);
        const int fit = std::max (0, (int) std::floor (t.box.getWidth() * scale / cellW));
        const auto s = t.s.substring (0, fit);
        const float w = (float) s.length() * cellW;
        float x = t.box.getX() * scale;
        if (t.just.testFlags (juce::Justification::horizontallyCentred)) x = t.box.getCentreX() * scale - w * 0.5f;
        else if (t.just.testFlags (juce::Justification::right)) x = t.box.getRight() * scale - w;
        const float y = std::round (t.box.getCentreY() * scale - cellH * 0.5f);
        x = std::round (x);
        const float r = t.colour.getFloatRed(), g = t.colour.getFloatGreen(), bl = t.colour.getFloatBlue(), a = t.colour.getFloatAlpha();
        for (int i = 0; i < s.length(); ++i)
        {
            const int gi = glyphIndex (s[i]);
            const float u0 = gi * cellW / (float) atlasW, u1 = (gi + 1) * cellW / (float) atlasW;
            const float x0 = x + i * cellW, x1 = x0 + cellW, y0 = y, y1 = y + cellH;
            quads.insert (quads.end(), { { x0, y0, u0, 0.0f, r, g, bl, a }, { x1, y0, u1, 0.0f, r, g, bl, a }, { x1, y1, u1, 1.0f, r, g, bl, a },
                                         { x0, y0, u0, 0.0f, r, g, bl, a }, { x1, y1, u1, 1.0f, r, g, bl, a }, { x0, y1, u0, 1.0f, r, g, bl, a } });
        }
    }
    drawTextured (quads, atlas, true, width, height, scale);
}

void GLRenderer::drawImage (const Batch& b, float width, float height, float scale)
{
    if (! b.img.isValid()) return;
    if (pictureSource.getPixelData() != b.img.getPixelData()) { upload (picture, b.img); pictureSource = b.img; }
    const float x0 = b.box.getX() * scale, y0 = b.box.getY() * scale, x1 = b.box.getRight() * scale, y1 = b.box.getBottom() * scale;
    const std::vector<TexVertex> quad { { x0, y0, 0.0f, 0.0f, 1, 1, 1, 1 }, { x1, y0, 1.0f, 0.0f, 1, 1, 1, 1 }, { x1, y1, 1.0f, 1.0f, 1, 1, 1, 1 },
                                        { x0, y0, 0.0f, 0.0f, 1, 1, 1, 1 }, { x1, y1, 1.0f, 1.0f, 1, 1, 1, 1 }, { x0, y1, 0.0f, 1.0f, 1, 1, 1, 1 } };
    drawTextured (quad, picture, false, width, height, scale);
}
}
