Surface modification {#modification}
===========

**Voxels** supports efficient ways to modify the Grid and hence the surface in real-time. The modification procedures 
are very fast, so that lag is not perceived by the user between the time she modifies the surface and sees the result.

## Changing the grid

The first step of the modification is changing the *Grid*. There are three ways to do this. The *Grid* is virtually 
subdivided in blocks 16x16x16 voxels each. These blocks are kept compressed in memory but are modifiable by the user.
The *Voxels::Grid::GetBlockDistanceData*, *Voxels::Grid::ModifyBlockDistanceData*, *Voxels::Grid::GetBlockMaterialData*,
*Voxels::Grid::ModifyBlockMaterialData* methods allow reading and changing the values of whole blocks.

Two more convenience methods are provided to change the grid: *Voxels::Grid::InjectSurface* and *Voxels::Grid::InjectMaterial*.
As their names imply, they allow the user to inject in the Grid a surface (an implementer of the *Voxels::VoxelSurface* interface) and 
a material in a specified position in the grid with specified extents. The returned pair of float triplets represent 
the extents of the modified region inside the grid.

**Note:** In *Grid* coordinates the *z* component is the *up* direction - the one linked to the *height* value.

## Polygonizing the change

Re-polygonizing the whole surface when just some part of it has changed is overkill. **Voxels** supports 
computing just parts the of the surface that have changed. This makes the modification process extremely fast 
and suitable for real-time.

The *Voxels::Polygonizer::Execute* can take as last parameter a pointer to a *Voxels::Modification* object that 
encapsulates information about exactly what part of the grid has changed and which surface we are modifying.

~~~~~~~~~~{.cpp}
Voxels::Modification* modification = Voxels::Modification::Create();
if(modified) {
	modification->Map = m_PolygonSurface;
	modification->MinCornerModified = modified->first;
	modification->MaxCornerModified = modified->second;
}
m_PolygonSurface = m_Polygonizer->Execute(*m_Grid, &m_Materials, modification);

modification->Destroy();
~~~~~~~~~~

The *Voxels::PolygonSurface* object contains a cache that aids modification performance. If you intend to modify a grid you should 
keep it around and pass it back to the *Polygonizer* upon modification. After the procedure has completed you'll get an
updated *Voxels::PolygonSurface* object.

**Note:** The Polygonizer produces vertices with the *y* component as *up*, which is usually the norm for most realtime graphics applications.

After a modification some block might change, others might disappear and new ones get created. The application should 
keep track of the previous block ids in each level and diff this collection with the new state of the *Voxels::PolygonSurface* object.
Each block has a unique id that can be used to identify it. GPU data related to blocks
that don't appear in the new *Voxels::PolygonSurface* object should be deleted. Newly created ones should be uploaded to the GPU.
A function that performs this operation is *DrawRoutine::UpdateGrid* found in the *VolumeRendering* sample and can be used as reference.
