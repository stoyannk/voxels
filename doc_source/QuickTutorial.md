Quick tutorial {#quicktutorial}
===========

## Installation
**Voxels** for Windows is distributed a pre-built DLL, a stub .lib and include headers.

If building with Visual Studio:

 - Set **Voxels** in your include path. Right click on your Project,
 select Properties->Configuration Properties->C/C++->General. Add the **Voxels** *include* folder in 
 *Additional Include Directories*.
 - Set the .lib folder for the Linker. Right click on your Project,
 select Properties->Configuration Properties->Linker->General. Add the *lib* folder to *Additional Library Directories*.
 - Add *Voxels.lib* as Linker input. Right click on your Project,
 select Properties->Configuration Properties->Linker->Input. Add *Voxels.lib* in the *Additional Dependencies* field.
 - Don't forget to add the *Voxels.dll* to the folder where you output your executable or in a DLL search path.
 
## Library initialization/deinitialization

Include the library via the *Voxels.h* header. All **Voxels** types reside in the *Voxels* namespace.

To initialize the library call *InitializeVoxels*. To deinit the library call *DeinitializeVoxels*.
You *must* initialize the library before using any types/methods from it and should *never* continue using them 
after you have deinitialized it.

~~~~~~~~~~{.cpp}

#include <Voxels.h>

int main()
{
	if(InitializeVoxels(VOXELS_VERSION, nullptr, nullptr) != Voxels::IE_Ok) {
		return 1;
	}
	
	// .. do some work
	
	DeinitializeVoxels();
	
	return 0;
}

~~~~~~~~~~

**Voxels** supports setting custom memory handlers and logging function. You can set them in the *InitializeVoxels*
call.

## Creating a voxel grid

To create a voxel grid you can use any of the *Voxels::Grid::Create* methods. After you are done with the grid 
you must free its memory by calling *Voxels::Grid::Destroy*. The method will make sure de-allocations happen in the 
*Voxels.dll* and respect allocators. You must destroy all created **Voxels** objects before deinitializing the library.

You can easily create a procedural voxel grid by inheriting the *Voxels::VoxelSurface* interface and implementing it.
This is a sample implementation that creates a plane:

~~~~~~~~~~{.cpp}
namespace Voxels
{

class VoxelPlane : public VoxelSurface, public Aligned<16>
{
public:
	VoxelPlane(const DirectX::XMFLOAT3& normal, float d)
		: m_Plane(XMVectorSet(normal.x, normal.y, normal.z, d))
	{
		m_Plane = XMPlaneNormalize(m_Plane);
	}
	virtual ~VoxelPlane() {}
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
					const XMVECTOR point = XMVectorSet(x, y, z, 1);
					output[id++] = XMVectorGetX(XMPlaneDot(m_Plane, point));
				}
			}
		}
	}
private:
	DirectX::XMVECTOR m_Plane;
};

}

// than you use the VoxelPlane class to create your Grid

{
	// ..
	auto plane = new Voxels::VoxelPlane(XMFLOAT3(0.0f, 0.0f, 1.0f), 5.0f);
	
	// Creates a 64x64x64 grid spanning from 0 to 64 in all directions
	// with a step of 1
	auto grid = Voxels::Grid::Create(64, 64, 64, 0, 0, 0, 1, plane);
	
	delete plane;
}
~~~~~~~~~~

**Note:** Currently only cube grids are supported.

**Note:** Free versions of the library are limited to 128x128x128 grids.

## Polygonizing the grid

To render the surface we have to transform it in a set of triangles. In **Voxels** this is done with the *Voxels::Polygonizer* class.

~~~~~~~~~~{.cpp}

auto polygonizer = new Voxels::Polygonizer;

Voxels::PolygonSurface* surface = polygonizer->Execute(grid, materialMap);

// .. use the triangles

surface->Destroy();

delete polygonizer;

~~~~~~~~~~

The second parameter is a *MaterialMap*. Check the *Material* page of this manual for more information on how to set it.

The output of the *Execute* method is a *Voxels::PolygonSurface* object that represents a collection of the produced vertices and indices 
of the surface. The surface vertices/indices are returned in collections of LOD levels. Each LOD level contains a collection of *blocks*. Each block 
is a part of the surface. The reason to have many blocks is to aid culling during rendering and to be able to show parts of the surface in different 
LOD levels.

The *Polygonizer* is multi-threaded during execution and scales very well. All the cores of the machine will be used during the 
polygonization process.

**Note:** The *Voxels::PolygonSurface* object contains a cache bound to the Grid it was created from that is used for faster 
modifications if needed. This cache might use substantial amounts of memory - depending on the size of the grid. If you don't 
intend to modify the grid at a later point in the program you can safely *Destroy* the object after you've uploaded the vertices and indices 
to the GPU.

For more details on the described operations please check the included samples.
