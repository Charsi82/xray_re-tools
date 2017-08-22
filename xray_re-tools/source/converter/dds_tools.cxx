#define NOMINMAX
#include <windows.h>
#include <cstring>
#include "dds_tools.h"
#include "xr_image.h"
#include "xr_texture_thumbnail.h"
#include "xr_file_system.h"

using namespace xray_re;

dds_tools::dds_tools(): m_with_solid(false) {}

bool dds_tools::check_paths() const
{
	bool status = true;
	check_path(PA_GAME_TEXTURES, status);
	check_path(PA_TEXTURES, status);
	return status;
}

/*void dds_tools::process_file(const std::string& path)
{
	std::string::size_type offs = path.size();
	if (offs < 4 || path.compare(offs -= 4, 4, ".thm") != 0)
		return;

	xr_texture_thumbnail thm;
	if (!thm.load(PA_GAME_TEXTURES, path.c_str())) {
		msg("can't load %s", path.c_str());
		return;
	}
	if (path.compare(0, 4, "lod\\") == 0) {
		msg("skipping %s (billboard)", path.c_str());
		return;
	}
	switch (thm.type) {
	case xr_texture_thumbnail::tt_image:
		if (m_with_solid || thm.has_alpha_channel() ||
				thm.is_implicitly_lighted()) {
			break;
		}
		msg("skipping %s (solid)", path.c_str());
		return;
	case xr_texture_thumbnail::tt_cube_map:
		msg("skipping %s (cube map)", path.c_str());
		return;
	case xr_texture_thumbnail::tt_bump_map:
		if (m_with_bump)
			break;
		msg("skipping %s (bump map)", path.c_str());
		return;
	case xr_texture_thumbnail::tt_normal_map:
	case xr_texture_thumbnail::tt_terrain:
		break;
	default:
		msg("skipping %s (unknown) %#x", path.c_str(), thm.type);
		return;
	}

	xr_file_system& fs = xr_file_system::instance();
	std::string tga_path(path);
	tga_path.replace(offs, 4, ".tga");
	std::string dest; fs.resolve_path(PA_TEXTURES, tga_path, dest);
	if (fs.file_exist(dest)) {
		msg("skipping %s (already exists)", path.c_str());
		return;
	}

	std::string folder; fs.split_path(dest, &folder, NULL, NULL);
	fs.create_path(folder);

	std::string dds_path(path);
	dds_path.replace(offs, 4, ".dds");
	xr_image dds;
	if (!dds.load_dds(PA_GAME_TEXTURES, dds_path.c_str())) {
		msg("can't load %s", dds_path.c_str());
		return;
	}
	msg("saving %s", tga_path.c_str());
	dds.save_tga(dest);
}*/

void dds_tools::process_file(const std::string& path)
{
	//skip no dds
	std::string::size_type offs = path.size();
	if (offs < 4 || path.compare(offs -= 4, 4, ".dds") != 0)
		return;

	//skip lod
	if (path.compare(0, 4, "lod\\") == 0) {
		msg("skipping %s (billboard)", path.c_str());
		return;
	}

	//skip sky
	if (path.compare(0, 4, "sky\\") == 0) {
		msg("skipping %s (skybox)", path.c_str());
		return;
	}

	//skip exists
	xr_file_system& fs = xr_file_system::instance();
	std::string tga_path(path);
	tga_path.replace(offs, 4, ".tga");
	std::string dest;
	fs.resolve_path(PA_TEXTURES, tga_path, dest);
	if (fs.file_exist(dest)) {
		msg("skipping %s (already exists)", path.c_str());
		return;
	}
	if (m_tga_import)
	{
		std::string name;
		xr_file_system::split_path(tga_path, 0, &name);
		fs.resolve_path(PA_IMPORT, name + ".tga", dest);
		if (fs.file_exist(dest)) {
			msg("skipping %s (already exists)", path.c_str());
			return;
		}

		//skip bump#
		if (path.find("bump#") != std::string::npos) {
			msg("skipping %s (bump#)", path.c_str());
			return;
		}

		//skip bump
		if (!m_with_bump && path.find("bump") != std::string::npos) {
			msg("skipping %s (bump)", path.c_str());
			return;
		}
	}
	//find thm 
	xr_texture_thumbnail thm;
	std::string thm_path(path);
	thm_path.replace(offs, 4, ".thm");
	std::string tmp;
	if (fs.resolve_path(PA_GAME_TEXTURES, thm_path, tmp) && fs.file_exist(tmp) && thm.load(PA_GAME_TEXTURES, thm_path.c_str()) || //in game_textures
		fs.resolve_path(PA_TEXTURES, thm_path, tmp) && fs.file_exist(tmp) && thm.load(PA_TEXTURES, thm_path.c_str())) //in raw_textures
	{
		switch (thm.type) {
		case xr_texture_thumbnail::tt_image:
			if (m_with_solid || thm.has_alpha_channel() ||
				thm.is_implicitly_lighted()) {
				break;
			}
			msg("skipping %s (solid)", path.c_str());
			return;
		case xr_texture_thumbnail::tt_cube_map:
			msg("skipping %s (cube map)", path.c_str());
			return;
		case xr_texture_thumbnail::tt_bump_map:
			if (m_with_bump)
				break;
			msg("skipping %s (bump map)", path.c_str());
			return;
		case xr_texture_thumbnail::tt_normal_map:
		case xr_texture_thumbnail::tt_terrain:
			break;
		default:
			msg("skipping %s (unknown) %#x", path.c_str(), thm.type);
			return;
		}
	}

	fs.split_path(dest, &tmp, NULL, NULL);
	fs.create_path(tmp);

	xr_image dds;
	if (!dds.load_dds(PA_GAME_TEXTURES, path.c_str())) {
		msg("can't load %s", path.c_str());
		return;
	}
	msg("saving %s", tga_path.c_str());
	dds.save_tga(dest);
}

void dds_tools::process_folder(const std::string& path)
{
	WIN32_FIND_DATAA info;
	HANDLE h = FindFirstFileA((m_textures + path + '*').c_str(), &info);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do {
		if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (std::strcmp(info.cFileName, ".") != 0 && std::strcmp(info.cFileName, "..") != 0)
				process_folder((path + info.cFileName) + '\\');
		} else {
			process_file(path + info.cFileName);
		}
	} while (FindNextFileA(h, &info));
	FindClose(h);
}

void dds_tools::process(const cl_parser& cl)
{
	if (!check_paths())
		return;

	m_with_solid = cl.get_bool("-with_solid");
	m_with_bump = cl.get_bool("-with_bump");
	m_tga_import = cl.get_bool("-tga_import");

	xr_file_system& fs = xr_file_system::instance();
	m_textures = fs.resolve_path(PA_GAME_TEXTURES);

	process_folder();
}
