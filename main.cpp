#include "lodepng/lodepng.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

/* -------------------------------------------------------------------------- */
/* GLFW CALLBACKS                                                             */
/* -------------------------------------------------------------------------- */

static void 
_ErrorCallback(int error, const char* description)
{
    std::cerr << "GLFW Error[" << error << "]: " << description << "\n";
}

static void 
_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) 
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}


/* -------------------------------------------------------------------------- */
/* GL HELPERS                                                                 */
/* -------------------------------------------------------------------------- */

GLuint _vao = 0;
GLuint _quadBuffer = 0;

static void
_GLCheckError(std::string const & where = "")
{
    GLuint err;
    int count = 0;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "GL error: "
                  << (where.empty() ? "" : where + " ")
                  << err << std::endl;
        count++;
    }
    if (count)
        exit(EXIT_FAILURE);
}

static void
_GLSetCoreProfile() 
{
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
}

static void
_GLEWInit()
{
    glewExperimental = true;
    if (GLenum r = glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize glew. Error = %s\n", glewGetErrorString(r);
        exit(EXIT_FAILURE);
    }
    // Glew causes GL errors :(
    glGetError();
}

static GLuint
_GLCompileShader(GLenum shaderType, const char *source)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    char log[1024];
    GLsizei length;
    glGetShaderInfoLog(shader, 1024, &length, log);
    if (length > 0) {
        std::cerr << "Failed to compile: " << source << "\n\n" << log << std::endl;
        exit(EXIT_FAILURE);
    }
    _GLCheckError("_GLCompileShader");
    return shader;
}

static GLuint 
_GLLinkProgram(char const* vsSrc, char const* fsSrc) {


    GLuint program = glCreateProgram();
    GLuint vertexShader = _GLCompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fragmentShader = _GLCompileShader(GL_FRAGMENT_SHADER, fsSrc);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    //glBindAttribLocation (program, 0, "position");
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
        char *infoLog = new char[infoLogLength];
        glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);
        std::cerr << "Shader link failed: " << infoLog << "\n";
        delete[] infoLog;
        exit(EXIT_FAILURE);
    }

    return program;
}

struct {
    GLuint program;
    GLint iResolutionLoc;
    GLint iGlobalTimeLoc;
    GLint iMouseLoc;
    GLint iChannel0Loc;
} _shaderToy;

static void 
_LinkSTProgram()
{
    #define GLSL_VERSION_DEFINE "#version 410\n"

    static const char *vsSrc =
        GLSL_VERSION_DEFINE
        "layout(location=0) in vec2 position;\n"
        "out vec4 fragColor;\n"
        "out vec2 uvCoord;\n"
        "void main() {\n"
        "  fragColor = vec4((position + 1)*.5*0.75, 0.0, 1);\n"
        "  uvCoord = (position + 1)*.5;\n"
        "  gl_Position = vec4(position, 0.0, 1);\n"
        "}\n";

    /* Required ShaderToy inputs:
    uniform vec3      iResolution;           // viewport resolution (in pixels)
    uniform float     iGlobalTime;           // shader playback time (in seconds)
    uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
    uniform sampler2D iChannel0;           // input channel. XX = 2D/Cube
    */
    
    static const std::string fsSrc =
        GLSL_VERSION_DEFINE
        "in vec4 fragColor;\n"
        "in vec2 uvCoord;\n"
        "out vec4 color;\n"
        "uniform vec3      iResolution;           // viewport resolution (in pixels)\n"
        "uniform float     iGlobalTime;           // shader playback time (in seconds)\n"
        "//uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click\n"
        "uniform sampler2D iChannel0;           // input channel. XX = 2D/Cube\n"

        "//void main() {\n"
        "//  color = fragColor + vec4(vec3(iGlobalTime*.1), 1.0);\n"
        "//  color = vec4(vec3(texture(iChannel0, vec2(uvCoord.x, 1-uvCoord.y), 0).r), 1);\n"
        "//  color = vec4(vec3(uvCoord, 0), 1);\n"
        "//}\n";

    std::string line;
    std::stringstream ss;
    std::ifstream glslFile("dunes.glsl");
    ss << fsSrc;
    if (glslFile.is_open()) {
        #if 1
        while (getline(glslFile,line)) {
            ss << line << "\n";
        }
        #endif
        glslFile.close();
    }

    _shaderToy.program = _GLLinkProgram(vsSrc, ss.str().c_str());
    _shaderToy.iResolutionLoc = glGetUniformLocation(_shaderToy.program, "iResolution");
    _shaderToy.iGlobalTimeLoc = glGetUniformLocation(_shaderToy.program, "iGlobalTime");
    _shaderToy.iChannel0Loc = glGetUniformLocation(_shaderToy.program, "iChannel0");
    //_shaderToy.iMouseLoc = glGetUniformLocation(_shaderToy.program, "iMouse");
}

static void
_GLInit()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);
    _GLCheckError("Setup state");

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    // Setup a full-screen quad in clip coordinates as two CCW 2-dimensional
    // triangles
    glGenBuffers(1, &_quadBuffer);
    _GLCheckError("GenQuadBuffer");
    float quadData[] = { -1,-1,   1,-1,  -1, 1,
                          1, 1,  -1, 1,   1,-1  };
    glBindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_READ);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    _GLCheckError("BufferData");

    _LinkSTProgram();
}

GLuint _fbo;
GLuint _rboDepth;
GLuint _rboColor;

static void
_InitFBO(GLsizei width, GLsizei height)
{
    glGenFramebuffers(1, &_fbo);
    glGenRenderbuffers(1, &_rboDepth);
    glGenRenderbuffers(1, &_rboColor);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    
    glBindRenderbuffer(GL_RENDERBUFFER, _rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, _rboDepth);

    glBindRenderbuffer(GL_RENDERBUFFER, _rboColor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, _rboColor);

    _GLCheckError("FBO");

    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
        std::cerr << "Invalid frame buffer: Incomplete attachment\n";
        exit(EXIT_FAILURE);
    }
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: {
        std::cerr << "Invalid frame buffer: missing attachment\n";
        exit(EXIT_FAILURE);
    }
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: {
        std::cerr << "Invalid frame buffer: incomplete draw buffer\n";
        exit(EXIT_FAILURE);
    }
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: {
        std::cerr << "Invalid frame buffer: incomplete read buffer\n";
        exit(EXIT_FAILURE);
    }
    case GL_FRAMEBUFFER_UNSUPPORTED: {
        std::cerr << "Invalid frame buffer: unsupported\n";
        exit(EXIT_FAILURE);
    }
    default:
        exit(EXIT_FAILURE);
    }


    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void
_InitRandomTexture()
{
    std::vector<unsigned char> data;
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;

    // decode
    lodepng::load_file(data, "tex12.png");
    unsigned error = lodepng::decode(image, width, height, data, LCT_GREY, 8);

    // if there's an error, display it
    if(error) 
        std::cerr << "decoder error " << error << ": " 
                  << lodepng_error_text(error) << std::endl;

    //the pixels are now in the vector "image", 4 bytes per pixel, ordered
    //RGBARGBA..., use it as texture, draw it, ...

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED,
                 GL_UNSIGNED_BYTE, &image[0]);
}

/* -------------------------------------------------------------------------- */
/* MAIN                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
    glfwSetErrorCallback(_ErrorCallback);
    if (!glfwInit())
        exit(EXIT_FAILURE);
    _GLSetCoreProfile();

    int width=1024, height=640;
    int widthFbo=width/2, heightFbo=height/2;
    GLFWwindow* window;
    
    // Must match FBO samples to use glBlitFramebuffer
    //glfwWindowHint(GLFW_SAMPLES, 1);

    // Setup native resolution full-screen; use FBO to downsample the 
    // render resolution. This looks better and doesn't cause the window 
    // manager to freak out due to resolution changes.
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    width = mode->width;
    height = mode->height;
    //window = glfwCreateWindow(width, height, "NVScene15", NULL, NULL);
    window = glfwCreateWindow(width, height, "NVScene15", glfwGetPrimaryMonitor(), NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwMakeContextCurrent(window);

    GLint major=0, minor=0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    _GLCheckError("Check version");
    std::cout << "OpenGL " << major << "." << minor << std::endl;

    _GLEWInit();
    _GLInit();
    _InitFBO(widthFbo, heightFbo);
    _InitRandomTexture();

    glfwSetKeyCallback(window, _KeyCallback);
    glfwSwapInterval(0);
    
    while (!glfwWindowShouldClose(window))
    {
        float ratio;
        glfwGetFramebufferSize(window, &width, &height);
        ratio = widthFbo / (float) heightFbo;

        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, widthFbo, heightFbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(_shaderToy.program);
        glUniform1f(_shaderToy.iGlobalTimeLoc, glfwGetTime());
        glUniform1i(_shaderToy.iChannel0Loc, 0);
        glUniform3f(_shaderToy.iResolutionLoc, widthFbo, heightFbo, 1.0);
        glBindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(/*attrib*/0, /*vec3*/2, GL_FLOAT, /*normalized*/GL_FALSE, 
                                /*stride*/0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3*2);
        _GLCheckError("draw");
        glDisableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
        _GLCheckError("blitbind");
        glBlitFramebuffer(0,0,widthFbo,heightFbo,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        //glBlitFramebuffer(0,0,10,10,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        _GLCheckError("blit");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
