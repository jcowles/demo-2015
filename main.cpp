// Created by Jeremy Cowles, 2015

#include "audio.h"

#include "lodepng/lodepng.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>


#include <cstdlib>  // for rand
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

struct QuadProgram {
    GLuint program;
    GLint iRandomLoc;
    GLint iResolutionLoc;
    GLint iGlobalTimeLoc;
    GLint iMouseLoc;
    GLint iChannel0Loc;
    GLint iChannel1Loc;
};
QuadProgram _shaderToy;
QuadProgram _film;
GLuint _texBuffers[2];

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

static std::string 
_ReadFile(std::string path)
{
    std::stringstream ss;
    std::string line;
    std::ifstream glslFile(path);
    if (glslFile.is_open()) {
        while (getline(glslFile,line)) {
            ss << line << "\n";
        }
        glslFile.close();
    }
    return ss.str();
}

static void 
_LinkQuadProgram(std::string vs, std::string fs, QuadProgram* qp)
{
    vs = _ReadFile(vs); 
    fs = _ReadFile(fs); 

    qp->program = _GLLinkProgram(vs.c_str(), fs.c_str());
    qp->iRandomLoc = glGetUniformLocation(qp->program, "iRandom");
    qp->iResolutionLoc = glGetUniformLocation(qp->program, "iResolution");
    qp->iGlobalTimeLoc = glGetUniformLocation(qp->program, "iGlobalTime");
    qp->iChannel0Loc = glGetUniformLocation(qp->program, "iChannel0");
    qp->iChannel1Loc = glGetUniformLocation(qp->program, "iChannel1");
    //qp->iMouseLoc = glGetUniformLocation(qp->program, "iMouse");
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

    _LinkQuadProgram("quad.vs.glsl", "dunes.fs.glsl", &_shaderToy);
    _LinkQuadProgram("aspect.vs.glsl", "film.fs.glsl", &_film);
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

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texBuffers[0], 0);
    /*
    glBindRenderbuffer(GL_RENDERBUFFER, _rboColor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, _rboColor);
    */

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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED,
                 GL_UNSIGNED_BYTE, &image[0]);
}

static void
_InitFrameTextures(GLsizei width, GLsizei height)
{
    glActiveTexture(GL_TEXTURE1);
    glGenTextures(2, &_texBuffers[0]);
    float* mem = new float(width*height*sizeof(float)*4);
    glBindTexture(GL_TEXTURE_2D, _texBuffers[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_FLOAT, mem);

    glBindTexture(GL_TEXTURE_2D, _texBuffers[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_FLOAT, mem);

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

    GLFWwindow* window;
    
    // Must match FBO samples to use glBlitFramebuffer
    //glfwWindowHint(GLFW_SAMPLES, 1);

    // Setup native resolution full-screen; use FBO to downsample the 
    // render resolution. This looks better and doesn't cause the window 
    // manager to freak out due to resolution changes.
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int width=mode->width, height=mode->height;
    //int width=960, height=400;
    //int width=1024, height=768;
    //int width=1920, height=800;

    // PI is a nice aspect ratio
    float aspect = 3.14159265359;
    float scale = 0.5;
    int widthFbo=width*scale, 
        heightFbo=((width*scale)*(1/aspect));

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
    _InitFrameTextures(width, height);
    _InitFBO(widthFbo, heightFbo);
    _InitRandomTexture();                 // binds TEXTURE0

    glfwSetKeyCallback(window, _KeyCallback);
    glfwSwapInterval(0);

    //StartAudio();

    srand(0);
    
    double frameTime = 0.0, lastTime = 0.0;
    size_t frameCnt = 0;
    glfwGetFramebufferSize(window, &width, &height);
    //std::cout << width << " x " << height << "\n";

    while (!glfwWindowShouldClose(window))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);

        glViewport(0, 0, widthFbo, heightFbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(_shaderToy.program);
        glUniform1f(_shaderToy.iGlobalTimeLoc, glfwGetTime());
        glUniform1i(_shaderToy.iChannel0Loc, 0);
        glUniform1i(_shaderToy.iChannel1Loc, 0);
        glUniform3f(_shaderToy.iResolutionLoc, widthFbo, heightFbo, 1.0);
        glUniform1f(_shaderToy.iRandomLoc, rand()/float(RAND_MAX));
        glBindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(/*attrib*/0, /*vec3*/2, GL_FLOAT, /*normalized*/GL_FALSE, 
                                /*stride*/0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3*2);
        _GLCheckError("draw");
        //glDisableVertexAttribArray(0);
        //glBindBuffer(GL_ARRAY_BUFFER, 0);
        //glUseProgram(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Apply film effect and blit to screen
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(_film.program);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _texBuffers[0]);
        glUniform1f(_film.iGlobalTimeLoc, glfwGetTime());
        glUniform1i(_film.iChannel0Loc, 0);
        glUniform1i(_film.iChannel1Loc, 1);
        glUniform3f(_film.iResolutionLoc, widthFbo, heightFbo, 1.0);
        glUniform1f(_film.iRandomLoc, rand()/float(RAND_MAX));
        glDrawArrays(GL_TRIANGLES, 0, 3*2);
        _GLCheckError("draw2");

        #if 0
        // Blit to screen with no effect.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
        _GLCheckError("blitbind");
        glBlitFramebuffer(0,0,widthFbo,heightFbo,0,height*.25,width,height*.75,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        _GLCheckError("blit");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        #endif

        glfwSwapBuffers(window);
        glfwPollEvents();
        frameCnt++;
        frameTime += glfwGetTime() - lastTime;
        lastTime = glfwGetTime();
        if (frameCnt % 30 == 0) {
            std::cout << "FPS: " << (frameCnt / frameTime) << "\n";
            frameTime = 0;
            frameCnt = 0;
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
