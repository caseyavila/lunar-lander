/*
 * Program 3 base code - includes modifications to shape and initGeom in preparation to load
 * multi shape objects 
 * CPE 471 Cal Poly Z. Wood + S. Sueda + I. Dunn
 */

#include <iostream>
#include <chrono>
#include <glad/glad.h>

#include "GLSL.h"
#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

using namespace std;
using namespace glm;

class Application : public EventCallbacks {

public:
    WindowManager * windowManager = nullptr;

    // Our shader program - use this one for Blinn-Phong
    shared_ptr<Program> prog;

    //Our shader program for textures
    shared_ptr<Program> texProg;

    //our geometry
    shared_ptr<Shape> crater;
    shared_ptr<Shape> sphere;
    shared_ptr<Shape> lander;
    shared_ptr<Shape> bunny;

    //the image to use as a texture (ground)
    shared_ptr<Texture> texture0;
    shared_ptr<Texture> night;
    shared_ptr<Texture> mars;

    vec3 up = vec3(0, 1, 0);
    vec3 right = vec3(0, 0, 1);
    vec3 forward = vec3(1, 0, 0);

    vec3 g_l = up * -0.0001f;

    vec3 cam_off = vec3(5, 2.5, 0);

    struct {
        vec3 pos = vec3(5, 0, 0);
        vec3 lvel = vec3(0, 0, 0);
        vec3 lacc = vec3(0, 0, 0);
        float thrust = 0;

        float pitch = 0;
        float pitch_v = 0;
        float pitch_a = 0;

        float roll = 0;
        float roll_v = 0;
        float roll_a = 0;
    } readings;

    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_SPACE) {
            if (action == GLFW_PRESS) {
                readings.thrust = 0.001;
            } else if (action == GLFW_RELEASE) {
                readings.thrust = 0;
            }
        }

        if (key == GLFW_KEY_A) {
            if (action == GLFW_PRESS) {
                readings.roll_a = 0.0001;
            } else if (action == GLFW_RELEASE) {
                readings.roll_a = 0;
            }
        }

        if (key == GLFW_KEY_D) {
            if (action == GLFW_PRESS) {
                readings.roll_a = -0.0001;
            } else if (action == GLFW_RELEASE) {
                readings.roll_a = 0;
            }
        }

        if (key == GLFW_KEY_W) {
            if (action == GLFW_PRESS) {
                readings.pitch_a = 0.0001;
            } else if (action == GLFW_RELEASE) {
                readings.pitch_a = 0;
            }
        }

        if (key == GLFW_KEY_S) {
            if (action == GLFW_PRESS) {
                readings.pitch_a = -0.0001;
            } else if (action == GLFW_RELEASE) {
                readings.pitch_a = 0;
            }
        }
    }

    void mouseCallback(GLFWwindow *window, int button, int action, int mods) {
    }

    void cursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
    }

    void resizeCallback(GLFWwindow *window, int width, int height) {
        glViewport(0, 0, width, height);
    }

    void update_lander() {

        readings.roll += readings.roll_v;
        readings.roll_v += readings.roll_a;

        readings.pitch += readings.pitch_v;
        readings.pitch_v += readings.pitch_a;

        readings.pos += readings.lvel;
        readings.lvel += readings.lacc;
        readings.lvel += g_l;

        vec3 heading = rotate(up, readings.roll, forward);
        heading = rotate(heading, readings.pitch, right);

        readings.lacc = readings.thrust * heading;
    }

    void init(const std::string& resourceDirectory) {
        GLSL::checkVersion();

        // Set background color.
        glClearColor(0, 0, 0.1f, 1.0f);
        // Enable z-buffer test.
        glEnable(GL_DEPTH_TEST);

        // Initialize the GLSL program that we will use for local shading
        prog = make_shared<Program>();
        prog->setVerbose(true);
        prog->setShaderNames(resourceDirectory + "/simple_vert.glsl", resourceDirectory + "/simple_frag.glsl");
        prog->init();
        prog->addUniform("P");
        prog->addUniform("V");
        prog->addUniform("M");
        prog->addUniform("MatAmb");
        prog->addUniform("MatSpec");
        prog->addUniform("MatShine");
        prog->addUniform("lightPos");
        prog->addAttribute("vertPos");
        prog->addAttribute("vertNor");

        // Initialize the GLSL program that we will use for texture mapping
        texProg = make_shared<Program>();
        texProg->setVerbose(true);
        texProg->setShaderNames(resourceDirectory + "/tex_vert.glsl", resourceDirectory + "/tex_frag0.glsl");
        texProg->init();
        texProg->addUniform("P");
        texProg->addUniform("V");
        texProg->addUniform("M");
        texProg->addUniform("flip");
        texProg->addUniform("Texture0");
        texProg->addUniform("lightPos");
        texProg->addAttribute("vertPos");
        texProg->addAttribute("vertNor");
        texProg->addAttribute("vertTex");

        //read in a load the texture
        texture0 = make_shared<Texture>();
        texture0->setFilename(resourceDirectory + "/moon.jpg");
        texture0->init();
        texture0->setUnit(0);
        texture0->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        //read in a load the texture
        night = make_shared<Texture>();
        night->setFilename(resourceDirectory + "/sphere-night.png");
        night->init();
        night->setUnit(0);
        night->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        //read in a load the texture
        mars = make_shared<Texture>();
        mars->setFilename(resourceDirectory + "/RS3_Mars.png");
        mars->init();
        mars->setUnit(0);
        mars->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    void initGeom(const std::string& resourceDirectory)
    {
        //EXAMPLE set up to read one shape from one obj file - convert to read several
        // Initialize mesh
        // Load geometry
        // Some obj files contain material information.We'll ignore them for this assignment.
        vector<tinyobj::shape_t> TOshapes;
        vector<tinyobj::material_t> objMaterials;
        string errStr;
        //load in the mesh and make the shape(s)
        bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/sphereWTex.obj").c_str());
        if (!rc) {
            cerr << errStr << endl;
        } else {
            sphere = make_shared<Shape>();
            sphere->createShape(TOshapes[0]);
            sphere->measure();
            sphere->init();
        }

        // Initialize lander mesh.
        vector<tinyobj::shape_t> TOshapesB;
        vector<tinyobj::material_t> objMaterialsB;
        //load in the mesh and make the shape(s)
        rc = tinyobj::LoadObj(TOshapesB, objMaterialsB, errStr, (resourceDirectory + "/lander.obj").c_str());
        if (!rc) {
            cerr << errStr << endl;
        } else {
            lander = make_shared<Shape>();
            lander->createShape(TOshapesB[0]);
            lander->measure();
            lander->init();
        }

        // Initialize lander mesh.
        vector<tinyobj::shape_t> TOshapesD;
        vector<tinyobj::material_t> objMaterialsD;
        //load in the mesh and make the shape(s)
        rc = tinyobj::LoadObj(TOshapesD, objMaterialsD, errStr, (resourceDirectory + "/bunnyNoNorm.obj").c_str());
        if (!rc) {
            cerr << errStr << endl;
        } else {
            bunny = make_shared<Shape>();
            bunny->createShape(TOshapesD[0]);
            bunny->measure();
            bunny->init();
        }

        // Initialize lander mesh.
        vector<tinyobj::shape_t> TOshapesC;
        vector<tinyobj::material_t> objMaterialsC;
        //load in the mesh and make the shape(s)
        rc = tinyobj::LoadObj(TOshapesC, objMaterialsC, errStr, (resourceDirectory + "/crater.obj").c_str());
        if (!rc) {
            cerr << errStr << endl;
        } else {
            crater = make_shared<Shape>();
            crater->createShape(TOshapesC[0]);
            crater->measure();
            crater->init();
        }
    }

    /* helper function to set model trasnforms */
    void SetModel(vec3 trans, float rotY, float rotX, float sc, shared_ptr<Program> curS) {
        mat4 Trans = glm::translate(glm::mat4(1.0f), trans);
        mat4 RotX = glm::rotate(glm::mat4(1.0f), rotX, vec3(1, 0, 0));
        mat4 RotY = glm::rotate(glm::mat4(1.0f), rotY, vec3(0, 1, 0));
        mat4 ScaleS = glm::scale(glm::mat4(1.0f), vec3(sc));
        mat4 ctm = Trans*RotX*RotY*ScaleS;
        glUniformMatrix4fv(curS->getUniform("M"), 1, GL_FALSE, value_ptr(ctm));
    }

    void setModel(std::shared_ptr<Program> prog, std::shared_ptr<MatrixStack>M) {
        glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
    }

       /* code to draw waving hierarchical model */
    void render(float frametime) {
        // Get current frame buffer size.
        int width, height;
        glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
        glViewport(0, 0, width, height);

        // Clear framebuffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto Projection = make_shared<MatrixStack>();
        auto Model = make_shared<MatrixStack>();

        mat4 view = lookAt(readings.pos + cam_off, readings.pos, up);

        Projection->pushMatrix();
        Projection->perspective(45.0f, width / (float) height, 0.01f, 1000.0f);

        texProg->bind();
        glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
        glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(view));
        glUniform1i(texProg->getUniform("flip"), 1);

        night->bind(texProg->getUniform("Texture0"));
        Model->pushMatrix();
            Model->translate(readings.pos + cam_off);
            setModel(texProg, Model);
            glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            glDisable(GL_DEPTH_TEST);
            sphere->draw(texProg);
            glEnable(GL_DEPTH_TEST);
        Model->popMatrix();

        texProg->unbind();
        
        prog->bind();
        glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
        glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, value_ptr(view));
        glUniform3f(prog->getUniform("lightPos"), 1000, 500, 0);

        float crater_scale = 1000 / (crater->max.x - crater->min.x);
        Model->pushMatrix();
            Model->translate(vec3(0, -1, 0));
            Model->scale(vec3(crater_scale, crater_scale, crater_scale));
            glUniform3f(prog->getUniform("MatAmb"), 0.03, 0.03, 0.03);
            glUniform3f(prog->getUniform("MatSpec"), 0.45, 0.45, 0.45);
            glUniform1f(prog->getUniform("MatShine"), 100.0);
            glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            crater->draw(prog);
        Model->popMatrix();

        Model->pushMatrix();
            Model->translate(readings.pos);
            Model->rotate(readings.pitch, right);
            Model->rotate(readings.roll, forward);
            glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            lander->draw(prog);
        Model->popMatrix();

        prog->unbind();

        Projection->popMatrix();

        update_lander();
    }
};

int main(int argc, char *argv[])
{
    // Where the resources are loaded from
    string resourceDir = "../resources";

    if (argc >= 2) {
        resourceDir = argv[1];
    }

    Application *application = new Application();

    // Your main will always include a similar set up to establish your window
    // and GL context, etc.

    WindowManager *windowManager = new WindowManager();
    windowManager->init(640, 480);
    windowManager->setEventCallbacks(application);
    application->windowManager = windowManager;

    // This is the code that will likely change program to program as you
    // may need to initialize or set up different data and state

    application->init(resourceDir);
    application->initGeom(resourceDir);

    auto lastTime = chrono::high_resolution_clock::now();

    // Loop until the user closes the window.
    while (!glfwWindowShouldClose(windowManager->getHandle())) {
        auto nextLastTime = chrono::high_resolution_clock::now();

        // get time since last frame
        float deltaTime =
          chrono::duration_cast<chrono::microseconds>(
            chrono::high_resolution_clock::now() - lastTime)
            .count();
        // convert microseconds (weird) to seconds (less weird)
        deltaTime *= 0.000001;

        // reset lastTime so that we can calculate the deltaTime
        // on the next frame
        lastTime = nextLastTime;

        // Render scene.
        application->render(deltaTime);

        // Swap front and back buffers.
        glfwSwapBuffers(windowManager->getHandle());
        // Poll for and process events.
        glfwPollEvents();
    }

    // Quit program.
    windowManager->shutdown();
    return 0;
}
