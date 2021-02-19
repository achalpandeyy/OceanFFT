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
#include <stb_image.h>
#include <stb_image_write.h>
#include <complex>
#include <chrono>

#define GRID_DIM 256
#define RESOLUTION 512
#define DISPLACEMENT_MAP_DIM 256
#define WORK_GROUP_DIM 32

#define PI 3.14159265359f

typedef float real_type;
typedef std::complex<real_type> complex_type;

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

    void BitReverseOrder(complex_type* array, int count)
    {
        for (int i = 0, j = 1; j < count; ++j)
        {
            for (int k = count >> 1; k > (i ^= k); k >>= 1);

            if (i < j)
                std::swap(array[i], array[j]);
        }
    }

    void Initialize() override
    {
        // shader = std::make_unique<Ogle::Shader>("../Source/Shaders/Shader.comp");
        fft_horizontal_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTHorizontal.comp");
        fft_vertical_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTVertical.comp");
        
        int width, height, channel_count;
        stbi_uc* image_data = stbi_load("C:/Projects/sandbox/data/gpu_fft/awesomeface_grayscale.png", &width, &height, &channel_count, 0);

        std::vector<complex_type> image_data_complex((size_t)width * height);
        stbi_uc* pixel = image_data;
        for (size_t i = 0; i < image_data_complex.size(); ++i)
            image_data_complex[i] = (*pixel++) / 255.f;

        stbi_image_free(image_data);

        Ogle::Texture2D image_complex_gpu(RESOLUTION, RESOLUTION, GL_RG32F, GL_RG, GL_FLOAT, GL_NEAREST, GL_NEAREST,
            image_data_complex.data());
        Ogle::Texture2D temp(RESOLUTION, RESOLUTION, GL_RG32F, GL_RG, GL_FLOAT);

        // Rows
        fft_horizontal_program->Bind();
        fft_horizontal_program->SetInt("u_total_count", RESOLUTION);
        
        bool temp_as_input = false; // Should you use temp_texture as input to the FFT shader?
        int stage_count = std::log2(RESOLUTION);
        
        for (int p = 1; p < RESOLUTION; p <<= 1)
        // for (int stage = 1; stage <= stage_count; ++stage)
        {
            if (temp_as_input)
            {
                temp.BindImage(0, GL_READ_ONLY, temp.internal_format);
                image_complex_gpu.BindImage(1, GL_WRITE_ONLY, image_complex_gpu.internal_format);
            }
            else
            {
                image_complex_gpu.BindImage(0, GL_READ_ONLY, image_complex_gpu.internal_format);
                temp.BindImage(1, GL_WRITE_ONLY, temp.internal_format);
            }
        
            fft_horizontal_program->SetInt("u_subseq_count", p);
            // fft_horizontal_program->SetInt("u_subseq_count", 1 << stage);
        
            // Note: One work group per row
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
                temp.BindImage(0, GL_READ_ONLY, temp.internal_format);
                image_complex_gpu.BindImage(1, GL_WRITE_ONLY, image_complex_gpu.internal_format);
            }
            else
            {
                image_complex_gpu.BindImage(0, GL_READ_ONLY, image_complex_gpu.internal_format);
                temp.BindImage(1, GL_WRITE_ONLY, temp.internal_format);
            }
        
            fft_vertical_program->SetInt("u_subseq_count", p);
            // fft_vertical_program->SetInt("u_subseq_count", 1 << stage);
        
            // Note: One work group per col
            glDispatchCompute(RESOLUTION, 1, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        
            temp_as_input = !temp_as_input;
        }

        std::vector<complex_type> out_pixels((size_t)width * height);
        // glGetTextureImage(temp.id, 0, GL_RG, GL_FLOAT, out_pixels.size() * 2 * 4, out_pixels.data());
        glGetTextureImage(image_complex_gpu.id, 0, GL_RG, GL_FLOAT, image_data_complex.size() * 2 * 4, out_pixels.data());

        // Magnitude and Phase extraction for test and debug stuff
        
        // Magnitude extraction
        std::vector<float> magnitude_array((size_t)width * height);
        
        float max_magnitude = 0.f;
        for (size_t l = 0; l < height; ++l)
        {
            for (size_t k = 0; k < width; ++k)
            {
                size_t transformed_k = (k + (width / 2)) % width;
                size_t transformed_l = (l + (height / 2)) % height;
        
                const complex_type& in_pixel = out_pixels[transformed_l * width + transformed_k];
        
                float magnitude = std::abs(in_pixel);
                magnitude_array[l * width + k] = magnitude;
        
                if (magnitude > max_magnitude)
                    max_magnitude = magnitude;
            }
        }
        
        if (max_magnitude == 0.f) max_magnitude = 1.f;
        
        const float c = 1.f / std::log(1.f + max_magnitude);
        
        for (size_t l = 0; l < height; ++l)
        {
            for (size_t k = 0; k < width; ++k)
            {
                float magnitude = magnitude_array[l * width + k];
                float normalized_magnitude = c * std::log(1.f + magnitude);
        
                magnitude_array[l * width + k] = normalized_magnitude;
            }
        }
        
        std::vector<unsigned char> mag_u8((size_t)width * height);
        for (size_t i = 0; i < mag_u8.size(); ++i)
            mag_u8[i] = unsigned char(255 * magnitude_array[i]);
        
        stbi_write_png("C:/Projects/sandbox/data/gpu_fft/awesomeface_grayscale_mag.png", width, height, 1, mag_u8.data(), width);
        
        // Phase extraction
        std::vector<float> phase_floats((size_t)height * width);
        for (size_t l = 0; l < height; ++l)
        {
            for (size_t k = 0; k < width; ++k)
            {
                int transformed_k = (k + (width / 2)) % width;
                int transformed_l = (l + (height / 2)) % height;
        
                const complex_type& in_pixel = out_pixels[transformed_l * width + transformed_k];
        
                float phase = 0.5f + (std::atan2(in_pixel.imag(), in_pixel.real()) / PI) * 0.5f;
                if (phase < 0.f) phase = 0.f;
                if (phase > 1.f) phase = 1.f;
        
                phase_floats[l * width + k] = phase;
            }
        }
        
        std::vector<unsigned char> phase_u8((size_t)width* height);
        for (size_t i = 0; i < phase_u8.size(); ++i)
            phase_u8[i] = unsigned char(255 * phase_floats[i]);
        
        stbi_write_png("C:/Projects/sandbox/data/gpu_fft/awesomeface_grayscale_phase.png", width, height, 1, phase_u8.data(), width);

        // stbi_write_png("C:/Projects/sandbox/data/gpu_test_out_from_oceanfft_result_the_new_one.png",
        //     width, height, 1, out_pixels_real.data(), width);

        // GLuint ping_tex;
        // glGenTextures(1, &ping_tex);
        // glBindTexture(GL_TEXTURE_1D, ping_tex);
        // 
        // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // 
        // glTexImage1D(GL_TEXTURE_1D, 0, GL_RG32F, cpu_in.size(), 0, GL_RG, GL_FLOAT, cpu_in.data());
        // glBindImageTexture(0, ping_tex, 0, GL_FALSE, 0,  GL_READ_ONLY, GL_RG32F);
        // 
        // glBindTexture(GL_TEXTURE_1D, 0);
        // 
        // GLuint pong_tex;
        // glGenTextures(1, &pong_tex);
        // glBindTexture(GL_TEXTURE_1D, pong_tex);
        // 
        // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        // glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // 
        // glTexImage1D(GL_TEXTURE_1D, 0, GL_RG32F, cpu_in.size(), 0, GL_RG, GL_FLOAT, 0);
        // glBindImageTexture(1, pong_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
        // 
        // glBindTexture(GL_TEXTURE_1D, 0);
        // 
        // bool read_from_ping = true;
        // 
        // shader->Bind();
        // shader->SetInt("u_total_count", cpu_in.size());
        // 
        // GLint work_group_dims[3];
        // glGetProgramiv(shader->id, GL_COMPUTE_WORK_GROUP_SIZE, work_group_dims);
        // 
        // std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();
        // for (int p = 1; p < cpu_in.size(); p <<= 1)
        // {
        //     shader->Bind();
        //     shader->SetInt("u_subseq_count", p);
        // 
        //     if (read_from_ping)
        //     {
        //         glBindImageTexture(0, ping_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
        //         glBindImageTexture(1, pong_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
        //     }
        //     else
        //     {
        //         glBindImageTexture(0, pong_tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32F);
        //         glBindImageTexture(1, ping_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);
        //     }
        // 
        //     glDispatchCompute(cpu_in.size() / (2ll * work_group_dims[0]), 1, 1);
        //     glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // 
        //     read_from_ping = !read_from_ping;
        // }
        // std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
        // 
        // std::cout << "Time taken = " << std::chrono::duration_cast<std::chrono::milliseconds> (end - begin).count() << "[ms]" << std::endl;
        // 
        // read_from_ping ? glBindTexture(GL_TEXTURE_1D, ping_tex) : glBindTexture(GL_TEXTURE_1D, pong_tex);
        // 
        // std::vector<complex_type> cpu_out(cpu_in.size());
        // glGetTexImage(GL_TEXTURE_1D, 0, GL_RG, GL_FLOAT, cpu_out.data());
        // 
        // for (size_t i = 0; i < cpu_out.size(); ++i)
        //     std::cout << '{' << cpu_out[i].real() << ',' << cpu_out[i].imag() << "},";
        // 
        // std::cout << std::endl;

        // // Generate grid
        // const int vertex_count = GRID_DIM + 1;
        // 
        // std::vector<GridVertex> vertices(vertex_count * vertex_count);
        // std::vector<unsigned int> indices(GRID_DIM * GRID_DIM * 2 * 3);
        // 
        // // float tex_coord_scale = 64.f;
        // float tex_coord_scale = 1.f;
        // 
        // unsigned int idx = 0;
        // for (int z = -GRID_DIM / 2; z <= GRID_DIM / 2; ++z)
        // {
        //     for (int x = -GRID_DIM / 2; x <= GRID_DIM / 2; ++x)
        //     {
        //         vertices[idx].position = glm::vec3(float(x), 0.f, float(z));
        // 
        //         float u = ((float)x / GRID_DIM) + 0.5f;
        //         float v = ((float)z / GRID_DIM) + 0.5f;
        //         vertices[idx++].tex_coords = glm::vec2(u, v) * tex_coord_scale;
        //     }
        // }
        // assert(idx == vertices.size());
        // 
        // // Clockwise winding
        // idx = 0;
        // for (unsigned int y = 0; y < GRID_DIM; ++y)
        // {
        //     for (unsigned int x = 0; x < GRID_DIM; ++x)
        //     {
        //         indices[idx++] = (vertex_count * y) + x;
        //         indices[idx++] = (vertex_count * (y + 1)) + x;
        //         indices[idx++] = (vertex_count * y) + x + 1;
        // 
        //         indices[idx++] = (vertex_count * y) + x + 1;
        //         indices[idx++] = (vertex_count * (y + 1)) + x;
        //         indices[idx++] = (vertex_count * (y + 1)) + x + 1;
        //     }
        // }
        // assert(idx == indices.size());
        // 
        // grid_vbo = std::make_unique<Ogle::VertexBuffer>(vertices.data(), vertices.size() * sizeof(GridVertex));
        // grid_ibo = std::make_unique<Ogle::IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));
        // 
        // Ogle::VertexAttribs attribs[] = { { 3, 0 }, { 2, offsetof(GridVertex, tex_coords) } };
        // grid_vao = std::make_unique<Ogle::VertexArray>(grid_vbo.get(), grid_ibo.get(), attribs, 2, (GLsizei)sizeof(GridVertex));
        // 
        // camera = std::make_unique<Ogle::Camera>(glm::vec3(220.f, 120.f, 160.f), 1000.f);
        // 
        // float quad_vertices[] =
        // {
        //     -1.f, -1.f,
        //      1.f, -1.f,
        //      1.f,  1.f,
        //     -1.f,  1.f
        // };
        // 
        // unsigned int quad_indices[] = { 0, 1, 2, 2, 3, 0 };
        // 
        // quad_vbo = std::make_unique<Ogle::VertexBuffer>(quad_vertices, sizeof(quad_vertices));
        // quad_ibo = std::make_unique<Ogle::IndexBuffer>(quad_indices, sizeof(quad_indices));
        // 
        // Ogle::VertexAttribs quad_attribs[] = { {2, 0} };
        // quad_vao = std::make_unique<Ogle::VertexArray>(quad_vbo.get(), quad_ibo.get(), quad_attribs, 1, (GLsizei)(2 * sizeof(float)));
        // 
        // quad_shader = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/DebugQuadViz.vert",
        //     "C:/Projects/OceanFFT/Source/Shaders/DebugQuadViz.frag");
        // 
        // // Initial spectrum
        // initial_spectrum_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_InitialSpectrum.comp");
        // initial_spectrum_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT);
        // 
        // // Phase
        // std::vector<float> ping_phase_array(RESOLUTION * RESOLUTION);
        // 
        // std::random_device dev;
        // std::mt19937 rng(dev());
        // std::uniform_real_distribution<float> dist(0.f, 1.f);
        // 
        // for (size_t i = 0; i < RESOLUTION * RESOLUTION; ++i)
        // {
        //     ping_phase_array[i] = dist(rng) * 2.f * PI;
        // }
        // 
        // ping_phase_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT, GL_NEAREST, GL_NEAREST,
        //     ping_phase_array.data());
        // pong_phase_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_R32F, GL_RED, GL_FLOAT);
        // 
        // phase_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_Phase.comp");
        // 
        // // Time-varying spectrum
        // 
        // spectrum_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_Spectrum.comp");
        // spectrum_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        // 
        // ping_transform_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        // temp_texture = std::make_unique<Ogle::Texture2D>(RESOLUTION, RESOLUTION, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        // 
        // fft_horizontal_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTHorizontal.comp");
        // fft_vertical_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/CS_FFTVertical.comp");
        // 
        // ocean_program = std::make_unique<Ogle::Shader>("C:/Projects/OceanFFT/Source/Shaders/Grid.vert",
        //     "C:/Projects/OceanFFT/Source/Shaders/Ocean.frag");
    }

    void Update() override
    {
        // Note: You can remove imgui_demo.cpp from the project if you don't need this ImGui::ShowDemoWindow call, which you won't
        // need eventually
        // if (show_debug_gui)
        // {
        //     ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        //     bool ocean_size_changed = ImGui::SliderInt("Ocean Size", &ocean_size, 100, 1000);
        // 
        //     bool wind_changed = ImGui::SliderFloat("Wind Magnitude", &wind_magnitude, 5.f, 25.f)
        //         || ImGui::SliderFloat("Wind Angle", &wind_angle, 0, 359);
        // 
        //     ImGui::SliderFloat("Choppiness", &choppiness, 0.f, 2.5f);
        // 
        //     changed = ocean_size_changed || wind_changed;
        //     // ImGui::ShowDemoWindow(&show_debug_gui);
        // }
        // 
        // if (changed)
        // {
        //     initial_spectrum_program->Bind();
        //     initial_spectrum_program->SetInt("u_ocean_size", ocean_size);
        // 
        //     float wind_angle_rad = glm::radians(wind_angle);
        //     initial_spectrum_program->SetVec2("u_wind", wind_magnitude * glm::cos(wind_angle_rad), wind_magnitude * glm::sin(wind_angle_rad));
        //     initial_spectrum_texture->BindImage(0, GL_WRITE_ONLY, initial_spectrum_texture->internal_format);
        // 
        //     glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        //     glFinish();
        // 
        //     changed = false;
        // }
        // 
        // glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Todo: I don't think you need this call here
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Todo: I don't think you need this call here
        // 
        // glViewport(0, 0, settings.width, settings.height); // Todo: I don't think you need this call here
        // 
        // glEnable(GL_DEPTH_TEST);    // Todo: I don't think you need this call here
        // glEnable(GL_CULL_FACE);     // Todo: I don't think you need this call here
        // glDisable(GL_BLEND);        // Todo: I don't think you need this call here
        // 
        // phase_program->Bind();
        // 
        // phase_program->SetInt("u_ocean_size", ocean_size);
        // phase_program->SetFloat("u_delta_time", (float)glfwGetTime());
        // 
        // // Todo: Why do you need ping-ponging here, again?
        // if (is_ping_phase)
        // {
        //     ping_phase_texture->BindImage(0, GL_READ_ONLY, ping_phase_texture->internal_format);
        //     pong_phase_texture->BindImage(1, GL_WRITE_ONLY, pong_phase_texture->internal_format);
        // }
        // else
        // {
        //     ping_phase_texture->BindImage(1, GL_WRITE_ONLY, ping_phase_texture->internal_format);
        //     pong_phase_texture->BindImage(0, GL_READ_ONLY, pong_phase_texture->internal_format);
        // }
        // 
        // glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        // glFinish();
        // 
        // spectrum_program->Bind();
        // 
        // spectrum_program->SetInt("u_ocean_size", ocean_size);
        // spectrum_program->SetFloat("u_choppiness", choppiness);
        // 
        // is_ping_phase ? pong_phase_texture->BindImage(0, GL_READ_ONLY, pong_phase_texture->internal_format)
        //     : ping_phase_texture->BindImage(0, GL_READ_ONLY, ping_phase_texture->internal_format);
        // initial_spectrum_texture->BindImage(1, GL_READ_ONLY, initial_spectrum_texture->internal_format);
        // 
        // spectrum_texture->BindImage(2, GL_WRITE_ONLY, spectrum_texture->internal_format);
        // 
        // glDispatchCompute(RESOLUTION / WORK_GROUP_DIM, RESOLUTION / WORK_GROUP_DIM, 1);
        // glFinish();
        // 
        // // Todo: Put this FFT code into a function
        // 
        // // Rows
        // fft_horizontal_program->Bind();
        // fft_horizontal_program->SetInt("u_total_count", RESOLUTION);
        // 
        // bool temp_as_input = false; // Should you use temp_texture as input to the FFT shader?
        // 
        // int stage_count = log2(RESOLUTION);
        // for (int stage = 1; stage <= stage_count; ++stage)
        // {
        //     if (temp_as_input)
        //     {
        //         temp_texture->BindImage(0, GL_READ_ONLY, temp_texture->internal_format);
        //         spectrum_texture->BindImage(1, GL_WRITE_ONLY, spectrum_texture->internal_format);
        //     }
        //     else
        //     {
        //         spectrum_texture->BindImage(0, GL_READ_ONLY, spectrum_texture->internal_format);
        //         temp_texture->BindImage(1, GL_WRITE_ONLY, temp_texture->internal_format);
        //     }
        // 
        //     fft_horizontal_program->SetInt("u_subseq_count", 1 << stage);
        // 
        //     // Note: One work group per row
        //     glDispatchCompute(RESOLUTION, 1, 1);
        //     glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // 
        //     temp_as_input = !temp_as_input;
        // }
        // 
        // // Cols
        // fft_vertical_program->Bind();
        // fft_vertical_program->SetInt("u_total_count", RESOLUTION);
        // 
        // for (int stage = 1; stage <= stage_count; ++stage)
        // {
        //     if (temp_as_input)
        //     {
        //         temp_texture->BindImage(0, GL_READ_ONLY, temp_texture->internal_format);
        //         spectrum_texture->BindImage(1, GL_WRITE_ONLY, spectrum_texture->internal_format);
        //     }
        //     else
        //     {
        //         spectrum_texture->BindImage(0, GL_READ_ONLY, spectrum_texture->internal_format);
        //         temp_texture->BindImage(1, GL_WRITE_ONLY, temp_texture->internal_format);
        //     }
        // 
        //     fft_vertical_program->SetInt("u_subseq_count", 1 << stage);
        // 
        //     // Note: One work group per col
        //     glDispatchCompute(RESOLUTION, 1, 1);
        //     glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // 
        //     temp_as_input = !temp_as_input;
        // }
        // 
        // 
        // 
        // glClear(GL_COLOR_BUFFER_BIT);
        // 
        // ocean_program->Bind();
        // ocean_program->SetMat4("u_pv", glm::value_ptr(camera->GetProjViewMatrix((float)settings.width / settings.height)));
        // 
        // ocean_program->SetInt("u_input", 0);
        // temp_as_input ? temp_texture->Bind(0) : spectrum_texture->Bind(0);
        // // spectrum_texture->Bind(0);
        // // temp_texture->Bind(0);
        // 
        // is_ping_phase = !is_ping_phase;
        // 
        // grid_vao->Bind();
        // grid_ibo->Bind();
        // 
        // glDrawElements(GL_TRIANGLES, GRID_DIM * GRID_DIM * 2 * 3, GL_UNSIGNED_INT, 0);
        
        // // Visualize Quad
        // quad_shader->Bind();
        // 
        // quad_shader->SetInt("s_texture", 0);
        // initial_spectrum->Bind(0);
        // 
        // quad_ibo->Bind();
        // quad_vao->Bind();
        // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void OnKeyPress(int key_code) override
    {
        // Todo:
        // 1. I would like the client to not touch GLFW code.
        // 2. This destroys the coherence between settings.enable_cursor and the
        // actual state of the cursor, fix this.

        // if (key_code == GLFW_KEY_G)
        // {
        //     if (!show_debug_gui)
        //     {
        //         glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        //         show_debug_gui = true;
        //     }
        //     else
        //     {
        //         glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        //         show_debug_gui = false;
        //     }
        // }
        // 
        // if (!show_debug_gui)
        //     camera->ProcessKeyboard(key_code, delta_time);
    }
    
    void OnMouseMove(float x_offset, float y_offset) override
    {
        // if (!show_debug_gui)
        //     camera->ProcessMouseMove(x_offset, y_offset);
    }
    
    void OnMouseScroll(float vertical_offset) override
    {
        // if (!show_debug_gui)
        //     camera->ProcessMouseScroll(vertical_offset);
    }

private:
    std::unique_ptr<Ogle::Shader> shader = nullptr;

    std::unique_ptr<Ogle::VertexBuffer> grid_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> grid_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> grid_vao = nullptr;

    std::unique_ptr<Ogle::Camera> camera = nullptr;

    std::unique_ptr<Ogle::Texture2D> initial_spectrum_texture = nullptr;

    bool is_ping_phase = true;
    std::unique_ptr<Ogle::Texture2D> ping_phase_texture = nullptr;
    std::unique_ptr<Ogle::Texture2D> pong_phase_texture = nullptr;
    std::unique_ptr<Ogle::Shader> phase_program = nullptr;

    std::unique_ptr<Ogle::Texture2D> spectrum_texture = nullptr;
    std::unique_ptr<Ogle::Shader> spectrum_program = nullptr;

    std::unique_ptr<Ogle::Texture2D> ping_transform_texture = nullptr;
    std::unique_ptr<Ogle::Texture2D> temp_texture = nullptr;
    std::unique_ptr<Ogle::Shader> fft_horizontal_program = nullptr;
    std::unique_ptr<Ogle::Shader> fft_vertical_program = nullptr;

    std::unique_ptr<Ogle::Shader> initial_spectrum_program = nullptr;
    std::unique_ptr<Ogle::Shader> tilde_h_k_t_program = nullptr;

    std::unique_ptr<Ogle::Shader> butterfly_precomp_program = nullptr;
    std::unique_ptr<Ogle::Shader> butterfly_fft_program = nullptr;
    std::unique_ptr<Ogle::Shader> butterfly_inversion_program = nullptr;

    std::unique_ptr<Ogle::Shader> normal_map_program = nullptr;
    std::unique_ptr<Ogle::Shader> ocean_wireframe_program = nullptr;
    std::unique_ptr<Ogle::Shader> ocean_program = nullptr;

    std::unique_ptr<Ogle::VertexBuffer> quad_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> quad_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> quad_vao = nullptr;
    std::unique_ptr<Ogle::Shader> quad_shader = nullptr;

    bool show_debug_gui = false;
    bool changed = true;

    int ocean_size = 250;
    float wind_magnitude = 14.142135f;
    float wind_angle = 45.f;
    float choppiness = 1.5f;

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