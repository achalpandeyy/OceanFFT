#include <Application.h>
#include <Mesh.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

namespace Ogle
{
    struct VertexBuffer
    {
        VertexBuffer(void* vertices, size_t vertices_size)
        {
            glGenBuffers(1, &id);
            glBindBuffer(GL_ARRAY_BUFFER, id);
            glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_STATIC_DRAW);
        }

        ~VertexBuffer()
        {
            glDeleteBuffers(1, &id);
        }

        GLuint id;
    };

    struct IndexBuffer
    {
        IndexBuffer(void* indices, size_t indices_size)
        {
            glGenBuffers(1, &id);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);
        }

        ~IndexBuffer()
        {
            glDeleteBuffers(1, &id);
        }

        GLuint id;
    };

    struct VertexAttribs
    {

    };

    struct VertexArray
    {

        GLuint id;
    };
}

struct OceanFFT : public Ogle::Application
{
    OceanFFT()
    {
        settings.enable_cursor = false;
        settings.enable_debug_callback = true;
        settings.width = 1280;
        settings.height = 720;
        settings.window_title = "OceanFFT | Ogle";
    }

    virtual void Initialize() override
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

        // Ogle::VertexAttribs attribs = {};
        // grid_vao = std::make_unique<Ogle::VertexArray>(grid_vbo.get(), grid_ibo.get(), sizeof(GridVertex), 2, attribs);
    }

    virtual void Update() override
    {

    }
private:
    std::unique_ptr<Ogle::VertexBuffer> grid_vbo = nullptr;
    std::unique_ptr<Ogle::IndexBuffer> grid_ibo = nullptr;
    std::unique_ptr<Ogle::VertexArray> grid_vao = nullptr;

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