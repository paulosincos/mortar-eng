/* This file is part of mortar.
 *
 * mortar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mortar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with mortar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lsw.hpp"

using namespace Mortar::LSW;

static struct Mesh readMeshInfo(Stream &stream, const uint32_t body_offset, uint32_t mesh_offset) {
	stream.seek(body_offset + mesh_offset, SEEK_SET);
	struct Mesh mesh;

	mesh.next_offset = stream.readUint32();

	stream.seek(sizeof(uint32_t), SEEK_CUR);

	mesh.material_idx = stream.readUint32();
	mesh.vertex_type = stream.readUint32();

	stream.seek(3 * sizeof(uint32_t), SEEK_CUR);

	mesh.vertex_block_idx = stream.readUint32();

	stream.seek(4 * sizeof(uint32_t), SEEK_CUR);

	mesh.chunk_offset = stream.readUint32();

	return mesh;
}

static struct Chunk readChunkInfo(Stream &stream, const uint32_t body_offset, uint32_t chunk_offset) {
	stream.seek(body_offset + chunk_offset, SEEK_SET);
	struct Chunk chunk;

	chunk.next_offset = stream.readUint32();
	chunk.primitive_type = stream.readUint32();

	chunk.num_elements = stream.readUint16();

	stream.seek(sizeof(uint16_t), SEEK_CUR);

	chunk.elements_offset = stream.readUint32();

	return chunk;
}

void Mortar::LSW::processMesh(Stream &stream, const uint32_t body_offset, uint32_t mesh_header_offset, glm::mat4 transform, Model *model, std::vector<Model::VertexBuffer> &vertexBuffers) {
	if (!mesh_header_offset)
		return;

	stream.seek(body_offset + mesh_header_offset, SEEK_SET);
	struct MeshHeader mesh_header;

	stream.seek(3 * sizeof(uint32_t), SEEK_CUR);

	mesh_header.mesh_offset = stream.readUint32();

	if (!mesh_header.mesh_offset)
		return;

	struct Mesh mesh = readMeshInfo(stream, body_offset, mesh_header.mesh_offset);
	bool processNextMesh;

	do {
		int stride = 0;

		/* Vertex stride is specified per-mesh. */
		switch (mesh.vertex_type) {
			case 89:
				stride = 36;
				break;
			case 93:
				stride = 56;
				break;
			default:
				fprintf(stderr, "Unknown vertex type %d\n", mesh.vertex_type);
		}

		int vertex_block_idx = mesh.vertex_block_idx - 1;

		vertexBuffers[vertex_block_idx].stride = stride;

		struct Chunk chunk_data = readChunkInfo(stream, body_offset, mesh.chunk_offset);
		bool processNextChunk;

		do {
			Model::Chunk chunk = Model::Chunk();

			chunk.vertex_buffer_idx = vertex_block_idx;
			chunk.material_idx = mesh.material_idx;
			chunk.primitive_type = chunk_data.primitive_type;
			chunk.num_elements = chunk_data.num_elements;

			chunk.transformation = transform;

			chunk.element_buffer = new uint16_t[chunk_data.num_elements];

			stream.seek(body_offset + chunk_data.elements_offset, SEEK_SET);

			for (int i = 0; i < chunk_data.num_elements; i++)
				chunk.element_buffer[i] = stream.readUint16();

			model->addChunk(chunk);

			processNextChunk = false;
			if (chunk_data.next_offset) {
				chunk_data = readChunkInfo(stream, body_offset, chunk_data.next_offset);
				processNextChunk = true;
			}
		} while (processNextChunk);

		processNextMesh = false;
		if (mesh.next_offset) {
			mesh = readMeshInfo(stream, body_offset, mesh.next_offset);
			processNextMesh = true;
		}
	} while (processNextMesh);
}
