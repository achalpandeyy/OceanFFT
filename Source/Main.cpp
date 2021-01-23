#include <Application.h>
#include <Camera.h>
#include <Shader.h>
#include <Texture2D.h>
#include <Mesh.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

#define DISPLACEMENT_MAP_DIM 256
#define COMPUTE_WORK_GROUP_DIM 32

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
        // Generate grid

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

        // Generate tilde_h0_k and tilde_h0_minus_k
        tilde_h0_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/TildeH0.comp");

        noise0 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_0.png"));
        noise1 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_1.png"));
        noise2 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_2.png"));
        noise3 = std::unique_ptr<Ogle::Texture2D>(Ogle::Texture2D::CreateFromFile("C:/Projects/OceanFFT/Resources/LDR_LLL1_3.png"));

        tilde_h0_k = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RG32F, GL_RG, GL_FLOAT);
        tilde_h0_minus_k = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RG32F, GL_RG, GL_FLOAT);

        tilde_h0_program->Bind();

        tilde_h0_k->BindImage(0, GL_WRITE_ONLY, tilde_h0_k->internal_format);
        tilde_h0_minus_k->BindImage(1, GL_WRITE_ONLY, tilde_h0_minus_k->internal_format);

        tilde_h0_program->SetInt("s_Noise0", 2);
        noise0->Bind(2);

        tilde_h0_program->SetInt("s_Noise1", 3);
        noise1->Bind(3);

        tilde_h0_program->SetInt("s_Noise2", 4);
        noise2->Bind(4);

        tilde_h0_program->SetInt("s_Noise3", 5);
        noise3->Bind(5);

        glDispatchCompute(32, 32, 1);
        glFinish();

        tilde_h_k_t_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/TildeHKT.comp");

        tilde_h_k_t_dx = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        tilde_h_k_t_dy = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        tilde_h_k_t_dz = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        
        // Precompute for butterfly fft --twiddle factors and samples

        butterfly_precomp_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/ButterflyPrecomp.comp");
        butterfly_precomp = std::make_unique<Ogle::Texture2D>(glm::log2(DISPLACEMENT_MAP_DIM), DISPLACEMENT_MAP_DIM, GL_RGBA32F, GL_RGBA,
            GL_FLOAT);

        butterfly_precomp_program->Bind();

        const unsigned int bit_reversed_indices[] =
        {
            0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
            0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
            0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
            0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
            0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
            0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
            0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
            0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
            0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
            0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
            0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
            0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
            0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
            0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
            0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
            0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
        };

        // Todo: OpenGL spec says, at the very least, UBOs will be no slower than SSBOs, it would be nice to
        // make this into a UBO and enjoy whatever perf gain I can

        Ogle::ShaderStorageBuffer ssbo(bit_reversed_indices, sizeof(bit_reversed_indices));
        ssbo.Bind();
        ssbo.BindBase(0);
        ssbo.Unbind();
        
        butterfly_precomp->BindImage(0, GL_WRITE_ONLY, butterfly_precomp->internal_format);
        
        glDispatchCompute(8, 32, 1);
        glFinish();

        butterfly_fft_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/ButterflyFFT.comp");
        butterfly_inversion_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/ButterflyInversion.comp");

        ping_pong1 = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_RGBA32F, GL_RGBA, GL_FLOAT);

        displacement_y = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_R32F, GL_RED, GL_FLOAT);
        displacement_x = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_R32F, GL_RED, GL_FLOAT);
        displacement_z = std::make_unique<Ogle::Texture2D>(DISPLACEMENT_MAP_DIM, DISPLACEMENT_MAP_DIM, GL_R32F, GL_RED, GL_FLOAT);
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

        tilde_h_k_t_program->Bind();

        tilde_h0_k->BindImage(0, GL_READ_ONLY, tilde_h0_k->internal_format);
        tilde_h0_minus_k->BindImage(1, GL_READ_ONLY, tilde_h0_minus_k->internal_format);
        tilde_h_k_t_dx->BindImage(2, GL_WRITE_ONLY, tilde_h_k_t_dx->internal_format);
        tilde_h_k_t_dy->BindImage(3, GL_WRITE_ONLY, tilde_h_k_t_dy->internal_format);
        tilde_h_k_t_dz->BindImage(4, GL_WRITE_ONLY, tilde_h_k_t_dz->internal_format);

        tilde_h_k_t_program->SetFloat("u_Time", (float)glfwGetTime());
        
        glDispatchCompute(32, 32, 1);
        
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        ButterflyFFT(tilde_h_k_t_dy, displacement_y);
        ButterflyFFT(tilde_h_k_t_dx, displacement_x);
        ButterflyFFT(tilde_h_k_t_dz, displacement_z);

        // Visualize Quad
        quad_shader->Bind();

        quad_shader->SetInt("s_Texture", 0);
        // tilde_h_k_t_dy->Bind(0);
        // butterfly_precomp->Bind(0);
        // displacement_y->Bind(0);
        // displacement_x->Bind(0);
        displacement_z->Bind(0);

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

    void ButterflyFFT(std::unique_ptr<Ogle::Texture2D>& ping_pong0, std::unique_ptr<Ogle::Texture2D>& displacement)
    {
        butterfly_fft_program->Bind();

        butterfly_precomp->BindImage(0, GL_READ_ONLY, butterfly_precomp->internal_format);
        ping_pong0->BindImage(1, GL_READ_WRITE, ping_pong0->internal_format);
        ping_pong1->BindImage(2, GL_READ_WRITE, ping_pong1->internal_format);

        unsigned int stage_count = glm::log2(DISPLACEMENT_MAP_DIM);
        unsigned int ping_pong_index = 0;

        // Horizontal FFT
        for (unsigned int stage = 0; stage < stage_count; ++stage)
        {
            butterfly_fft_program->SetInt("u_PingPongIndex", ping_pong_index);
            butterfly_fft_program->SetInt("u_FFTDirection", 0);
            butterfly_fft_program->SetInt("u_Stage", stage);

            glDispatchCompute(32, 32, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            ping_pong_index = 1 - ping_pong_index;
        }

        // Vertical FFT
        for (unsigned int stage = 0; stage < stage_count; ++stage)
        {
            butterfly_fft_program->SetInt("u_PingPongIndex", ping_pong_index);
            butterfly_fft_program->SetInt("u_FFTDirection", 1);
            butterfly_fft_program->SetInt("u_Stage", stage);

            glDispatchCompute(32, 32, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            ping_pong_index = 1 - ping_pong_index;
        }

        butterfly_inversion_program->Bind();
        butterfly_inversion_program->SetInt("u_PingPongIndex", ping_pong_index);

        displacement->BindImage(0, GL_WRITE_ONLY, displacement->internal_format);
        ping_pong0->BindImage(1, GL_READ_ONLY, ping_pong0->internal_format);
        ping_pong1->BindImage(2, GL_READ_ONLY, ping_pong1->internal_format);

        glDispatchCompute(32, 32, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

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
    std::unique_ptr<Ogle::Texture2D> tilde_h_k_t_dx = nullptr;
    std::unique_ptr<Ogle::Texture2D> tilde_h_k_t_dy = nullptr;
    std::unique_ptr<Ogle::Texture2D> tilde_h_k_t_dz = nullptr;

    std::unique_ptr<Ogle::Texture2D> butterfly_precomp = nullptr;
    std::unique_ptr<Ogle::Texture2D> ping_pong1 = nullptr;

    std::unique_ptr<Ogle::Texture2D> displacement_y = nullptr;
    std::unique_ptr<Ogle::Texture2D> displacement_x = nullptr;
    std::unique_ptr<Ogle::Texture2D> displacement_z = nullptr;

    std::unique_ptr<Ogle::Shader> tilde_h0_program = nullptr;
    std::unique_ptr<Ogle::Shader> tilde_h_k_t_program = nullptr;

    std::unique_ptr<Ogle::Shader> butterfly_precomp_program = nullptr;
    std::unique_ptr<Ogle::Shader> butterfly_fft_program = nullptr;
    std::unique_ptr<Ogle::Shader> butterfly_inversion_program = nullptr;

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