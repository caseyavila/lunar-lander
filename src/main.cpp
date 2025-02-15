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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

using namespace std;
using namespace glm;

bool landed = false;

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
    shared_ptr<Shape> plume;

    //the image to use as a texture (ground)
    shared_ptr<Texture> night;
    shared_ptr<Texture> lander_tex;
    shared_ptr<Texture> nibox;

    vec3 up = vec3(0, 1, 0);
    vec3 right = vec3(0, 0, 1);
    vec3 forward = vec3(1, 0, 0);

    vec3 g_l = up * -0.0001f;

    vec3 cam_off = vec3(5, 2.5, 0);

    float crater_scale;
    vector<vector<float>> map;
    vector<vector<float>> norm_x;
    vector<vector<float>> norm_y;
    vector<vector<float>> norm_z;

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

    float altitude() {
        float x = readings.pos.x - (crater->min.x * crater_scale);
        float z = readings.pos.z - (crater->min.z * crater_scale);
        return readings.pos.y - map[x][z];
    }

    float aligned() {
        float x = readings.pos.x - (crater->min.x * crater_scale);
        float z = readings.pos.z - (crater->min.z * crater_scale);

        vec3 moon = vec3(norm_x[x][z], norm_y[x][z], norm_z[x][z]);
        vec3 lander = rotate(up, readings.roll, forward);
        lander = rotate(lander, readings.pitch, right);

        return dot(normalize(moon), normalize(lander));
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

        if (altitude() < 0) {
            cout << aligned() << endl;
            if (glm::length(readings.lvel) > 0.02) {
                cout << "CRASH: Hit the ground too hard!" << endl;
            } else if (readings.pitch_v + readings.roll_v > 0.02) {
                cout << "CRASH: Hit the ground spinning!" << endl;
            } else if (aligned() < 0.8) {
                cout << "CRASH: Didn't land straight!" << endl;
            } else {
                cout << "The eagle has landed!" << endl;
            }
            landed = true;
        }
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
        texProg->addUniform("skytex");
        texProg->addUniform("lightPos");
        texProg->addAttribute("vertPos");
        texProg->addAttribute("vertNor");
        texProg->addAttribute("vertTex");

        //read in a load the texture
        night = make_shared<Texture>();
        night->setFilename(resourceDirectory + "/sphere-night.png");
        night->init();
        night->setUnit(1);
        night->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        //read in a load the texture
        lander_tex = make_shared<Texture>();
        lander_tex->setFilename(resourceDirectory + "/lander.png");
        lander_tex->init();
        lander_tex->setUnit(2);
        lander_tex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        //read in a load the texture
        nibox = make_shared<Texture>();
        nibox->setFilename(resourceDirectory + "/nibox.jpg");
        nibox->init();
        nibox->setUnit(3);
        nibox->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    void interpolateZeroes(std::vector<std::vector<float>>& vec) {
        int rows = vec.size();
        if (rows == 0) return; // Empty vector
        int cols = vec[0].size();

        // Interpolate rows
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                if (vec[i][j] == 0) {
                    // Find nearest non-zero values in the same row
                    float leftVal = 0, rightVal = 0;
                    for (int k = j - 1; k >= 0; --k) {
                        if (vec[i][k] != 0) {
                            leftVal = vec[i][k];
                            break;
                        }
                    }
                    for (int k = j + 1; k < cols; ++k) {
                        if (vec[i][k] != 0) {
                            rightVal = vec[i][k];
                            break;
                        }
                    }
                    // Linear interpolation
                    vec[i][j] = (leftVal + rightVal) / 2;
                }
            }
        }

        // Interpolate columns
        for (int j = 0; j < cols; ++j) {
            for (int i = 0; i < rows; ++i) {
                if (vec[i][j] == 0) {
                    // Find nearest non-zero values in the same column
                    float upVal = 0, downVal = 0;
                    for (int k = i - 1; k >= 0; --k) {
                        if (vec[k][j] != 0) {
                            upVal = vec[k][j];
                            break;
                        }
                    }
                    for (int k = i + 1; k < rows; ++k) {
                        if (vec[k][j] != 0) {
                            downVal = vec[k][j];
                            break;
                        }
                    }
                    // Linear interpolation
                    vec[i][j] = (upVal + downVal) / 2;
                }
            }
        }
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
        rc = tinyobj::LoadObj(TOshapesD, objMaterialsD, errStr, (resourceDirectory + "/plume.obj").c_str());
        if (!rc) {
            cerr << errStr << endl;
        } else {
            plume = make_shared<Shape>();
            plume->createShape(TOshapesD[0]);
            plume->measure();
            plume->init();
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

        crater_scale = 1000 / (crater->max.x - crater->min.x);
        int x_width = (crater->max.x - crater->min.x) * crater_scale;
        int z_width = (crater->max.z - crater->min.z) * crater_scale;
        for (int i = 0; i < x_width + 1; i++) {
            std::vector<float> row;
            for (int i = 0; i < z_width + 1; i++) {
                row.push_back(0.f);
            }
            map.push_back(row);
            norm_x.push_back(row);
            norm_y.push_back(row);
            norm_z.push_back(row);
        }

        for (size_t i = 0; i < crater->posBuf.size(); i += 3) {
            float px = crater->posBuf[i] - crater->min.x;
            float pz = crater->posBuf[i + 2] - crater->min.z;
            map[px * crater_scale][pz * crater_scale]
               = crater->posBuf[i + 1] * crater_scale;
        }

        interpolateZeroes(map);

        for (size_t i = 0; i < crater->posBuf.size(); i += 3) {
            float px = crater->posBuf[i] - crater->min.x;
            float pz = crater->posBuf[i + 2] - crater->min.z;
            norm_x[px * crater_scale][pz * crater_scale] = crater->norBuf[i];
            norm_y[px * crater_scale][pz * crater_scale] = crater->norBuf[i + 1];
            norm_z[px * crater_scale][pz * crater_scale] = crater->norBuf[i + 2];
        }

        interpolateZeroes(norm_x);
        interpolateZeroes(norm_y);
        interpolateZeroes(norm_z);

        /*
        for (size_t i = 0; i < norm_x.size(); i++) {
            for (size_t j = 0; j < norm_x[i].size(); j++) {
                cout << norm_x[i][j] << ", ";
            }
            cout << endl;
        }
        */
        
    }

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
        glUniform3f(texProg->getUniform("lightPos"), 1000, 200, 0);
        nibox->bind(texProg->getUniform("skytex"));

        glUniform1i(texProg->getUniform("flip"), 1);
        night->bind(texProg->getUniform("Texture0"));
        Model->pushMatrix();
            Model->translate(readings.pos + cam_off);
            glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            glDisable(GL_DEPTH_TEST);
            sphere->draw(texProg);
            glEnable(GL_DEPTH_TEST);
        Model->popMatrix();

        glUniform1i(texProg->getUniform("flip"), 0);
        lander_tex->bind(texProg->getUniform("Texture0"));
        Model->pushMatrix();
            Model->translate(readings.pos);
            Model->rotate(readings.pitch, right);
            Model->rotate(readings.roll, forward);
            glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            lander->draw(texProg);

            texProg->unbind();

            prog->bind();

            glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
            glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, value_ptr(view));
            glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            glUniform3f(prog->getUniform("MatAmb"), 0.93, 0.03, 0.03);
            glUniform3f(prog->getUniform("MatSpec"), 0.45, 0.05, 0.05);
            glUniform1f(prog->getUniform("MatShine"), 100.0);
            glUniform3f(prog->getUniform("lightPos"), 1000, 200, 0);
            if (readings.thrust != 0) {
                Model->pushMatrix();
                    Model->scale(up * 0.5f);
                    plume->draw(texProg);
                Model->popMatrix();
            }
        Model->popMatrix();


        Model->pushMatrix();
            Model->translate(vec3(0, -1, 0));
            Model->scale(vec3(crater_scale, crater_scale, crater_scale));
            glUniform3f(prog->getUniform("MatAmb"), 0.03, 0.03, 0.03);
            glUniform3f(prog->getUniform("MatSpec"), 0.45, 0.45, 0.45);
            glUniform1f(prog->getUniform("MatShine"), 100.0);
            glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
            crater->draw(prog);
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
    while (!glfwWindowShouldClose(windowManager->getHandle()) && !landed) {
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
