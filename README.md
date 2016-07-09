**Voxels** Library - Library for Voxel manipulation and polygonization

Official *Voxels* website: http://stoyannk.wordpress.com/voxels-library/

## General Notes

**Voxels** is a library for manipulating voxel grids and triangulating them to
triangle soups. The library supports grid compression and fast polygonization
using the TransVoxel algorithm.

**Voxels** is in *alpha*. Unfortunately I don't have enough time to dedicate it and at this point there are no plans to update the library. No significant changes were applied to the library since the first public release in 2014.

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
 
## Requirements
 - Windows 32-bit or 64-bit
 - SSE2-enabled processor
 - **Voxels** scales very well across processor cores; the more available on the machine - the faster the polygonization process will be.

The library is not strictly cross platform at the moment. I haven't tried it on other targets, but there shouldn't be hurdles porting it.
 
## Documentation

Detailed documentation is available in the doc_source folder.
 
## Samples

A sample application that uses **Voxels** is available on http://github.com/stoyannk/volumerendering. A pre-built version of the sample with 
editing functionality is available in the Download section.
 
 
## Acknowledgements

**Voxels** uses the TransVoxel algorithm as described in:
 Lengyel, Eric. “Voxel-Based Terrain for Real-Time Virtual Simulations”. PhD diss., University of California at Davis, 2010.
 For more information please refer to http://www.terathon.com/voxels/.
 **Voxels** uses the GLM maths library http://glm.g-truc.net/0.9.7/index.html
 
## License

The library is licensed under the permissive 3-clause BSD license. 
Please refer to the LICENSE file for all the details.
