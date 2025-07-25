#pragma once

#include "Enums.h"
#include <map>

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
		DirectX::SimpleMath::Vector4 colourVector;
		DirectX::SimpleMath::Vector3 position;
		Enums::COLOUR colour;
		float minX, maxX;
		float minZ, maxZ;
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

	void SetScale(const float scale) { m_Scale = scale; }
	const float GetScale() const { return m_Scale; }

	void SetTranslation(const DirectX::SimpleMath::Vector3& translation) { m_Translation = translation; }
	const DirectX::SimpleMath::Vector3& GetTranslation() const { return m_Translation; }

	// For random height map
	void SetRandomSeed(unsigned int seed);
	bool GenerateRandomHeightMap(ID3D11Device*);

	bool GenerateHeightMap(ID3D11Device*);
	bool GeneratePerlinNoiseTerrain(ID3D11Device* device, float scale = 1.0f, int octaves = 4);
	bool GenerateVoronoiRegions(ID3D11Device* device, int numRegions);

	const Enums::COLOUR& GetRandomVoronoiRegionColour() const;
	const Enums::COLOUR& GetRegionColourAtPosition(const float x, const float z);
	const DirectX::SimpleMath::Vector4& GetVoronoiRegionColourVector(const Enums::COLOUR& colour) const;
	const std::vector<VoronoiRegion>& GetVoronoiRegions() const { return m_voronoiRegions; }

	float GetHeightAt(float x, float z) const;
	int GetWidth() const { return m_terrainWidth; }
	int GetHeight() const { return m_terrainHeight; }

	const DirectX::SimpleMath::Vector3& GetRandomPosition() const;

	bool SmoothTerrain(ID3D11Device* device, float smoothFactor);
	bool GenerateFaultTerrain(ID3D11Device* device);
	bool GenerateParticleDepositionTerrain(ID3D11Device* device);

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
	float Grad(int hash, float x, float y);
	void GeneratePermutationTable();

	DirectX::SimpleMath::Vector4 GetColorByHeight(float height);
	float CalculateDistance(float x1, float y1, float x2, float y2) const;
	const Enums::COLOUR& GetRandomColour();
	void FillVoronoiRegionColours();
	const DirectX::SimpleMath::Vector3& GetRegionPosition(VoronoiRegion* region) const;
	const bool IsPointInRegion(int x, int z, VoronoiRegion* region) const;

private:
	bool m_terrainGeneratedToggle;
	int m_terrainWidth, m_terrainHeight;
	ID3D11Buffer * m_vertexBuffer, *m_indexBuffer;
	int m_vertexCount, m_indexCount;
	float m_frequency, m_amplitude, m_wavelength;
	HeightMapType* m_heightMap;
	
	float m_Scale = 0.0f;
	DirectX::SimpleMath::Vector3 m_Translation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);

	//arrays for our generated objects Made by directX
	std::vector<VertexPositionNormalTexture> preFabVertices;
	std::vector<uint16_t> preFabIndices;

	// Random number generator for random height map
	unsigned int m_randomSeed;

	// Noise generation parameters
	std::vector<int> m_permutation;

	std::vector<VoronoiRegion> m_voronoiRegions;
	std::map<Enums::COLOUR, DirectX::SimpleMath::Vector4> m_voronoiRegionColours;
	std::vector<Enums::COLOUR> m_randomVoronoiRegionColours;

	// Smoothing-related members
	std::vector<float> m_smoothedHeights;
	float m_smoothingIntensity = 0.5f;
};

