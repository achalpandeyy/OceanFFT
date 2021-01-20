#include <Application.h>
#include <Camera.h>
#include <Shader.h>
#include <Texture2D.h>
#include <Mesh.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

#define DISPLACEMENT_MAP_DIM 256
#define COMPUTE_WORK_GROUP_COUNT 32

struct OceanFFT final : public Ogle::Application
{
    OceanFFT()
    {
        settings.width = 256;
        settings.height = 256;
        settings.window_title = "OceanFFT | Ogle";
        settings.enable_debug_callback = true;
        settings.enable_cursor = true;
    }

    void Initialize() override
    {
        // For each X & Z
        const int grid_dim = 1024;
        const int vertex_count = grid_dim + 1;

        std::vector<GridVertex> vertices(vertex_count * vertex_count);
        std::vector<unsigned int> indices(grid_dim * grid_dim * 2 * 3);

        unsigned int idx = 0;
        for (int z = -grid_dim / 2; z <= grid_dim / 2; ++z)
        {
            for (int x = -grid_dim / 2; x <= grid_dim / 2; ++x)
            {
                vertices[idx].position = glm::vec3(float(x), 0.f, float(z));

                float u = ((float)x / grid_dim) + 0.5f;
                float v = ((float)z / grid_dim) + 0.5f;
                vertices[idx++].tex_coords = glm::vec2(u, v); // For some apparent reason, uv grid is scaled here???
            }
        }
        assert(idx == vertices.size());

        // Clockwise winding
        idx = 0;
        for (unsigned int y = 0; y < grid_dim; ++y)
        {
            for (unsigned int x = 0; x < grid_dim; ++x)
            {
                indices[idx++] = (vertex_count * y) + x;
                indices[idx++] = (vertex_count * (y + 1)) + x;
                indices[idx++] = (vertex_count * y) + x + 1;

                indices[idx++] = (vertex_count * y) + x + 1;
                indices[idx++] = (vertex_count * (y + 1)) + x;
                indices[idx++] = (vertex_count * (y + 1)) + x + 1;
            }
        }
        assert(idx == indices.size());

        grid_vbo = std::make_unique<Ogle::VertexBuffer>(vertices.data(), vertices.size() * sizeof(GridVertex));
        grid_ibo = std::make_unique<Ogle::IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));

        Ogle::VertexAttribs attribs[] = { { 3, 0 }, { 2, offsetof(GridVertex, tex_coords) } };
        grid_vao = std::make_unique<Ogle::VertexArray>(grid_vbo.get(), grid_ibo.get(), attribs, 2, (GLsizei)sizeof(GridVertex));

        // Create Shader
        grid_shader = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/DefaultShader.vert",
            "C:/Projects/OceanFFT/Source/Shaders/DefaultShader.frag");

        camera = std::make_unique<Ogle::Camera>(glm::vec3(0.f, 300.f, 300.f), 10000.f);

        float quad_vertices[] =
        {
            -1.f, -1.f,
             1.f, -1.f,
             1.f,  1.f,
            -1.f,  1.f
        };

        unsigned int quad_indices[] = { 0, 1, 2, 2, 3, 0 };

        quad_vbo = std::make_unique<Ogle::VertexBuffer>(quad_vertices, sizeof(quad_vertices));
        quad_ibo = std::make_unique<Ogle::IndexBuffer>(quad_indices, sizeof(quad_indices));

        Ogle::VertexAttribs quad_attribs[] = { {2, 0} };
        quad_vao = std::make_unique<Ogle::VertexArray>(quad_vbo.get(), quad_ibo.get(), quad_attribs, 1, (GLsizei)(2 * sizeof(float)));

        quad_shader = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/DebugQuadViz.vert",
            "C:/Projects/OceanFFT/Source/Shaders/DebugQuadViz.frag");

        // Generate tilde_h0_k
        tilde_h0_k_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/TildeH0K.comp");

        noise0 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_0.png"));
        noise1 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_1.png"));
        noise2 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_2.png"));
        noise3 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_3.png"));

        tilde_h0_k = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RG32F, GL_RG, GL_FLOAT);
        tilde_h0_minus_k = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RG32F, GL_RG, GL_FLOAT);

        tilde_h0_k_program->Bind();

        tilde_h0_k_program->SetInt("s_Noise0", 0);
        noise0->Bind(0);

        tilde_h0_k_program->SetInt("s_Noise1", 1);
        noise1->Bind(1);

        tilde_h0_k_program->SetInt("s_Noise2", 2);
        noise2->Bind(2);

        tilde_h0_k_program->SetInt("s_Noise3", 3);
        noise3->Bind(3);

        tilde_h0_k->BindImage(0, GL_WRITE_ONLY, GL_RG32F);
        tilde_h0_minus_k->BindImage(1, GL_WRITE_ONLY, GL_RG32F);
        
        glClearColor(0.f, 0.f, 0.f, 1.f);

        unsigned int work_group_count = DISPLACEMENT_MAP_DIM / 32;
        glDispatchCompute(work_group_count, work_group_count, 1);

        glFinish();
    }

    void Update() override
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // grid_shader->Bind();
        // grid_shader->SetMat4("u_PV", glm::value_ptr(camera->GetProjViewMatrix((float)settings.width / settings.height)));
        // 
        // grid_vao->Bind();
        // grid_ibo->Bind();
        // glDrawElements(GL_TRIANGLES, 1024 * 1024 * 2 * 3, GL_UNSIGNED_INT, 0);

        quad_shader->Bind();

        quad_shader->SetInt("s_Texture", 0);
        // tilde_h0_k->Bind(0);
        tilde_h0_minus_k->Bind(0);

        quad_ibo->Bind();
        quad_vao->Bind();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void OnKeyPress(int key_code) override
    {
        camera->ProcessKeyboard(key_code, delta_time);
    }

    void OnMouseMove(float x_offset, float y_offset) override
    {
        camera->ProcessMouseMove(x_offset, y_offset);
    }

    void OnMouseScroll(float vertical_offset) override
    {
        camera->ProcessMouseScroll(vertical_offset);
    }

private:
    std::unique_ptr<Ogle::VertexBuffer> grid_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> grid_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> grid_vao = nullptr;
    std::unique_ptr<Ogle::Shader> grid_shader = nullptr;

    std::unique_ptr<Ogle::Camera> camera = nullptr;

    std::unique_ptr<Ogle::Texture2D> noise0 = nullptr;
    std::unique_ptr<Ogle::Texture2D> noise1 = nullptr;
    std::unique_ptr<Ogle::Texture2D> noise2 = nullptr;
    std::unique_ptr<Ogle::Texture2D> noise3 = nullptr;
    std::unique_ptr<Ogle::Texture2D> tilde_h0_k = nullptr;
    std::unique_ptr<Ogle::Texture2D> tilde_h0_minus_k = nullptr;
    std::unique_ptr<Ogle::Shader> tilde_h0_k_program = nullptr;

    std::unique_ptr<Ogle::VertexBuffer> quad_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> quad_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> quad_vao = nullptr;
    std::unique_ptr<Ogle::Shader> quad_shader = nullptr;

    // Todo: Probably you don't have to store texture coordinates, they could be
    // computed on the fly by the vertex shader
    struct GridVertex
    {
        glm::vec3 position;
        glm::vec2 tex_coords;
    };
};

int main()
{
    OceanFFT app;
    return app.Run();
}