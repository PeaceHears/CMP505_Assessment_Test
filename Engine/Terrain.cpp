#include "pch.h"
#include "Terrain.h"
#include <random>
#include "Utils.h"

Terrain::Terrain()
{
	m_terrainGeneratedToggle = false;

	// Default random seed
	m_randomSeed = static_cast<unsigned int>(std::time(nullptr));
}


Terrain::~Terrain()
{

}

void Terrain::SetRandomSeed(unsigned int seed)
{
	m_randomSeed = seed;
}

bool Terrain::Initialize(ID3D11Device* device, int terrainWidth, int terrainHeight)
{
	int index;
	float height = 0.0;
	bool result;

	FillVoronoiRegionColours();

	// Save the dimensions of the terrain.
	m_terrainWidth = terrainWidth;
	m_terrainHeight = terrainHeight;

	m_frequency = m_terrainWidth / 20;
	m_amplitude = 3.0;
	m_wavelength = 1;

	// Create the structure to hold the terrain data.
	m_heightMap = new HeightMapType[m_terrainWidth * m_terrainHeight];
	if (!m_heightMap)
	{
		return false;
	}

	//this is how we calculate the texture coordinates first calculate the step size there will be between vertices. 
	float textureCoordinatesStep = 5.0f / m_terrainWidth;  //tile 5 times across the terrain. 
	// Initialise the data in the height map (flat).
	for (int j = 0; j<m_terrainHeight; j++)
	{
		for (int i = 0; i<m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)height;
			m_heightMap[index].z = (float)j;

			//and use this step to calculate the texture coordinates for this point on the terrain.
			m_heightMap[index].u = (float)i * textureCoordinatesStep;
			m_heightMap[index].v = (float)j * textureCoordinatesStep;

		}
	}

	//even though we are generating a flat terrain, we still need to normalise it. 
	// Calculate the normals for the terrain data.
	result = CalculateNormals();
	if (!result)
	{
		return false;
	}

	// Initialize the vertex and index buffer that hold the geometry for the terrain.
	result = InitializeBuffers(device);
	if (!result)
	{
		return false;
	}

	return true;
}

void Terrain::Render(ID3D11DeviceContext * deviceContext)
{
	// Put the vertex and index buffers on the graphics pipeline to prepare them for drawing.
	RenderBuffers(deviceContext);
	deviceContext->DrawIndexed(m_indexCount, 0, 0);

	return;
}

bool Terrain::CalculateNormals()
{
	int i, j, index1, index2, index3, index, count;
	float vertex1[3], vertex2[3], vertex3[3], vector1[3], vector2[3], sum[3], length;
	DirectX::SimpleMath::Vector3* normals;
	
	// Create a temporary array to hold the un-normalized normal vectors.
	normals = new DirectX::SimpleMath::Vector3[(m_terrainHeight - 1) * (m_terrainWidth - 1)];
	if (!normals)
	{
		return false;
	}

	// Go through all the faces in the mesh and calculate their normals.
	for (j = 0; j<(m_terrainHeight - 1); j++)
	{
		for (i = 0; i<(m_terrainWidth - 1); i++)
		{
			index1 = (j * m_terrainHeight) + i;
			index2 = (j * m_terrainHeight) + (i + 1);
			index3 = ((j + 1) * m_terrainHeight) + i;

			// Get three vertices from the face.
			vertex1[0] = m_heightMap[index1].x;
			vertex1[1] = m_heightMap[index1].y;
			vertex1[2] = m_heightMap[index1].z;

			vertex2[0] = m_heightMap[index2].x;
			vertex2[1] = m_heightMap[index2].y;
			vertex2[2] = m_heightMap[index2].z;

			vertex3[0] = m_heightMap[index3].x;
			vertex3[1] = m_heightMap[index3].y;
			vertex3[2] = m_heightMap[index3].z;

			// Calculate the two vectors for this face.
			vector1[0] = vertex1[0] - vertex3[0];
			vector1[1] = vertex1[1] - vertex3[1];
			vector1[2] = vertex1[2] - vertex3[2];
			vector2[0] = vertex3[0] - vertex2[0];
			vector2[1] = vertex3[1] - vertex2[1];
			vector2[2] = vertex3[2] - vertex2[2];

			index = (j * (m_terrainHeight - 1)) + i;

			// Calculate the cross product of those two vectors to get the un-normalized value for this face normal.
			normals[index].x = (vector1[1] * vector2[2]) - (vector1[2] * vector2[1]);
			normals[index].y = (vector1[2] * vector2[0]) - (vector1[0] * vector2[2]);
			normals[index].z = (vector1[0] * vector2[1]) - (vector1[1] * vector2[0]);
		}
	}

	// Now go through all the vertices and take an average of each face normal 	
	// that the vertex touches to get the averaged normal for that vertex.
	for (j = 0; j<m_terrainHeight; j++)
	{
		for (i = 0; i<m_terrainWidth; i++)
		{
			// Initialize the sum.
			sum[0] = 0.0f;
			sum[1] = 0.0f;
			sum[2] = 0.0f;

			// Initialize the count.
			count = 0;

			// Bottom left face.
			if (((i - 1) >= 0) && ((j - 1) >= 0))
			{
				index = ((j - 1) * (m_terrainHeight - 1)) + (i - 1);

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Bottom right face.
			if ((i < (m_terrainWidth - 1)) && ((j - 1) >= 0))
			{
				index = ((j - 1) * (m_terrainHeight - 1)) + i;

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Upper left face.
			if (((i - 1) >= 0) && (j < (m_terrainHeight - 1)))
			{
				index = (j * (m_terrainHeight - 1)) + (i - 1);

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Upper right face.
			if ((i < (m_terrainWidth - 1)) && (j < (m_terrainHeight - 1)))
			{
				index = (j * (m_terrainHeight - 1)) + i;

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Take the average of the faces touching this vertex.
			sum[0] = (sum[0] / (float)count);
			sum[1] = (sum[1] / (float)count);
			sum[2] = (sum[2] / (float)count);

			// Calculate the length of this normal.
			length = sqrt((sum[0] * sum[0]) + (sum[1] * sum[1]) + (sum[2] * sum[2]));

			// Get an index to the vertex location in the height map array.
			index = (j * m_terrainHeight) + i;

			// Normalize the final shared normal for this vertex and store it in the height map array.
			m_heightMap[index].nx = (sum[0] / length);
			m_heightMap[index].ny = (sum[1] / length);
			m_heightMap[index].nz = (sum[2] / length);
		}
	}

	// Release the temporary normals.
	delete[] normals;
	normals = 0;

	return true;
}

void Terrain::Shutdown()
{
	// Release the index buffer.
	if (m_indexBuffer)
	{
		m_indexBuffer->Release();
		m_indexBuffer = 0;
	}

	// Release the vertex buffer.
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = 0;
	}

	return;
}

bool Terrain::InitializeBuffers(ID3D11Device * device )
{
	VertexType* vertices;
	unsigned long* indices;
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;
	int index, i, j;
	int index1, index2, index3, index4; //geometric indices. 

	// Calculate the number of vertices in the terrain mesh.
	m_vertexCount = (m_terrainWidth - 1) * (m_terrainHeight - 1) * 6;

	// Set the index count to the same as the vertex count.
	m_indexCount = m_vertexCount;

	// Create the vertex array.
	vertices = new VertexType[m_vertexCount];
	if (!vertices)
	{
		return false;
	}

	// Create the index array.
	indices = new unsigned long[m_indexCount];
	if (!indices)
	{
		return false;
	}

	// Initialize the index to the vertex buffer.
	index = 0;

	for (j = 0; j < (m_terrainHeight - 1); j++)
	{
		for (i = 0; i < (m_terrainWidth - 1); i++)
		{
			index1 = (m_terrainHeight * j) + i;          // Bottom left.
			index2 = (m_terrainHeight * j) + (i + 1);      // Bottom right.
			index3 = (m_terrainHeight * (j + 1)) + i;      // Upper left.
			index4 = (m_terrainHeight * (j + 1)) + (i + 1);  // Upper right.

			// Upper left.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index3].y);
			vertices[index].colour = m_heightMap[index3].colour;
			indices[index] = index;
			index++;

			// Upper right.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index4].y);
			vertices[index].colour = m_heightMap[index4].colour;
			indices[index] = index;
			index++;

			// Bottom left.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index1].y);
			vertices[index].colour = m_heightMap[index1].colour;
			indices[index] = index;
			index++;

			// Bottom left.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index1].y);
			vertices[index].colour = m_heightMap[index1].colour;
			indices[index] = index;
			index++;

			// Upper right.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index4].y);
			vertices[index].colour = m_heightMap[index4].colour;
			indices[index] = index;
			index++;

			// Bottom right.
			vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
			vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
			vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
			//vertices[index].colour = GetColorByHeight(m_heightMap[index2].y);
			vertices[index].colour = m_heightMap[index2].colour;
			indices[index] = index;
			index++;
		}
	}

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Release the arrays now that the vertex and index buffers have been created and loaded.
	delete[] vertices;
	vertices = 0;

	delete[] indices;
	indices = 0;

	return true;
}

void Terrain::RenderBuffers(ID3D11DeviceContext * deviceContext)
{
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(VertexType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return;
}

bool Terrain::CalculateNormalsAndInitializeBuffers(ID3D11Device* device)
{
	bool result = false;

	result = CalculateNormals();
	if (!result)
	{
		return false;
	}

	result = InitializeBuffers(device);
	if (!result)
	{
		return false;
	}

	return true;
}

bool Terrain::GenerateHeightMap(ID3D11Device* device)
{
	bool result = false;
	int index = 0;
	float height = 0.0;

	m_frequency = (6.283/m_terrainHeight) / m_wavelength; //we want a wavelength of 1 to be a single wave over the whole terrain.  A single wave is 2 pi which is about 6.283

	//loop through the terrain and set the hieghts how we want. This is where we generate the terrain
	//in this case I will run a sin-wave through the terrain in one axis.

	for (int j = 0; j<m_terrainHeight; j++)
	{
		for (int i = 0; i<m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)(sin((float)i *(m_frequency))*m_amplitude); 
			m_heightMap[index].z = (float)j;
		}
	}

	result = CalculateNormalsAndInitializeBuffers(device);
	return result;
}

bool Terrain::GenerateRandomHeightMap(ID3D11Device* device)
{
	bool result = false;
	int index = 0;

	// Create random engine with the stored seed
	std::mt19937 randomEngine(m_randomSeed);

	// Create a uniform distribution
	std::uniform_real_distribution<float> heightDistribution(0.0f, m_amplitude);

	// Loop through the terrain and set random heights
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			// Generate random height
			float randomHeight = heightDistribution(randomEngine);

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = randomHeight;
			m_heightMap[index].z = (float)j;
		}
	}

	result = CalculateNormalsAndInitializeBuffers(device);
	return result;
}

void Terrain::GeneratePermutationTable()
{
	// Create and shuffle permutation table
	m_permutation.resize(512);
	std::vector<int> basePermutation(256);

	// Initialize base permutation
	for (int i = 0; i < 256; i++)
	{
		basePermutation[i] = i;
	}

	// Shuffle the base permutation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::shuffle(basePermutation.begin(), basePermutation.end(), gen);

	// Duplicate the permutation to avoid overflow
	for (int i = 0; i < 256; i++)
	{
		m_permutation[i] = basePermutation[i];
		m_permutation[i + 256] = basePermutation[i];
	}
}

float Terrain::Fade(float t)
{
	// Smoothing function: 6t^5 - 15t^4 + 10t^3
	return t * t * t * (t * (t * 6 - 15) + 10);
}

float Terrain::Lerp(float t, float a, float b)
{
	// Linear interpolation
	return a + t * (b - a);
}

float Terrain::Grad(int hash, float x, float y)
{
	// Ensure hash is within the permutation table range
	hash &= 255;  // Limit to 0-255

	// Compute gradient based on hash
	switch (hash & 3)
	{
	case 0:  return  x + y;
	case 1:  return -x + y;
	case 2:  return  x - y;
	case 3:  return -x - y;
	default: return 0;
	}
}

float Terrain::PerlinNoise2D(float x, float y)
{
	// Ensure we have a permutation table
	if (m_permutation.empty())
	{
		GeneratePermutationTable();
	}

	// Find unit grid cell containing point
	int X = static_cast<int>(std::floor(x)) & 255;
	int Y = static_cast<int>(std::floor(y)) & 255;

	// Relative coordinates within the cell
	x -= std::floor(x);
	y -= std::floor(y);

	// Compute fade curves for x and y
	float u = Fade(x);
	float v = Fade(y);

	// Hash coordinates of the 4 square corners
	int A = m_permutation[X] + Y;
	int AA = m_permutation[A & 255];
	int AB = m_permutation[(A + 1) & 255];
	int B = m_permutation[(X + 1) & 255] + Y;
	int BA = m_permutation[B & 255];
	int BB = m_permutation[(B + 1) & 255];

	// Blend corner gradients
	return Lerp(v,
		Lerp(u,
			Grad(m_permutation[AA], x, y),
			Grad(m_permutation[BA], x - 1, y)
		),
		Lerp(u,
			Grad(m_permutation[AB], x, y - 1),
			Grad(m_permutation[BB], x - 1, y - 1)
		)
	);
}

bool Terrain::GeneratePerlinNoiseTerrain(ID3D11Device* device, float scale, int octaves)
{
	bool result = false;

	// Clamp octaves to prevent excessive computation
	octaves = std::max(1, std::min(octaves, 8));

	// Initialize the height map
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			int index = (m_terrainHeight * j) + i;

			// Scale coordinates
			float x = static_cast<float>(i) / scale;
			float y = static_cast<float>(j) / scale;

			float amplitude = 1.0f;
			float frequency = 1.0f;
			float noiseValue = 0.0f;
			float totalAmplitude = 0.0f;

			// Sum noise contributions (Fractal Brownian Motion)
			for (int o = 0; o < octaves; o++)
			{
				noiseValue += PerlinNoise2D(x * frequency, y * frequency) * amplitude;
				totalAmplitude += amplitude;

				// Update parameters for the next octave
				amplitude *= 0.5f;   // Reduce amplitude for higher octaves
				frequency *= 2.0f;   // Increase frequency for higher octaves
			}

			// Normalize the noise value
			if (totalAmplitude > 0.0f)
			{
				noiseValue /= totalAmplitude;
			}

			// Scale height to desired range (adjust as needed)
			m_heightMap[index].y = noiseValue * m_amplitude;
		}
	}

	result = CalculateNormalsAndInitializeBuffers(device);
	return result;

	return true;
}

DirectX::SimpleMath::Vector4 Terrain::GetColorByHeight(float height)
{
	// Define your height ranges and color mappings
	// Adjust these values as necessary for your desired effect
	if (height < -1.0f) return DirectX::Colors::Blue;     // Low heights (water)
	else if (height < 0.0f) return DirectX::Colors::LightBlue; // Land below sea level
	else if (height < 1.0f) return DirectX::Colors::Green;   // Low land
	else if (height < 3.0f) return DirectX::Colors::DarkGreen; // Hills
	else if (height < 5.0f) return DirectX::Colors::Brown;    // Mountains
	else return DirectX::Colors::White;                      // Snow-capped peaks
}

bool Terrain::GenerateVoronoiRegions(ID3D11Device* device, int numRegions)
{
	bool result = false;

	// Clear existing regions
	m_voronoiRegions.clear();

	// Random number generator setup
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> disX(0, m_terrainWidth - 1);
	std::uniform_real_distribution<> disZ(0, m_terrainHeight - 1);
	std::uniform_real_distribution<> disHeight(-1.0f, 1.0f);

	// Generate seed points for each region
	for (int i = 0; i < numRegions; i++)
	{
		VoronoiRegion region;
		region.seedPoint = DirectX::SimpleMath::Vector2(
			static_cast<float>(disX(gen)),
			static_cast<float>(disZ(gen))
		);
		region.colour = GetRandomColour();
		region.colourVector = m_voronoiRegionColours[region.colour];

		const auto regionSize = 30.0f; // Adjust based on the terrain size
		region.minX = std::max(0.0f, region.seedPoint.x - regionSize / 2);
		region.maxX = std::min(static_cast<float>(m_terrainWidth - 1), region.seedPoint.x + regionSize / 2);
		region.minZ = std::max(0.0f, region.seedPoint.y - regionSize / 2);
		region.maxZ = std::min(static_cast<float>(m_terrainHeight - 1), region.seedPoint.y + regionSize / 2);
		
		region.heightOffset = disHeight(gen);

		m_voronoiRegions.push_back(region);
	}

	// Assign regions and modify terrain
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			int index = (m_terrainHeight * j) + i;

			// Find the closest region seed point
			float minDistance = std::numeric_limits<float>::max();
			VoronoiRegion* closestRegion = nullptr;

			for (auto& region : m_voronoiRegions)
			{
				float distance = CalculateDistance(
					static_cast<float>(i), static_cast<float>(j),
					region.seedPoint.x, region.seedPoint.y
				);

				if (distance < minDistance)
				{
					minDistance = distance;
					closestRegion = &region;
				}
			}

			// Modify terrain based on closest region
			if (closestRegion)
			{
				// Color coding
				m_heightMap[index].colour = closestRegion->colourVector;

				// Optional height modification
				m_heightMap[index].y += closestRegion->heightOffset * 0.5f;
			}
		}
	}

	for (auto& region : m_voronoiRegions)
	{
		region.position = GetRegionPosition(&region);
	}

	result = CalculateNormalsAndInitializeBuffers(device);
	return result;
}

float Terrain::CalculateDistance(float x1, float y1, float x2, float y2) const
{
	float dx = x1 - x2;
	float dy = y1 - y2;
	return std::sqrt(dx * dx + dy * dy);
}

void Terrain::FillVoronoiRegionColours()
{
	m_voronoiRegionColours[Enums::COLOUR::Red] = DirectX::Colors::Red;
	m_voronoiRegionColours[Enums::COLOUR::Blue] = DirectX::Colors::Blue;
	m_voronoiRegionColours[Enums::COLOUR::Green] = DirectX::Colors::Green;
	m_voronoiRegionColours[Enums::COLOUR::Goldenrod] = DirectX::Colors::Goldenrod;
	m_voronoiRegionColours[Enums::COLOUR::Yellow] = DirectX::Colors::Yellow;
	m_voronoiRegionColours[Enums::COLOUR::Orange] = DirectX::Colors::Orange;
	m_voronoiRegionColours[Enums::COLOUR::Pink] = DirectX::Colors::Pink;
	m_voronoiRegionColours[Enums::COLOUR::SlateGray] = DirectX::Colors::SlateGray;
	m_voronoiRegionColours[Enums::COLOUR::Violet] = DirectX::Colors::Violet;
	m_voronoiRegionColours[Enums::COLOUR::RosyBrown] = DirectX::Colors::RosyBrown;
}

const Enums::COLOUR& Terrain::GetRandomColour() const
{
	//std::random_device rd;
	//std::mt19937 gen(rd());
	//std::uniform_real_distribution<> dis(0.0f, 1.0f);

	//return DirectX::SimpleMath::Vector4(
	//	dis(gen),  // Red
	//	dis(gen),  // Green
	//	dis(gen),  // Blue
	//	1.0f       // Full opacity
	//);

	const auto voronoiRegionColourCount = m_voronoiRegionColours.size();
	const auto randomIndex = Utils::GetRandomInt(0, voronoiRegionColourCount - 1);
	Enums::COLOUR randomColour;

	for (size_t i = 0; i < voronoiRegionColourCount; i++)
	{
		if (i == randomIndex)
		{
			randomColour = static_cast<Enums::COLOUR>(i);
			break;
		}
	}

	return randomColour;
}

bool Terrain::Update()
{
	return true; 
}

float* Terrain::GetWavelength()
{
	return &m_wavelength;
}

float* Terrain::GetAmplitude()
{
	return &m_amplitude;
}

const Enums::COLOUR& Terrain::GetRandomVoronoiRegionColour() const
{
	const auto voronoiRegionsCount = m_voronoiRegions.size();
	const auto randomIndex = Utils::GetRandomInt(0, voronoiRegionsCount - 1);
	const auto randomRegion = m_voronoiRegions[randomIndex];

	return randomRegion.colour;
}

const Enums::COLOUR& Terrain::GetRegionColourAtPosition(float x, float z) const
{
	Enums::COLOUR colour;

	for (const auto& region : m_voronoiRegions)
	{
		if (x >= region.minX && x <= region.maxX &&
			z >= region.minZ && z <= region.maxZ)
		{
			colour = region.colour;
			break;
		}
	}

	return colour;
}

const DirectX::SimpleMath::Vector4& Terrain::GetVoronoiRegionColourVector(const Enums::COLOUR& colour) const
{
	return m_voronoiRegionColours.at(colour);
}

const DirectX::SimpleMath::Vector3& Terrain::GetRegionPosition(VoronoiRegion* region) const
{
	DirectX::SimpleMath::Vector3 regionPosition(0.0f, 0.0f, 0.0f);
	int pointCount = 0;

	// Iterate through terrain height map
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			int index = (m_terrainHeight * j) + i;

			// Check if point belongs to this region
			if (IsPointInRegion(i, j, region))
			{
				regionPosition.x += m_heightMap[index].x;
				regionPosition.y += m_heightMap[index].y;
				regionPosition.z += m_heightMap[index].z;
				pointCount++;
			}
		}
	}

	// Calculate average if points found
	if (pointCount > 0)
	{
		regionPosition.x /= pointCount;
		regionPosition.y /= pointCount;
		regionPosition.z /= pointCount;
	}

	return regionPosition;
}

const bool Terrain::IsPointInRegion(int x, int z, VoronoiRegion* region) const
{
	//const bool withinBounds = (x >= region->minX && x <= region->maxX &&
	//							z >= region->minZ && z <= region->maxZ);
	//return withinBounds;

	float distance = CalculateDistance(
		static_cast<float>(x),
		static_cast<float>(z),
		region->seedPoint.x,
		region->seedPoint.y
	);

	// Use bounding box size as maximum distance
	float maxDistance = std::max(
		region->maxX - region->minX,
		region->maxZ - region->minZ
	) / 2.0f;

	return distance <= maxDistance;
}