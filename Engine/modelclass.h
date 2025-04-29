////////////////////////////////////////////////////////////////////////////////
// Filename: modelclass.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _MODELCLASS_H_
#define _MODELCLASS_H_


//////////////
// INCLUDES //
//////////////
#include "pch.h"
//#include <d3dx10math.h>
//#include <fstream>
//using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Class name: ModelClass
////////////////////////////////////////////////////////////////////////////////

using namespace DirectX;

class ModelClass
{
private:
	struct VertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector3 normal;
		DirectX::SimpleMath::Vector4 colour;
	};

public:
	ModelClass();
	~ModelClass();

	bool InitializeModel(ID3D11Device *device, char* filename, bool isColoured = false);
	bool InitializeTeapot(ID3D11Device*);
	bool InitializeSphere(ID3D11Device*);
	bool InitializeBox(ID3D11Device*, float xwidth, float yheight, float zdepth);
	void Shutdown();
	void Render(ID3D11DeviceContext*);
	
	int GetIndexCount();

	void SetScale(const DirectX::SimpleMath::Vector3& scale);
	const DirectX::SimpleMath::Vector3& GetScale() const;

	void SetPosition(const DirectX::SimpleMath::Vector3& position);
	const DirectX::SimpleMath::Vector3& GetPosition() const;

	void SetRotation(const DirectX::SimpleMath::Vector3& rotation);
	const DirectX::SimpleMath::Vector3& GetRotation() const;

	DirectX::SimpleMath::Matrix GetWorldMatrix() const;

	const DirectX::SimpleMath::Vector3& GetWorldPosition() const;

	void ChangeColour(ID3D11Device* device, const DirectX::SimpleMath::Vector4& colour);

private:
	bool InitializeBuffers(ID3D11Device*, const bool isColoured = false);
	void ShutdownBuffers();
	void RenderBuffers(ID3D11DeviceContext*);
	bool LoadModel(char*, bool isColoured = false);

	void ReleaseModel();

private:
	ID3D11Buffer *m_vertexBuffer, *m_indexBuffer;
	int m_vertexCount, m_indexCount;

	//arrays for our generated objects Made by directX
	std::vector<VertexPositionNormalTexture> preFabVertices;
	std::vector<VertexPositionNormalColorTexture> preFabColouredVertices;
	std::vector<uint16_t> preFabIndices;

	DirectX::SimpleMath::Vector3 m_scale = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 m_position = DirectX::SimpleMath::Vector3::Zero;
	DirectX::SimpleMath::Vector3 m_rotation = DirectX::SimpleMath::Vector3::Zero;

};

#endif