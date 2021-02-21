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
#include <imgui.h>
#include <iostream>
#include <vector>
#include <random>

#define GRID_DIM 1024
#define RESOLUTION 512
#define WORK_GROUP_DIM 32

#define PI 3.14159265359f

struct OceanFFT final : public Ogle::Application
{
    // Note: Don't try to call anything GL in the constructor because it hasn't been initialized yet
    OceanFFT()
    {
        settings.width = 1280;
        settings.height = 720;
        settings.window_title = "OceanFFT | Ogle";
        settings.enable_debug_callback = true;
        settings.enable_cursor = false;
    }

    void Initialize() override
    {
        // Generate grid
        const int vertex_count = GRID_DIM + 1;
        
        std::vector<GridVertex> vertices(vertex_count * vertex_count);
        std::vector<unsigned int> indices(GRID_DIM * GRID_DIM * 2 * 3);
        
        float tex_coord_scale = 2.f;
        
        unsigned int idx = 0;
        for (int z = -GRID_DIM / 2; z <= GRID_DIM / 2; ++z)
        {
            for (int x = -GRID_DIM / 2; x <= GRID_DIM / 2; ++x)
            {
                vertices[idx].position = glm::vec3(float(x), 0.f, float(z));
        
                float u = ((float)x / GRID_DIM) + 0.5f;
                float v = ((float)z / GRID_DIM) + 0.5f;
                vertices[idx++].tex_coords = glm::vec2(u, v) * tex_coord_scale;
            }
        }
        assert(idx == vertices.size());
        
        // Clockwise winding
        idx = 0;
        for (unsigned int y = 0; y < GRID_DIM; ++y)
        {
            for (unsigned int x = 0; x < GRID_DIM; ++x)
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
        
        camera = std::make_unique<Ogle::Camera>(glm::vec3(0.f, 60.f, 0.f), 0.1f, 10000.f, 1000.f);
        
        // Initial spectrum
        initial_spectrum_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_InitialSpectrum.comp");
        initial_spectrum_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT);
        
        // Phase
        std::vector<float> ping_phase_array(RESOLUTION * RESOLUTION);
        
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_real_distribution<float> dist(0.f, 1.f);
        
        for (size_t i = 0; i < RESOLUTION * RESOLUTION; ++i)
        {
            ping_phase_array[i] = dist(rng) * 2.f * PI;
        }

        phase_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_Phase.comp");
        ping_phase_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT, GL_NEAREST, GL_NEAREST,
            GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER, ping_phase_array.data());
        pong_phase_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT);
        
        // Time-varying spectrum
        
        spectrum_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_Spectrum.comp");
        spectrum_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_LINEAR, GL_LINEAR,
            GL_REPEAT, GL_REPEAT);
        
        temp_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        
        fft_horizontal_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTHorizontal.comp");
        fft_vertical_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTVertical.comp");

        // Normal map

        normal_map_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_NormalMap.comp");
        normal_map = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_LINEAR, GL_LINEAR,
            GL_REPEAT, GL_REPEAT);
        
        // Ocean shading

        ocean_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/VS_Ocean.vert",
            "C:/Projects/OceanFFT/Source/Shaders/FS_Ocean.frag");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glClearColor(0.9f, 0.9f, 0.9f, 1.f);
    }

    void Update() override
    {
        // Note: You can remove imgui_demo.cpp from the project if you don't need this ImGui::ShowDemoWindow call, which you won't
        // need eventually
        if (show_debug_gui)
        {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            bool ocean_size_changed = ImGui::SliderInt("Ocean Size", &ocean_size, 100, 1000);
            
            // Todo: Check this wind magnitude range for the ocean_size variable
            bool wind_changed = ImGui::SliderFloat("Wind Magnitude", &wind_magnitude, 5.5f, 25.f)
                || ImGui::SliderFloat("Wind Angle", &wind_angle, 0, 359);
        
            ImGui::SliderFloat("Choppiness", &choppiness, 0.f, 2.5f);
        
            changed = ocean_size_changed || wind_changed;
            // ImGui::ShowDemoWindow(&show_debug_gui);
        }
        
        if (changed)
        {
            initial_spectrum_program->Bind();
            initial_spectrum_program->SetInt("u_ocean_size", ocean_size);
            initial_spectrum_program->SetInt("u_resolution", RESOLUTION);
        
            float wind_angle_rad = glm::radians(wind_angle);
            initial_spectrum_program->SetVec2("u_wind", wind_magnitude * glm::cos(wind_angle_rad), wind_magnitude * glm::sin(wind_angle_rad));
            initial_spectrum_texture->BindImage(0, GL_WRITE_ONLY, initial_spectrum_texture->internal_format);
        
            glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
            glFinish();
        
            changed = false;
        }
        
        phase_program->Bind();
        phase_program->SetInt("u_ocean_size", ocean_size);
        phase_program->SetFloat("u_delta_time", delta_time);
        phase_program->SetInt("u_resolution", RESOLUTION);
        
        // Todo: Why do you need ping-ponging here, again?
        if (is_ping_phase)
        {
            ping_phase_texture->BindImage(0, GL_READ_ONLY, ping_phase_texture->internal_format);
            pong_phase_texture->BindImage(1, GL_WRITE_ONLY, pong_phase_texture->internal_format);
        }
        else
        {
            ping_phase_texture->BindImage(1, GL_WRITE_ONLY, ping_phase_texture->internal_format);
            pong_phase_texture->BindImage(0, GL_READ_ONLY, pong_phase_texture->internal_format);
        }
        
        glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        glFinish();
        
        spectrum_program->Bind();
        
        spectrum_program->SetInt("u_ocean_size", ocean_size);
        spectrum_program->SetFloat("u_choppiness", choppiness);
        
        is_ping_phase ? pong_phase_texture->BindImage(0, GL_READ_ONLY, pong_phase_texture->internal_format)
            : ping_phase_texture->BindImage(0, GL_READ_ONLY, ping_phase_texture->internal_format);
        initial_spectrum_texture->BindImage(1, GL_READ_ONLY, initial_spectrum_texture->internal_format);
        
        spectrum_texture->BindImage(2, GL_WRITE_ONLY, spectrum_texture->internal_format);
        
        glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        glFinish();
        
        // Todo: Put this FFT code into a function
        
        // Rows
        fft_horizontal_program->Bind();
        fft_horizontal_program->SetInt("u_total_count", RESOLUTION);
        
        bool temp_as_input = false; // Should you use temp_texture as input to the FFT shader?
        
        for (int p = 1; p < RESOLUTION; p <<= 1)
        {
            if (temp_as_input)
            {
                temp_texture->BindImage(0, GL_READ_ONLY, temp_texture->internal_format);
                spectrum_texture->BindImage(1, GL_WRITE_ONLY, spectrum_texture->internal_format);
            }
            else
            {
                spectrum_texture->BindImage(0, GL_READ_ONLY, spectrum_texture->internal_format);
                temp_texture->BindImage(1, GL_WRITE_ONLY, temp_texture->internal_format);
            }
        
            fft_horizontal_program->SetInt("u_subseq_count", p);
        
            // One work group per row
            glDispatchCompute(RESOLUTION, 1, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        
            temp_as_input = !temp_as_input;
        }
        
        // Cols
        fft_vertical_program->Bind();
        fft_vertical_program->SetInt("u_total_count", RESOLUTION);
        
        for (int p = 1; p < RESOLUTION; p <<= 1)
        {
            if (temp_as_input)
            {
                temp_texture->BindImage(0, GL_READ_ONLY, temp_texture->internal_format);
                spectrum_texture->BindImage(1, GL_WRITE_ONLY, spectrum_texture->internal_format);
            }
            else
            {
                spectrum_texture->BindImage(0, GL_READ_ONLY, spectrum_texture->internal_format);
                temp_texture->BindImage(1, GL_WRITE_ONLY, temp_texture->internal_format);
            }
        
            fft_vertical_program->SetInt("u_subseq_count", p);
        
            // One work group per col
            glDispatchCompute(RESOLUTION, 1, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        
            temp_as_input = !temp_as_input;
        }

        // Generate Normal Map

        normal_map_program->Bind();
        spectrum_texture->BindImage(0, GL_READ_ONLY, spectrum_texture->internal_format);
        normal_map->BindImage(1, GL_WRITE_ONLY, normal_map->internal_format);

        normal_map_program->SetInt("u_resolution", RESOLUTION);
        normal_map_program->SetInt("u_ocean_size", ocean_size);

        glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        glFinish();
        
        // Ocean Shading

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ocean_program->Bind();
        ocean_program->SetMat4("u_pv", glm::value_ptr(camera->GetProjViewMatrix((float)settings.width / settings.height)));
        ocean_program->SetVec3("u_world_camera_pos", camera->position.x, camera->position.y, camera->position.z);
        ocean_program->SetInt("u_ocean_size", ocean_size);
        
        ocean_program->SetInt("u_displacement_map", 0);
        temp_as_input ? temp_texture->Bind(0) : spectrum_texture->Bind(0); // Todo: Make it clear that here we are binding disp map

        ocean_program->SetInt("u_normal_map", 1);
        normal_map->Bind(1);
        
        is_ping_phase = !is_ping_phase;
        
        grid_vao->Bind();
        grid_ibo->Bind();

        // Todo: You need to set something (macro/uniform) indicating if its rendering wireframe mode
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glDrawElements(GL_TRIANGLES, GRID_DIM * GRID_DIM * 2 * 3, GL_UNSIGNED_INT, 0);
    }

    void OnKeyPress(int key_code) override
    {
        // Todo:
        // 1. I would like the client to not touch GLFW code.
        // 2. This destroys the coherence between settings.enable_cursor and the
        // actual state of the cursor, fix this.

        if (key_code == GLFW_KEY_G)
        {
            if (!show_debug_gui)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                show_debug_gui = true;
            }
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                show_debug_gui = false;
            }
        }
        
        if (!show_debug_gui)
            camera->ProcessKeyboard(key_code, delta_time);
    }
    
    void OnMouseMove(float x_offset, float y_offset) override
    {
        if (!show_debug_gui)
            camera->ProcessMouseMove(x_offset, y_offset);
    }
    
    void OnMouseScroll(float vertical_offset) override
    {
        if (!show_debug_gui)
            camera->ProcessMouseScroll(vertical_offset);
    }

private:
    std::unique_ptr<Ogle::VertexBuffer> grid_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> grid_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> grid_vao = nullptr;

    std::unique_ptr<Ogle::Camera> camera = nullptr;

    std::unique_ptr<Ogle::Shader> initial_spectrum_program = nullptr;
    std::unique_ptr<Ogle::Texture2D> initial_spectrum_texture = nullptr;

    bool is_ping_phase = true;
    std::unique_ptr<Ogle::Shader> phase_program = nullptr;
    std::unique_ptr<Ogle::Texture2D> ping_phase_texture = nullptr;
    std::unique_ptr<Ogle::Texture2D> pong_phase_texture = nullptr;

    std::unique_ptr<Ogle::Shader> spectrum_program = nullptr;
    std::unique_ptr<Ogle::Texture2D> spectrum_texture = nullptr;

    std::unique_ptr<Ogle::Shader> fft_horizontal_program = nullptr;
    std::unique_ptr<Ogle::Shader> fft_vertical_program = nullptr;
    std::unique_ptr<Ogle::Texture2D> temp_texture = nullptr;

    std::unique_ptr<Ogle::Shader> normal_map_program = nullptr;
    std::unique_ptr<Ogle::Texture2D> normal_map = nullptr;

    std::unique_ptr<Ogle::Shader> ocean_program = nullptr;

    bool show_debug_gui = false;
    bool changed = true;

    int ocean_size = 1024;

    float wind_magnitude = 14.142135f;
    float wind_angle = 45.f;
    float choppiness = 1.5f;

    // Todo: You don't need this struct, texture coordinates can be computed on the fly right?
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