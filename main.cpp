#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <random>
#include "camera.h"
#include "shader.h"

const unsigned int screenWidth = 1280;
const unsigned int screenHeight = 720;

bool cameraMode = false;
bool tabPressed = false;
Camera camera(glm::vec3(0.0f, 0.0f, 25.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float lastX = screenWidth / 2.0f;
float lastY = screenHeight / 2.0f;
bool firstMouse = true;

const int NUM_ANTS = 500;
const int GRID_SIZE = 128;
float pheromoneToFood[GRID_SIZE][GRID_SIZE] = { 0.0f };
float pheromoneToHome[GRID_SIZE][GRID_SIZE] = { 0.0f };
float PHEROMONE_DECAY = 0.9995f;
float PHEROMONE_DEPOSIT = 5.0f;

struct Ant {
    glm::vec2 pos;
    glm::vec2 vel;
    bool carryingFood;
};

std::vector<Ant> ants;
std::vector<glm::vec3> particlePositions;
std::vector<glm::vec3> particleColors;

glm::vec2 homePos(-10.0f, 0.0f);
glm::vec2 foodPos(10.0f, 10.0f);

float MAX_SPEED = 0.5f;
float SENSE_DISTANCE = 3.0f;
float SENSE_ANGLE = 0.4f;
float DETECTION_RADIUS = 3.0f;
float WANDER_STRENGTH = 0.1f;



void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) { camera.ProcessMouseScroll((float)yoffset); }
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = -(ypos - lastY);
    lastX = xpos; lastY = ypos;
    if (cameraMode) camera.ProcessMouseMovement(xoffset, yoffset);
}
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    //aas
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabPressed) {

        cameraMode = !cameraMode;
        glfwSetInputMode(window, GLFW_CURSOR, cameraMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        tabPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) tabPressed = false;
}

void initAnts() {
    MAX_SPEED = 0.4f;
    SENSE_DISTANCE = 2.5f;
    SENSE_ANGLE = 0.5f;
    DETECTION_RADIUS = 2.5f;
    WANDER_STRENGTH = 0.08f;
    PHEROMONE_DECAY = 0.995f;
    PHEROMONE_DEPOSIT = 10.0f;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> distVel(-0.02f, 0.02f);
    ants.clear(); particlePositions.clear(); particleColors.clear();
    for (int i = 0; i < NUM_ANTS; i++) {
        ants.push_back({ homePos,glm::vec2(distVel(rng),distVel(rng)),false });
        particlePositions.push_back(glm::vec3(homePos, 0.0f));
        particleColors.push_back(glm::vec3(1.0f, 1.0f, 0.0f));
    }
}

float samplePheromone(glm::vec2 direction, const Ant& ant, const std::vector<std::vector<float>>& pheromoneToFood, const std::vector<std::vector<float>>& pheromoneToHome, int GRID_SIZE, float SENSE_DISTANCE)
{
    glm::vec2 samplePos = ant.pos + direction * SENSE_DISTANCE;
    int gridX = (int)((samplePos.x + 20.0f) / 40.0f * GRID_SIZE);
    int gridY = (int)((samplePos.y + 20.0f) / 40.0f * GRID_SIZE);

    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE)
        return 0.0f;

    if (ant.carryingFood)
        return pheromoneToHome[gridX][gridY];
    else
        return pheromoneToFood[gridX][gridY];
}

void updateAnts(float dt)
{
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> randAngle(-0.5f, 0.5f);
    static std::uniform_real_distribution<float> randTurn(-1.0f, 1.0f);

    for (int i = 0; i < ants.size(); i++)
    {
        Ant& ant = ants[i];
        float distToFood = glm::length(foodPos - ant.pos);
        float distToHome = glm::length(homePos - ant.pos);

        if (!ant.carryingFood && distToFood < DETECTION_RADIUS)
        {
            ant.carryingFood = true;
            ant.vel = -ant.vel;
        }
        else if (ant.carryingFood && distToHome < DETECTION_RADIUS)
        {
            ant.carryingFood = false;
            ant.vel = -ant.vel;
        }

        float currentAngle = atan2(ant.vel.y, ant.vel.x);
        glm::vec2 forward(cos(currentAngle), sin(currentAngle));
        glm::vec2 left(cos(currentAngle + SENSE_ANGLE), sin(currentAngle + SENSE_ANGLE));
        glm::vec2 right(cos(currentAngle - SENSE_ANGLE), sin(currentAngle - SENSE_ANGLE));

        auto samplePheromone = [&](glm::vec2 dir) -> float {
            glm::vec2 samplePos = ant.pos + dir * SENSE_DISTANCE;
            int gridX = (int)((samplePos.x + 20.0f) / 40.0f * GRID_SIZE);
            int gridY = (int)((samplePos.y + 20.0f) / 40.0f * GRID_SIZE);

            if (gridX >= 0 && gridX < GRID_SIZE && gridY >= 0 && gridY < GRID_SIZE) {
                return ant.carryingFood ? pheromoneToHome[gridX][gridY] : pheromoneToFood[gridX][gridY];
            }
            return 0.0f;
            };

        float leftPheromone = samplePheromone(left);
        float forwardPheromone = samplePheromone(forward);
        float rightPheromone = samplePheromone(right);

        float turnAngle = 0.0f;
        if (forwardPheromone > leftPheromone && forwardPheromone > rightPheromone) {
            turnAngle = randAngle(rng) * WANDER_STRENGTH;
        }
        else if (leftPheromone > rightPheromone) {
            turnAngle = SENSE_ANGLE + randAngle(rng) * WANDER_STRENGTH;
        }
        else if (rightPheromone > leftPheromone) {
            turnAngle = -SENSE_ANGLE + randAngle(rng) * WANDER_STRENGTH;
        }
        else {
            turnAngle = randTurn(rng) * 0.1f;
        }

        currentAngle += turnAngle;
        ant.vel = glm::vec2(cos(currentAngle), sin(currentAngle));

        int gridX = (int)((ant.pos.x + 20.0f) / 40.0f * GRID_SIZE);
        int gridY = (int)((ant.pos.y + 20.0f) / 40.0f * GRID_SIZE);

        if (gridX >= 0 && gridX < GRID_SIZE && gridY >= 0 && gridY < GRID_SIZE) {
            if (ant.carryingFood) {
                pheromoneToFood[gridX][gridY] += PHEROMONE_DEPOSIT * dt;
            }
            else {
                pheromoneToHome[gridX][gridY] += PHEROMONE_DEPOSIT * dt;
            }
        }
        ant.pos += ant.vel * dt * MAX_SPEED;

        if (ant.pos.x < -20.0f)
        {
            ant.pos.x = -20.0f;
            ant.vel.x *= -1.0f;
        }
        if (ant.pos.x > 20.0f)
        {
            ant.pos.x = 20.0f;
            ant.vel.x *= -1.0f;
        }
        if (ant.pos.y < -20.0f)
        {
            ant.pos.y = -20.0f;
            ant.vel.y *= -1.0f;
        }
        if (ant.pos.y > 20.0f)
        {
            ant.pos.y = 20.0f;
            ant.vel.y *= -1.0f;
        }

        particlePositions[i] = glm::vec3(ant.pos, 0.0f);
        particleColors[i] = ant.carryingFood ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 1.0f, 0.0f);
    }

    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            pheromoneToFood[x][y] *= PHEROMONE_DECAY;
            pheromoneToHome[x][y] *= PHEROMONE_DECAY;
        }
    }
    /*float tempFood[GRID_SIZE][GRID_SIZE];
    float tempHome[GRID_SIZE][GRID_SIZE];

    for (int x = 1; x < GRID_SIZE - 1; x++) {
        for (int y = 1; y < GRID_SIZE - 1; y++) {
            tempFood[x][y] = (
                pheromoneToFood[x][y] +
                pheromoneToFood[x - 1][y] + pheromoneToFood[x + 1][y] +
                pheromoneToFood[x][y - 1] + pheromoneToFood[x][y + 1]
                ) / 5.0f;
            tempHome[x][y] = (
                pheromoneToHome[x][y] +
                pheromoneToHome[x - 1][y] + pheromoneToHome[x + 1][y] +
                pheromoneToHome[x][y - 1] + pheromoneToHome[x][y + 1]
                ) / 5.0f;
        }
    }

    memcpy(pheromoneToFood, tempFood, sizeof(tempFood));
    memcpy(pheromoneToHome, tempHome, sizeof(tempHome));*/

}

void drawCircle(const glm::vec2& center, float radius, const glm::vec3& color, const glm::mat4& projection, const glm::mat4& view)
{
    const int segments = 64;
    std::vector<glm::vec3> vertices;
    vertices.reserve(segments + 2);
    for (int i = 0; i <= segments; i++)
    {
        float angle = i * 2.0f * 3.1415926f / segments;
        float x = center.x + cos(angle) * radius;
        float y = center.y + sin(angle) * radius;
        vertices.push_back(glm::vec3(x, y, 0.0f));
    }

    Shader lineShader("line.vs", "line.fs");
    lineShader.use();
    lineShader.setMat4("projection", projection);
    lineShader.setMat4("view", view);
    lineShader.setVec3("color", color);

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINE_LOOP, 0, vertices.size());

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Ant Simulation", NULL, NULL);

    if (!window) { 
        std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; 
    
    }

    glfwMakeContextCurrent(window);


    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { 
        std::cerr << "Failed to initialize GLAD\n"; return -1; 
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");


    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);


    Shader pointShader("point.vs", "point.fs");


    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    float pointVertex[] = { 0.0f,0.0f,0.0f };

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pointVertex), pointVertex, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);


    unsigned int instanceVBO, colorVBO;
    glGenBuffers(1, &instanceVBO);
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    initAnts();
    float simDeltaTime = 0.01f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Simulation Controls", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::SliderFloat("Simulation Step", &simDeltaTime, 0.001f, 0.05f, "%.4f");
        ImGui::SeparatorText("Ant Behavior");
        ImGui::SliderFloat("Max Speed", &MAX_SPEED, 0.1f, 20.0f, "%.2f");
        ImGui::SliderFloat("Sense Distance", &SENSE_DISTANCE, 0.1f, 10.0f, "%.2f");
        ImGui::SliderFloat("Sense Angle", &SENSE_ANGLE, 0.1f, 1.5f, "%.2f");
        ImGui::SliderFloat("Detection Radius", &DETECTION_RADIUS, 0.1f, 10.0f, "%.2f");
        ImGui::SliderFloat("Wander Strength", &WANDER_STRENGTH, 0.01f, 1.0f, "%.3f");
        ImGui::SeparatorText("Pheromone Behavior");
        ImGui::SliderFloat("Pheromone Decay", &PHEROMONE_DECAY, 0.90f, 0.9999f, "%.5f");
        ImGui::SliderFloat("Pheromone Deposit", &PHEROMONE_DEPOSIT, 0.1f, 100.0f, "%.2f");
        ImGui::End();

        updateAnts(simDeltaTime);

        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, particlePositions.size() * sizeof(glm::vec3), particlePositions.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
        glBufferData(GL_ARRAY_BUFFER, particleColors.size() * sizeof(glm::vec3), particleColors.data(), GL_DYNAMIC_DRAW);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();


        pointShader.use();
        pointShader.setMat4("projection", projection);
        pointShader.setMat4("view", view);
        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_POINTS, 0, 1, particlePositions.size());

        drawCircle(homePos, DETECTION_RADIUS, glm::vec3(0.0f, 0.5f, 1.0f), projection, view);
        drawCircle(foodPos, DETECTION_RADIUS, glm::vec3(1.0f, 0.2f, 0.2f), projection, view);



        ImGui::Begin("Poi");
        ImGui::Text("Home: (%.1f, %.1f)", homePos.x, homePos.y);
        ImGui::Text("Food: (%.1f, %.1f)", foodPos.x, foodPos.y);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
