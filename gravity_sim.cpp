#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>

const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out float lightIntensity;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vec3 worldPos = (model * vec4(aPos, 1.0)).xyz;
    vec3 normal = normalize(aPos);
    vec3 dirToCenter = normalize(-worldPos);
    lightIntensity = max(dot(normal, dirToCenter), 0.15);})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in float lightIntensity;
out vec4 FragColor;
uniform vec4 objectColor;
uniform bool isGrid; // Add this uniform
uniform bool GLOW;
void main() {
    if (isGrid) {
        // If it's the grid, use the original color without lighting
        FragColor = objectColor;
    } else if(GLOW){
        FragColor = vec4(objectColor.rgb * 100000, objectColor.a);
    }else {
        // If it's an object, apply the lighting effect
        float fade = smoothstep(0.0, 10.0, lightIntensity*10);
        FragColor = vec4(objectColor.rgb * fade, objectColor.a);
    }})glsl";

bool running = true;
bool pause = true;
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  1.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float lastX = 400.0, lastY = 300.0;
float yaw = -90;
float pitch =0.0;
float deltaTime = 0.0;
float lastFrame = 0.0;

const double G = 6.6743e-11;
const float c = 299792458.0;
float initMass = float(pow(10, 22));
float sizeRatio = 30000.0f;

GLFWwindow* StartGLU();
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount);
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void mouse_callback(GLFWwindow* window, double xpos, double ypos);
glm::vec3 sphericalToCartesian(float r, float theta, float phi);
void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount);


class Object {
    public:
        GLuint VAO, VBO;
        glm::vec3 position = glm::vec3(400, 300, 0);
        glm::vec3 velocity = glm::vec3(0, 0, 0);
        size_t vertexCount;
        glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

        bool Initalizing = false;
        bool Launched = false;
        bool target = false;

        float mass;
        float density; 
        float radius;

        glm::vec3 LastPos = position;
        bool glow;

        Object(glm::vec3 initPosition, glm::vec3 initVelocity, float mass, float density = 3344, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), bool Glow = false) {   
            this->position = initPosition;
            this->velocity = initVelocity;
            this->mass = mass;
            this->density = density;
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / sizeRatio;
            this->color = color;
            this->glow = Glow;
            

            std::vector<float> vertices = Draw();
            vertexCount = vertices.size();

            CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
        }

        std::vector<float> Draw() {
            std::vector<float> vertices;
            int stacks = 10;
            int sectors = 10;

            for(float i = 0.0f; i <= stacks; ++i){
                float theta1 = (i / stacks) * glm::pi<float>();
                float theta2 = (i+1) / stacks * glm::pi<float>();
                for (float j = 0.0f; j < sectors; ++j){
                    float phi1 = j / sectors * 2 * glm::pi<float>();
                    float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
                    glm::vec3 v1 = sphericalToCartesian(this->radius, theta1, phi1);
                    glm::vec3 v2 = sphericalToCartesian(this->radius, theta1, phi2);
                    glm::vec3 v3 = sphericalToCartesian(this->radius, theta2, phi1);
                    glm::vec3 v4 = sphericalToCartesian(this->radius, theta2, phi2);

                    vertices.insert(vertices.end(), {v1.x, v1.y, v1.z}); 
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z}); 
                    

                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                }   
            }
            return vertices;
        }
        
        void UpdatePos(){
            this->position[0] += this->velocity[0] / 94;
            this->position[1] += this->velocity[1] / 94;
            this->position[2] += this->velocity[2] / 94;
            this->radius = pow(((3 * this->mass/this->density)/(4 * 3.14159265359)), (1.0f/3.0f)) / sizeRatio;
        }
        void UpdateVertices() {

            std::vector<float> vertices = Draw();
            
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        }
        glm::vec3 GetPos() const {
            return this->position;
        }
        void accelerate(float x, float y, float z){
            this->velocity[0] += x / 96;
            this->velocity[1] += y / 96;
            this->velocity[2] += z / 96;
        }
        float CheckCollision(const Object& other) {
            float dx = other.position[0] - this->position[0];
            float dy = other.position[1] - this->position[1];
            float dz = other.position[2] - this->position[2];
            float distance = std::pow(dx*dx + dy*dy + dz*dz, (1.0f/2.0f));
            if (other.radius + this->radius > distance){
                return -0.2f;
            }
            return 1.0f;
        }
};









