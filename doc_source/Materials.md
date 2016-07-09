Materials {#materials}
===========

**Voxels** allows setting different materials to parts of the surface it polygonizes. Each voxel has two properties associated with the material - 
a material id (one byte - up to 255 ids for the surface) and a blend id.

## Material Map

The material id associated with each voxel has to be interpreted somehow and **Voxels** transforms it in a set of 6 textures. A triplet of textures 
represents one material. Three textures are used because UV coordinates can't be generated for the volume surface and hence the mapping of the 
texture on the triangles becomes problematic. **Voxels** encourages the use of *Triplanar texture projection* during rendering. In essence 
*triplanar projection* uses the *normal* of the surface to select a texel among 3 textures. One textures is used for the *top* of the surface, 
one for all its *sides* and one for the *bottom* facing parts.

**Voxels** generates two sets of textures for each material id in order to have smooth transitions between materials. The *blend* parameter 
of each voxel is governs how much to take from the first set of textures and how much from the second. An application that creates or edits the voxel grid 
must take care to generate smooth varying values for the *blend* parameter, otherwise abrupt transitions might appear.

For each material id only two sets of textures get generated which means that you can't have a voxel where > 2 distinct materials meet or an abrupt texture
change will happen there. This is rarely a problem for virtual terrains for which **Voxels** is currently designed. During editing care should be taken 
to ensure that materials are properly generated to accommodate the different combinations of materials that meet each other.

The mapping from material id to texture id must be accomplished by the application because the texture management is done in it. The application 
must implement the *Voxels::MaterialMap* class and pass an objects of that type to the *Voxels::Polygonizer::Execute* method. This object 
must map material ids passed by **Voxels** to texture ids that have a meaning for the rendering system and can later be used when drawing the 
surface.

## Rendering

Surfaces created by **Voxels** should be textured via *triplanar projection*. If the rendering API supports texture arrays, selecting which texture to use is 
relatively simple.

The texture ids required for each vertex are encoded in the *Textures* property of the *Voxels::PolygonVertex* structure.

~~~~~~~~~~{.cpp}
/// Packed texture indices used to select
	/// a textures for blending on this vertex
	union {
		struct {
			unsigned char Reserved;
			unsigned char Blend;
			unsigned char Uxz;
			unsigned char Txz;

			unsigned char Uny;
			unsigned char Upy;
			unsigned char Tny;
			unsigned char Tpy;
		} TextureIndices;

		unsigned TI[2];
	} Textures;
~~~~~~~~~~

The texture triplets here are called T and U. Txz and Uxz are the ids of the textures for the sides of the surface. The Tny and Uny are the *bottom* texture ids 
while the Tpy and Upy the *top* ones.
The *Blend* parameter tells how to mix the two sets of textures.

For shader code that performs drawing with materials please consult the examples accompanying **Voxels**.

If your target hardware doesn't support texture arrays than you should resort to texture atlases. Extensive detail on the technique can be found in Eric Lengyel's Thesis 
about the TransVoxel algorithm on http://www.terathon.com/voxels/.
