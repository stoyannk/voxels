Procedural Generation {#procedural}
===========

## Generating procedural grids

One of the major benefits of a volume representation for a surface is that it's pretty easy to generate 
procedural content.

**Voxels** supports procedurally generated grids via the *Voxels::VoxelSurface* interface. You can pass 
an object implementing the interface to the *Voxels::Grid::Create* method. During Grid construction the object's 
*GetSurfaceDist* method will be called for every block of the surface and it must provide the distance values 
for the required coordinates.

Notice that distance values are required as *float* numbers. They are later compressed and truncated as 
necessary by the library itself.

An example that generates a ball is given below:

~~~~~~~~~~{.cpp}
__declspec(align(16))
class VoxelBall : public VoxelSurface, public Aligned<16>
{
public:
	VoxelBall(const DirectX::XMFLOAT3& position, float radius)
		: m_Ball(XMVectorSet(position.x, position.y, position.z, radius))
	{}
	
	virtual void GetSurfaceDist(float xStart, float xEnd, float xStep,
						float yStart, float yEnd, float yStep,
						float zStart, float zEnd, float zStep,
						float* output) override
	{
		auto id = 0;
		for (auto z = zStart; z < zEnd; z += zStep)
		{
			for (auto y = yStart; y < yEnd; y += yStep)
			{
				for (auto x = xStart; x < xEnd; x += xStep)
				{
					const XMVECTOR vec = XMVectorSubtract(m_Ball, XMVectorSet(x, y, z, 0));
					const XMVECTOR dist = XMVector3Length(vec);

					output[id] = XMVectorGetW(XMVectorSubtract(dist, m_Ball));
					if (materialid != nullptr)
					{
						materialid[id] = 0;
						blend[id] = 0;
					}
					++id;
				}
			}
		}
	}
private:
	DirectX::XMVECTOR m_Ball;
};
~~~~~~~~~~

Additional examples are given in the samples accompanying the library.
