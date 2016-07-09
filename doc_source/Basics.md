Basics {#basics}
===========

## License

Please refer to the LICENSE file and the *License* page in this documentation for information about licensing.

You can use *Voxels* for both commercial and non-commercial applications but you should clearly state 
that you use the library. The Free versions of the library are artificially limited in the size of the Grid they support - 
128x128x128. For unlimited versions please contact me. You will receive the unlimited version but I would like to 
know who is using it actively as feedback is vital for enhancing **Voxels** in the future.

## Surface representation

All surfaces in the library are kept in a *voxel grid* - this is a 3D grid of volume elements. Each voxel in the library holds
three values:

- **distance value** - The value of the distance function in this voxel. This is the distance to the surface from this voxel. 
Note that voxels have implicit positions denoted by their position in the grid.
- **material id** - An Id of a material in the position of the voxel. This id is later mapped to textures used during shading.
- **blend factor** - Used to blend two materials together so that smooth transitions are created in regions of the surface.

Values are kept compressed by the library in the grid to save memory. If no compression is applied the memory footprint would 
be too big. For instance for a 512x512x512 grid with one byte for each value, we would need ~400 MB of storage.
Volume data however usually changes slowly in space and **Voxels** takes advantage of this and achieves extremely high compression 
ratios (usually 30x+ compression, depending on input).

The grid can be modified very fast - new values can be injected in it effectively morphing the underlying surface geometry or its materials.

**Note:** In *Grid* coordinates the *z* component is the *up* direction - the one linked to the *height* value.

## Polygonization

The voxel grid is very convenient for storage and modifications but not suitable for rendering. Although there are algorithms that allow to 
directly render voxels, they are currently unsuitable for real-time applications. Modern GPUs work best with polygons and engines and other libraries 
usually expect polygons to work with.

**Voxels** can transform the grid in a set of polygons that can directly be rendered with any graphic API like DirectX or OpenGL. It uses the 
TransVoxel algorithm invented by Eric Lengyel. The algorithm allows very high performance polygonization, great scalability and LOD generation.

The library is aimed to support very large meshes like virtual terrains. With so large surfaces some kind of level of detail solution must be implemented 
in order to save GPU resources. **Voxels** generates such LODs for the surface. In the sample applications there are examples on how to use those 
LODs for effective rendering of the mesh.

**Note:** The Polygonizer produces vertices with the *y* component as *up*, which is usually the norm for most realtime graphics applications.

For more details on the TransVoxel algorithm please visit its official site: http://www.terathon.com/voxels/. 
