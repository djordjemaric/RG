#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
unsigned int loadTexture(char const *path);
unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool noc = false;

//HDR
bool hdr = false;
float exposure = 1.0;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(-1.0f, 1.0f, 2.0f);
    PointLight pointLight;
    DirLight dirLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);
    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif


    // glfw window creation
    // --------------------
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Piratsko Ostrvo", monitor, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    //Face culling, ne renderujemo stranice koje nisu vidljive
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Kompajliranje sejdera
    // -------------------------
    Shader modelShader("resources/shaders/modelShader.vs", "resources/shaders/modelShader.fs");
    Shader planeShader("resources/shaders/planeShader.vs", "resources/shaders/planeShader.fs");
    Shader skyboxShader("resources/shaders/skyboxShader.vs", "resources/shaders/skyboxShader.fs");
    Shader blendingShader("resources/shaders/blendingShader.vs", "resources/shaders/blendingShader.fs");
    Shader hdrShader("resources/shaders/hdrShader.vs", "resources/shaders/hdrShader.fs");
    Shader blurShader("resources/shaders/7.blur.vs", "resources/shaders/7.blur.fs");

    // Ucitavanje i podesavanje modela
    // ------------------
    Model ostrvo("resources/objects/island/island_no_plants.obj");
    ostrvo.SetShaderTextureNamePrefix("material.");

    Model vatra("resources/objects/fire2/Campfire.obj");
    vatra.SetShaderTextureNamePrefix("material.");

    Model brod("resources/objects/ship/SM_Ship12.obj");
    brod.SetShaderTextureNamePrefix("material.");

    //Tekstura je obrnuta vec, pa iskljucujemo ovo
    stbi_set_flip_vertically_on_load(false);
    Model dzek("resources/objects/JackSparrow/Jack Sparrow.obj");
    dzek.SetShaderTextureNamePrefix("material.");
    stbi_set_flip_vertically_on_load(true);


    //Ravan koja predstavlja vodu
    float planeVertices[] = {
        //      vertex           texture        normal
        50.0f, -1.0f,  50.0f,  2.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        -50.0f, -1.0f, -50.0f,  0.0f, 2.0f, 0.0f, 1.0f, 0.0f,
        -50.0f, -1.0f,  50.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f,


        50.0f, -1.0f,  50.0f,  2.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        50.0f, -1.0f, -50.0f,  2.0f, 2.0f, 0.0f, 1.0f, 0.0f,
        -50.0f, -1.0f, -50.0f,  0.0f, 2.0f, 0.0f, 1.0f, 0.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));

    glBindVertexArray(0);

    unsigned int waterTexture = loadTexture("resources/textures/water_texture.jpg");
    planeShader.use();
    planeShader.setInt("texture1", 0);


    // parametri za skybox
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    vector<std::string> faces_night {
        FileSystem::getPath("resources/textures/night_skybox/px.png"),
        FileSystem::getPath("resources/textures/night_skybox/nx.png"),
        FileSystem::getPath("resources/textures/night_skybox/py.png"),
        FileSystem::getPath("resources/textures/night_skybox/ny.png"),
        FileSystem::getPath("resources/textures/night_skybox/pz.png"),
        FileSystem::getPath("resources/textures/night_skybox/nz.png")
    };

    vector<std::string> faces_day {
        FileSystem::getPath("resources/textures/skybox/px.png"),
        FileSystem::getPath("resources/textures/skybox/nx.png"),
        FileSystem::getPath("resources/textures/skybox/py.png"),
        FileSystem::getPath("resources/textures/skybox/ny.png"),
        FileSystem::getPath("resources/textures/skybox/pz.png"),
        FileSystem::getPath("resources/textures/skybox/nz.png")
    };

    stbi_set_flip_vertically_on_load(false);
    unsigned int cubemapTextureDay = loadCubemap(faces_day);
    unsigned  int cubemapTextureNight = loadCubemap(faces_night);
    stbi_set_flip_vertically_on_load(true);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);


    //biljka
    float transparentVertices2[] = {
            // positions         // texture Coords (swapped y coordinates because texture is flipped upside down)
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    float transparentVertices[] = {
            // positions          texture        normal
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f,   0.0f,  1.0f,  0.0f
    };


    glm::vec3 biljkePozicije[] = {
            glm::vec3(0, 0.18f, 1.35f),
            glm::vec3(-0.5f, 0.18f, 1.40f),
            glm::vec3(0.5f, 0.18f, 1.40f),
            glm::vec3(-1.0f, 0.18f, 1.35f)
    };

    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);
    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices), transparentVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glBindVertexArray(0);
    
    

    stbi_set_flip_vertically_on_load(false);
    unsigned int transparentTexture = loadTexture("resources/textures/biljka.png");
    stbi_set_flip_vertically_on_load(true);


    //Quad
    float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, //4
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, //2
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, //1
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f, //3
    };

    // setup plane VAO
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    /*
    // configure floating point framebuffer
    // ------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    // create floating point color buffer
    unsigned int colorBuffer;
    glGenTextures(1, &colorBuffer);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // create depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    // attach buffers
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    */

    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    //Svetla

    //Podesavamo svetlo koje izvire iz vatre
    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(-1.5f, 0.0f, -1.0f);
    pointLight.ambient = glm::vec3(0.95, 0.5, 0.0);
    pointLight.diffuse = glm::vec3(20, 13, 2);
    pointLight.specular = glm::vec3(0.5f);

    pointLight.constant = 1.0f;
    pointLight.linear = 0.5f;
    pointLight.quadratic = 1.1f;

    DirLight& dirLight = programState->dirLight;


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {

        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // postavljanje svetla za odgovarajuci deo dana
        if (noc) {
            //mesec
            dirLight.direction = glm::vec3(-2.0f, -1.0f, -0.3f);
            dirLight.ambient = glm::vec3(0.02f, 0.02f, 0.02f);
            dirLight.diffuse = glm::vec3(0.1, 0.1, 0.1);
            dirLight.specular = glm::vec3(0.5f, 0.5f, 0.5f);
        } else {
            //sunce
            dirLight.direction = glm::vec3(-2.0f, -1.0f, -0.3f);
            dirLight.ambient = glm::vec3(0.35f, 0.35f, 0.35f);
            dirLight.diffuse = glm::vec3(1, 0.8, 0.1);
            dirLight.specular = glm::vec3(0.5f, 0.5f, 0.5f);
        }

        // input
        // -----
        processInput(window);


        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //------------------------------------------------------------
        //bindujemo nas framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //renderujemo vodu
        //----------
        planeShader.use();
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        planeShader.setMat4("view", view);
        planeShader.setMat4("projection", projection);

        glBindVertexArray(planeVAO);
        glBindTexture(GL_TEXTURE_2D, waterTexture);
        model = glm::translate(model, glm::vec3(0.0f, 0.08f, 0.0f));
        planeShader.setMat4("model", model);


        planeShader.setVec3("dirLight.direction", dirLight.direction);
        planeShader.setVec3("dirLight.ambient", dirLight.ambient);
        planeShader.setVec3("dirLight.diffuse", dirLight.diffuse);
        planeShader.setVec3("dirLight.specular", dirLight.specular);
        planeShader.setFloat("shininess", 1.0f);
        planeShader.setBool("noc", noc);


        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        //---------------

        // don't forget to enable shader before setting uniforms
        modelShader.use();
        modelShader.setBool("noc", noc);
        modelShader.setVec3("pointLight.position", pointLight.position);
        modelShader.setVec3("pointLight.ambient", pointLight.ambient);
        modelShader.setVec3("pointLight.diffuse", pointLight.diffuse);
        modelShader.setVec3("pointLight.specular", pointLight.specular);
        modelShader.setFloat("pointLight.constant", pointLight.constant);
        modelShader.setFloat("pointLight.linear", pointLight.linear);
        modelShader.setFloat("pointLight.quadratic", pointLight.quadratic);
        modelShader.setVec3("viewPosition", programState->camera.Position);

        modelShader.setVec3("dirLight.direction", dirLight.direction);
        modelShader.setVec3("dirLight.ambient", dirLight.ambient);
        modelShader.setVec3("dirLight.diffuse", dirLight.diffuse);
        modelShader.setVec3("dirLight.specular", dirLight.specular);
        modelShader.setFloat("material.shininess", 16.0f);


        // view/projection transformations
        projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        view = programState->camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);


        //ostrvo
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.02f));
        modelShader.setFloat("material.shininess", 4.0f);
        modelShader.setMat4("model", model);
        ostrvo.Draw(modelShader);

        modelShader.setFloat("material.shininess", 32.0f);

        //logorska vatra
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.5f, -0.2f, -1.0f));
        model = glm::scale(model, glm::vec3(0.15f));
        modelShader.setMat4("model", model);
        vatra.Draw(modelShader);

        //brod1
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.5f, -0.8f, -20.0f));
        model = glm::rotate(model, glm::radians(-75.0f), glm::vec3(0, 1.0f, 0));
        model = glm::scale(model, glm::vec3(0.35f));
        modelShader.setMat4("model", model);
        brod.Draw(modelShader);

        //brod2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-15.0f, -0.3f, 4.0f));
        model = glm::rotate(model, glm::radians(-180.0f), glm::vec3(0, 0, 1));
        model = glm::rotate(model, glm::radians(30.0f), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(0.2f));
        modelShader.setMat4("model", model);
        brod.Draw(modelShader);

        //dzek
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, -0.12f, -2.0f));
        model = glm::rotate(model, glm::radians(-45.0f), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(0.005f));
        modelShader.setMat4("model", model);
        modelShader.setFloat("material.shininess", 16.0f);
        dzek.Draw(modelShader);



        //renderujemo biljke
        //iskljucujemo face culling da bi se videla sa obe strane
        glDisable(GL_CULL_FACE);
        glBindVertexArray(transparentVAO);
        glBindTexture(GL_TEXTURE_2D, transparentTexture);

        blendingShader.use();
        blendingShader.setInt("texture1", 0);
        blendingShader.setMat4("projection", projection);
        blendingShader.setMat4("view", view);

        blendingShader.setVec3("dirLight.direction", dirLight.direction);
        blendingShader.setVec3("dirLight.ambient", dirLight.ambient);
        blendingShader.setVec3("dirLight.diffuse", dirLight.diffuse);
        blendingShader.setVec3("dirLight.specular", dirLight.specular);
        blendingShader.setFloat("shininess", 32.0f);
        blendingShader.setBool("noc", noc);

       //biljke ispred kamena
        for (int i = 0; i < 4; i++) {
            model = glm::mat4(1.0f);
            model = glm::rotate(model, glm::radians(-45.0f), glm::vec3(0, 1, 0));
            model = glm::translate(model, biljkePozicije[i]);
            model = glm::scale(model, glm::vec3(0.8f));
            blendingShader.use();
            blendingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        //kruzna biljka
        for (int i = 0; i < 8; i++) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-2.2f, 0.08f, 1.5f));
            model = glm::scale(model, glm::vec3(0.8f));
            model = glm::rotate(model, glm::radians((float)i * (-45.0f)), glm::vec3(0, 1, 0));
            blendingShader.use();
            blendingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glEnable(GL_CULL_FACE);


        //renderujemo skybox kao poslednji
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);

        unsigned int activeSkybox;
        if (noc) {
            activeSkybox = cubemapTextureNight;
        } else {
            activeSkybox = cubemapTextureDay;
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, activeSkybox);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //kraj renderovanja u nas frejmbafer
        //--------------------------------------------


        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
//        hdrShader.setInt("bloom", bloom);
        hdrShader.setInt("hdr", hdr);
        hdrShader.setFloat("exposure", exposure);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);



        /*
        //HDR - renderujemo quad
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        hdrShader.setInt("hdr", hdr);
        hdrShader.setFloat("exposure", exposure);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        */



        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.0f)
            exposure -= 0.1f;
        else
            exposure = 0.0f;
        std::cout << exposure << endl;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.1f;
        std::cout << exposure << endl;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 0.5);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 0.5);
        ImGui::DragFloat3("dirLight.diffuse", (float*)&programState->dirLight.direction);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        programState->CameraMouseMovementUpdateEnabled = !programState->CameraMouseMovementUpdateEnabled;
    }

    if (key == GLFW_KEY_N && action == GLFW_PRESS){
        noc = !noc;
        if (noc) {
            hdr = true;
        } else {
            hdr = false;
        }
    }

    if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        hdr = !hdr;
    }

}


unsigned int loadTexture(char const *path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}


unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}