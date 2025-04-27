#pragma once

using namespace DirectX;

class Terrain
{
private:
	struct VertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector3 normal;
		DirectX::SimpleMath::Vector4 colour;
	};

	struct HeightMapType
	{
		float x, y, z;
		float nx, ny, nz;
		float u, v;
		DirectX::SimpleMath::Vector4 colour;
	};

	struct VoronoiRegion
	{
		DirectX::SimpleMath::Vector2 seedPoint;
		DirectX::SimpleMath::Vector4 color;
		float heightOffset;
	};

public:
	Terrain();
	~Terrain();

	bool Initialize(ID3D11Device*, int terrainWidth, int terrainHeight);
	void Render(ID3D11DeviceContext*);
	bool Update();

	float* GetWavelength();
	float* GetAmplitude();

	// For random height map
	void SetRandomSeed(unsigned int seed);
	bool GenerateRandomHeightMap(ID3D11Device*);

	bool GenerateHeightMap(ID3D11Device*);
	bool GeneratePerlinNoiseTerrain(ID3D11Device* device, float scale = 1.0f, int octaves = 4);
	bool GenerateVoronoiRegions(ID3D11Device* device, int numRegions);

private:
	bool CalculateNormals();
	void Shutdown();
	void ShutdownBuffers();
	bool InitializeBuffers(ID3D11Device*);
	void RenderBuffers(ID3D11DeviceContext*);
	bool CalculateNormalsAndInitializeBuffers(ID3D11Device* device);

	// Perlin Noise helper methods
	float PerlinNoise2D(float x, float y);
	float Fade(float t);
	float Lerp(float t, float a, float b);
	float Grad(int hash, float x, float y);
	void GeneratePermutationTable();

	DirectX::SimpleMath::Vector4 GetColorByHeight(float height);
	float CalculateDistance(float x1, float y1, float x2, float y2);
	DirectX::SimpleMath::Vector4 GenerateRandomColor();

private:
	bool m_terrainGeneratedToggle;
	int m_terrainWidth, m_terrainHeight;
	ID3D11Buffer * m_vertexBuffer, *m_indexBuffer;
	int m_vertexCount, m_indexCount;
	float m_frequency, m_amplitude, m_wavelength;
	HeightMapType* m_heightMap;

	//arrays for our generated objects Made by directX
	std::vector<VertexPositionNormalTexture> preFabVertices;
	std::vector<uint16_t> preFabIndices;

	// Random number generator for random height map
	unsigned int m_randomSeed;

	// Noise generation parameters
	std::vector<int> m_permutation;

	std::vector<VoronoiRegion> m_voronoiRegions;
};

