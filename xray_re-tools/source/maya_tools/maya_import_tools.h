#pragma once
#ifndef __MAYA_IMPORT_TOOLS_H__
#define __MAYA_IMPORT_TOOLS_H__

#include <string>
#include <vector>
#ifdef _MSC_VER
#if _MSC_VER<1900
#include <hash_map>
#else
#include <unordered_map>
#endif
#else
#include <map>
#endif
#include <maya/MStatus.h>

namespace xray_re {
	class xr_object;
	class xr_bone;
	class xr_mesh;
	class xr_skl_motion;
	class xr_surface;
};

class MObject;

#ifdef _MSC_VER
#if _MSC_VER<1900
typedef stdext::hash_map<std::string, MObject> maya_object_map;
#else
typedef std::unordered_map<std::string, MObject> maya_object_map;
#endif
#else
typedef std::map<std::string, MObject> maya_object_map;
#endif
typedef maya_object_map::iterator maya_object_map_it;
typedef std::pair<std::string, MObject> maya_object_pair;

class maya_import_tools {
public:
			maya_import_tools();
			~maya_import_tools();
			maya_import_tools(const xray_re::xr_object* object, MStatus* return_status = 0);

	MStatus		import_object(const xray_re::xr_object* object);
	MStatus		import_motion(const xray_re::xr_skl_motion* motion, MObject& character_obj);
	MStatus		import_motions(const std::vector<xray_re::xr_skl_motion*>& motions, MObject& character_obj);

	void		reset_animation_state() const;
	MObject		lookup_character(MStatus* return_status = 0);

private:
	MObject		create_character(MStatus* return_status = 0);
	MStatus		import_surface(const xray_re::xr_surface* surface, MObject& texture_obj);
	MStatus		import_bone(const xray_re::xr_bone* bone, MObject& parent_obj);
	MStatus		import_mesh(const xray_re::xr_mesh* mesh, const std::vector<xray_re::xr_bone*>& bones);

private:
	maya_object_map	m_sets;
	maya_object_map	m_joints;
};

inline maya_import_tools::maya_import_tools() {}

#endif
