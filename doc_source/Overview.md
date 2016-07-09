Overview {#overview}
===========

**Voxels** is a library for manipulating and polygonizing (transforming into triangles) voxel grids in real-time applications. It can 
be used to implement *volume rendering*. It is geared for large meshes like terrains.

**Voxels** is both the name of the library and the plural of the word *voxel* which stands for
*volume element* and is a 3D version of the more commonly known *pixel*. **Voxels** is not a rendering library,
but the output polygon surface from it can be trivially rendered with any graphics API. Materials are supported 
for rich and interesting results.

## Major features
 - Polygonization based on the TransVoxel algorithm
 - Level of detail
 - Dynamic Material support
 - Dynamic manipulation
 - Voxel grid compression
 - Scalable implementation
 - Pluggable grid generation

## Voxels advantages
Voxel-based surfaces allow for more freedom than usual polygon-based ones. The major advantages of such a representation 
can be summarized as follows:

 - **Freedom of form** - You can create virtually any surface configuration. Gives much more possibilities than heightmaps for terrains
 for instance. With usual heightmaps you can't have caves or overhangs as part of the terrain mesh.
 - **Destructibility/constructibility** - The voxel surface can be trivially modified in real-time allowing easy sculpting. Allow users 
 to create their worlds or destroy them.
 - **Procedural generation** - Procedural surfaces can easily be created, which diminishes greatly the content creation effort and possibly 
 the memory footprint of the whole application

## Components

**Voxels** implements two important aspects related to *volume rendering*.
The surface to render is represented by a *voxel grid* - a 3D grid of elements with properties for each that 
approximate it.

It also implements a *polygonization* algorithm that creates triangles from the *voxel grid*. Those triangles are 
usually what gets sent to the graphics adapter and rendered to the client.
