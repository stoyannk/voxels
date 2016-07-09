Saving and Loading the Grid {#saveload}
===========

## Saving

The *Voxels::Grid* object can be saved via the *Voxels::Grid::PackForSave* method.
It returns a *Voxels::PackedGrid* object that contains a pointer to the data to serialize and their length. The 
*Voxels::PackedGrid* is heavily compressed version of the Grid that is at the same time optimized 
for later fast loading.
When you are done serializing your Grid you should *Destroy* the *Voxels::PackedGrid* object.

~~~~~~~~~~{.cpp}
bool Scene::SaveVoxelGrid(const std::string& filename)
{
	std::ofstream fout(filename.c_str(), std::ios::binary);

	if (!fout.is_open())
		return false;

	auto pack = m_Grid->PackForSave();

	fout.write(pack->GetData(), pack->GetSize());

	pack->Destroy();

	return true;
}
~~~~~~~~~~

## Loading

The loading process is essentially the same but in reverse. The *Voxels::Grid::Load* method method 
that takes a byte array and a size as parameters. An already packed grid should be passed there.

~~~~~~~~~~{.cpp}
bool Scene::LoadVoxelGrid(const std::string& filename)
{
	std::ifstream fin(filename.c_str(), std::ios::binary);
	if (!fin.is_open())
	{
		return false;
	}

	fin.seekg(0, std::ios::end);
	unsigned length = (unsigned)fin.tellg();
	fin.seekg(0, std::ios::beg);

	std::unique_ptr<char[]> data(new char[length]);
	fin.read(data.get(), length);

	m_Grid = Voxels::Grid::Load(data.get(), length);
			
	return true;
}
~~~~~~~~~~
