**Voxels** Library - Library for Voxel manipulation and polygonization

Official *Voxels* website: http://stoyannk.wordpress.com/voxels-library/

## General Notes

**Voxels** is a library for manipulating voxel grids and triangulating them to
triangle soups. The library supports grid compression and fast polygonization
using the TransVoxel algorithm.

**Voxels** is still in *alpha*. I plan to extend the library to support more platforms and features. Feedback is highly 
appreciated.

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

## Documentation

Detailed documentation is available in the library packet.
 
## Samples

A sample application that uses **Voxels** is available on http://github.com/stoyannk/volumerendering. A pre-built version of the sample with 
editing functionality is available in the Download section.
 
## Limitations

This Free version of **Voxels** has a grid limit of 128x128x128 voxels. If you need and unlimited version - please write to me directly and I'll send it.
I would like to know the developers using the library as feedback is extremely important for the future of the library.
 
## Acknowledgements

**Voxels** uses the TransVoxel algorithm as described in:
 Lengyel, Eric. “Voxel-Based Terrain for Real-Time Virtual Simulations”. PhD diss., University of California at Davis, 2010.
 For more information please refer to http://www.terathon.com/voxels/.
 
## License

You can use *Voxels* for both commercial and non-commercial applications but you should clearly state 
that you use the library. The Free versions of the library are artificially limited in the size of the Grid they support - 
128x128x128. For unlimited versions please contact me. You will receive the unlimited version but I would like to 
know who is using it actively as feedback is vital for enhancing **Voxels** in the future.

Please refer to the LICENSE file for all the details.
