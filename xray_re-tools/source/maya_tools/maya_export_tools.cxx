#define NOMINMAX
#include <maya/MAnimControl.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDistance.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include "maya_export_tools.h"
#include "xr_object.h"
#include "xr_skl_motion.h"
#include "xr_envelope.h"
#include "xr_utils.h"

using namespace xray_re;

static MStatus extract_bones(MFnSkinCluster& skin_fn, xr_bone_vec& bones)
{
	MStatus status;

	MDagPathArray joints;
	skin_fn.influenceObjects(joints, &status);
	unsigned num_joints = joints.length();
	if (num_joints == 0) {
		msg("xray_re: can't find any influence object");
		MGlobal::displayError("xray_re: can't find any influence object");
		return MS::kInvalidParameter;
	} else if (num_joints > MAX_BONES) {
		msg("xray_re: too many joints (%u of %u possible)", num_joints, MAX_BONES);
		MGlobal::displayError(MString("xray_re: too many joints ") +
			"(" + num_joints + " of " + MAX_BONES + " possible)");
	}

	bones.resize(num_joints);

	MString command("dagPose -r -g -bp ");
	for (unsigned i = num_joints; i != 0;) {
		MFnIkJoint joint_fn(joints[--i], &status);
		if (!status) {
			msg("xray_re: can't handle non-joint node %s",
				joints[i].partialPathName().asChar());
			MGlobal::displayError(MString("xray_re: can't handle non-joint node ") +
				joints[i].partialPathName().asChar());
			return status;
		}
		command += joint_fn.partialPathName();
		command += " ";

		xr_bone* bone = new xr_bone;
		bones[i] = bone;
		const char* name = joint_fn.name().asChar();
		bone->vmap_name() = bone->name() = name;

		unsigned num_parents = joint_fn.parentCount();
		if (num_parents > 1) {
			msg("xray_re: can't handle multi-parented joint %s", name);
			MGlobal::displayError(MString("xray_re: can't handle multi-parented joint ") + name);
			return MS::kInvalidParameter;
		} else if (num_parents == 1) {
			MObject parent_obj = joint_fn.parent(0);
			if (parent_obj.hasFn(MFn::kJoint) &&
					joint_fn.setObject(parent_obj)) {
				bone->parent_name() = joint_fn.name().asChar();
			}
		}
	}

	xr_bone* root = 0;
	for (xr_bone_vec_it it = bones.begin(), end = bones.end(); it != end; ++it) {
		xr_bone* bone = *it;
		xr_assert(bone);
		if (bone->parent_name().empty()) {
			if (root == 0) {
				root = bone;
				continue;
			} else {
				msg("xray_re: can't handle multiple root joints in skeleton");
				MGlobal::displayError("xray_re: can't handle multiple root joints in skeleton");
				return MS::kInvalidParameter;
			}
		}
		xr_bone* parent = find_by_name(bones, bone->parent_name());
		if (parent == 0) {
			msg("xray_re: can't find parent bone %s", bone->parent_name().c_str());
			MGlobal::displayError(MString("xray_re: can't find parent bone ") +
				bone->parent_name().c_str());
			return MS::kFailure;
		}
		parent->children().push_back(bone);
	}
	if (root == 0) {
		msg("xray_re: can't find root joint");
		MGlobal::displayError("xray_re: can't find root joint");
		return MS::kInvalidParameter;
	}

	if (!(status = MGlobal::executeCommand(command))) {
		msg("xray_re: can't set skeleton to bind pose");
		MGlobal::displayError("xray_re: can't set skeleton to bind pose");
		return MS::kFailure;
	}

	for (unsigned i = num_joints; i != 0;) {
		MFnIkJoint joint_fn(joints[--i]);
		xr_bone* bone = bones[i];

		MVector t = joint_fn.getTranslation(MSpace::kTransform, &status);
		CHECK_MSTATUS(status);
		bone->bind_offset().set(float(MDistance(t.x, MDistance::kCentimeters).asMeters()),
				float(MDistance(t.y, MDistance::kCentimeters).asMeters()),
				float(MDistance(-t.z, MDistance::kCentimeters).asMeters()));

		MEulerRotation r;
		status = joint_fn.getRotation(r);
		CHECK_MSTATUS(status);
		r.reorderIt(MEulerRotation::kZXY);
		bone->bind_rotate().set(float(-r.x), float(-r.y), float(r.z));
	}

	return status;
}

static MStatus extract_points(MFnMesh& mesh_fn, std::vector<fvector3>& points, fbox& aabb)
{
	MStatus status;

	int num_points = mesh_fn.numVertices();
	if (num_points < 4) {
		msg("xray_re: can't export mesh %s with less than four vertices",
			mesh_fn.name().asChar());
		MGlobal::displayError(MString("xray_re: can't export mesh ") +
			mesh_fn.name().asChar() + " with less than four vertices");
		return MS::kInvalidParameter;
	}
	points.reserve(size_t(num_points & INT_MAX));
	aabb.invalidate();
	MPoint p0;
	fvector3 p;
	for (int i = 0; i != num_points; ++i) {
		mesh_fn.getPoint(i, p0);
		p0.cartesianize();
		aabb.extend(p.set(float(MDistance(p0.x, MDistance::kCentimeters).asMeters()),
				float(MDistance(p0.y, MDistance::kCentimeters).asMeters()),
				float(MDistance(-p0.z, MDistance::kCentimeters).asMeters())));
		points.push_back(p);
	}
	return status;
}

static MStatus extract_faces(MFnMesh& mesh_fn, lw_face_vec& faces)
{
	MStatus status;

	MItMeshPolygon it(mesh_fn.object());
	if (it.isLamina()) {
		msg("xray_re: lamina faces found in mesh %s", mesh_fn.name().asChar());
		MGlobal::displayWarning(MString("xray_re: lamina faces found in mesh ") +
			mesh_fn.name().asChar());
	}
	// FIXME: it would be nice to support automatic triangulation using getTriangles() etc.
	int num_polys = mesh_fn.numPolygons(&status);
	if (num_polys < 2) {
		msg("xray_re: can't export mesh %s with less than two faces",
			mesh_fn.name().asChar());
		MGlobal::displayError(MString("xray_re: can't export mesh ") +
			mesh_fn.name().asChar() + " with less than two faces");
		return MS::kInvalidParameter;
	}
	faces.reserve(size_t(num_polys & INT_MAX));
	MIntArray verts;
	for (int i = 0; i != num_polys; ++i) {
		status = mesh_fn.getPolygonVertices(i, verts);
		if (!status || verts.length() != 3) {
			msg("xray_re: can't handle polygons with 4 or more sides for mesh %s",
				mesh_fn.name().asChar());
			MGlobal::displayError(MString("xray_re: can't handle polygons with 4 or more sides for mesh ") +
				mesh_fn.name().asChar());
			return MS::kInvalidParameter;
		}
		lw_face face(verts[2], verts[1], verts[0]);
		faces.push_back(face);
	}
	return status;
}

static MStatus extract_uvs(MFnMesh& mesh_fn, lw_face_vec& faces,
		lw_vmref_vec& vmrefs, xr_vmap_vec& vmaps)
{
	MStatus status;

	xr_uv_vmap* uv_vmap = 0;
	xr_face_uv_vmap* face_uv_vmap = 0;

	for (MItMeshVertex it(mesh_fn.object()); !it.isDone(); it.next()) {
		uint32_t vert_idx = uint32_t(it.index() & INT_MAX);

		fvector2 uv0;
		if (!it.getUV(uv0.xy)) {
			msg("xray_re: can't extract shared UVs for vert %" PRIu32 " on mesh %s",
				vert_idx, mesh_fn.name().asChar());
			MGlobal::displayError(MString("xray_re: can't extract shared UVs for vert ") +
				vert_idx + " on mesh " + mesh_fn.name().asChar());
			return MS::kInvalidParameter;
		}
		uv0.v = 1.f - uv0.v;

		if (uv_vmap == 0) {
			uv_vmap = new xr_uv_vmap("Texture");
			uv_vmap->reserve(size_t(mesh_fn.numVertices() & INT_MAX));
			vmaps.push_back(uv_vmap);
		}
		lw_vmref vmref0;
		vmref0.push_back(lw_vmref_entry(0, uv_vmap->add_uv(uv0, vert_idx)));

		uint32_t vmref0_idx = uint32_t(vmrefs.size() & UINT32_MAX);
		vmrefs.push_back(vmref0);

		MIntArray adjacents;
		it.getConnectedFaces(adjacents);
		for (unsigned i = adjacents.length(); i != 0;) {
			uint32_t face_idx = uint32_t(adjacents[--i] & INT_MAX), vmref_idx;

			fvector2 uv;
			if (!it.getUV(face_idx, uv.xy)) {
				msg("xray_re: can't extract UVs for vert %" PRIu32 " face %" PRIu32, vert_idx, face_idx);
				MGlobal::displayWarning(MString("xray_re: can't extract UVs for vert ") +
					vert_idx + " face " + face_idx);
				uv = uv0;
			}
			uv.v = 1.f - uv.v;

			lw_face& face = faces[face_idx];
			if (uv == uv0) {
				vmref_idx = vmref0_idx;
			} else {
				if (face_uv_vmap == 0) {
					face_uv_vmap = new xr_face_uv_vmap("Texture");
					vmaps.push_back(face_uv_vmap);
				}
				lw_vmref vmref;
				vmref.push_back(lw_vmref_entry(1, face_uv_vmap->add_uv(uv, vert_idx, face_idx)));
				vmref_idx = uint32_t(vmrefs.size() & UINT32_MAX);
				vmrefs.push_back(vmref);
			}
			for (uint_fast32_t j = 3; j != 0;) {
				if (face.v[--j] == vert_idx) {
					face.ref[j] = vmref_idx;
					vmref_idx = UINT32_MAX;
					break;
				}
			}
			xr_assert(vmref_idx == UINT32_MAX);
		}
	}
	return status;
}

static MStatus extract_weights(MFnMesh& mesh_fn, MFnSkinCluster& skin_fn,
		lw_face_vec& faces, lw_vmref_vec& vmrefs, xr_vmap_vec& vmaps)
{
	MStatus status;

	MDagPathArray joints;
	skin_fn.influenceObjects(joints, &status);
	CHECK_MSTATUS(status);

	// collect bone weights in vmaps and build vertex-ordered refs
	lw_vmref_vec weight_vmrefs(size_t(mesh_fn.numVertices() & INT_MAX));
	for (unsigned joint_idx = joints.length(); joint_idx != 0;) {
		MDoubleArray weights;
		MSelectionList affected;
		status = skin_fn.getPointsAffectedByInfluence(joints[--joint_idx], affected, weights);
		CHECK_MSTATUS(status);
		if (affected.isEmpty())
			continue;

		MFnIkJoint joint_fn(joints[joint_idx], &status);
		CHECK_MSTATUS(status);

		msg("xray_re: joint=%s", joint_fn.name().asChar());
		MGlobal::displayInfo(MString("xray_re: joint=") + joint_fn.name().asChar());

		xr_weight_vmap* weight_vmap = new xr_weight_vmap(joint_fn.name().asChar());
		weight_vmap->reserve(weights.length());
		uint32_t vmap_idx = uint32_t(vmaps.size() & UINT32_MAX);
		vmaps.push_back(weight_vmap);

		msg("         num_affected=%u, num_weights=%u", affected.length(), weights.length());
		MGlobal::displayInfo(MString("         num_affected=") + affected.length() +
			" ," + " num_weights=" + weights.length());

		// FIXME: is it enough to expect the single element in the list here?
		for (unsigned i = affected.length(), k = weights.length(); i != 0;) {
			MDagPath dag_path;
			MObject component_obj;
			status = affected.getDagPath(--i, dag_path, component_obj);
			CHECK_MSTATUS(status);
			MFnSingleIndexedComponent component_fn(component_obj, &status);
			CHECK_MSTATUS(status);
			for (int j = component_fn.elementCount() - 1; j >= 0; --j) {
				int vert_idx = component_fn.element(j, &status);
				CHECK_MSTATUS(status);
				uint32_t weight_idx = weight_vmap->add_weight(float(weights[--k]), uint32_t(vert_idx & INT_MAX));
				xr_assert(!weight_vmrefs[vert_idx].full());
				weight_vmrefs[vert_idx].push_back(lw_vmref_entry(vmap_idx, weight_idx));
			}
		}
	}

	// merge weight vmrefs with the already extracted uv vmrefs
	for (lw_face_vec_it it = faces.begin(), end = faces.end(); it != end; ++it) {
		for (uint_fast32_t i = 3; i != 0;) {
			lw_vmref& vmref = vmrefs[it->ref[--i]];
			if (vmref.size() > 1)
				continue;
			vmref.append(weight_vmrefs[it->v[i]]);
		}
	}

	return status;
}

static void get_xraymtl_attr(MFnDependencyNode& dep_fn, const char* name, std::string& value)
{
	MStatus status;
	MPlug plug = dep_fn.findPlug(name, &status);
	if (status) {
		MFnEnumAttribute attr_fn(plug.attribute(&status), &status);
		if (status) {
			MString temp = attr_fn.fieldName(plug.asShort(), &status);
			if (status) {
				value = temp.asChar();
				for (std::string::size_type i = 0; (i = value.find('/', i)) != std::string::npos; ++i)
					value[i] = '\\';
				return;
			}
		}
	}
	msg("xray_re: can't get attribute %s", name);
	MGlobal::displayWarning(MString("xray_re: can't get attribute ") + name);
}

xr_surface* maya_export_tools::create_surface(const char* surf_name, MFnSet& set_fn)
{
	xr_surface* surface = new xr_surface(m_skeletal);
	surface->name() = surf_name;

	MStatus status;
	MPlugArray connected_plugs;
	set_fn.findPlug("ss").connectedTo(connected_plugs, true, false, &status);
	MObject shader_obj;
	for (unsigned i = connected_plugs.length(); i != 0;) {
		MObject obj = connected_plugs[--i].node();
		MFnDependencyNode dep_fn(obj);
		if (dep_fn.typeName() == "XRayMtl") {
			shader_obj = obj;
			get_xraymtl_attr(dep_fn, "xrayGameMaterial", surface->gamemtl());
			get_xraymtl_attr(dep_fn, "xrayEngineShader", surface->eshader());
			get_xraymtl_attr(dep_fn, "xrayCompilerShader", surface->cshader());
			MPlug plug = dep_fn.findPlug("xrayDoubleSide", &status);
			if (status && plug.asBool())
				surface->set_two_sided();
			break;
		} else if (obj.hasFn(MFn::kLambert) || obj.hasFn(MFn::kPhong) || obj.hasFn(MFn::kBlinn)) {
			if (shader_obj.isNull())
				shader_obj = obj;
		}
	}
	if (shader_obj.isNull()) {
		msg("xray_re: can't find shader node for surface %s", surf_name);
		MGlobal::displayError(MString("xray_re: can't find shader node for surface ") + surf_name);
		return surface;
	}

	MFnDependencyNode shader_fn(shader_obj);
	shader_fn.findPlug("c").connectedTo(connected_plugs, true, false);
	for (unsigned i = connected_plugs.length(); i != 0;) {
		MFnDependencyNode dep_fn(connected_plugs[--i].node());
		if (dep_fn.typeName() == "file") {
			MString file_path = dep_fn.findPlug("ftn").asString();
			if (file_path.numChars()) {
				int i, j;
				if ((i = file_path.rindexW('/')) < 0)
					i = file_path.rindexW('\\');
				if ((j = file_path.rindexW('.')) < 0)
					j = file_path.numChars();
				MString name(file_path.substringW(i + 1, j - 1));
				if ((i = name.indexW('_')) > 0) {
					surface->texture() = (name.substringW(0, i - 1) + "\\" + name).asChar();
				} else {
					surface->texture() = name.asChar();
				}
			}
			break;
		}
	}
	return surface;
}

MStatus maya_export_tools::extract_surfaces(MFnMesh& mesh_fn, xr_surfmap_vec& surfmaps)
{
	MObjectArray shading_groups;
	MIntArray faces;
	MStatus status = mesh_fn.getConnectedShaders(0, shading_groups, faces);
	if (!status || shading_groups.length() == 0) {
		msg("xray_re: can't get connected shaders for mesh %s", mesh_fn.name().asChar());
		MGlobal::displayError(MString("xray_re: can't get connected shaders for mesh ") +
			mesh_fn.fullPathName().asChar());
		return MS::kInvalidParameter;
	}
	surfmaps.resize(shading_groups.length());
	for (unsigned i = faces.length(); i != 0;) {
		xr_surfmap* smap = surfmaps[faces[--i]];
		if (smap == 0) {
			MFnSet set_fn(shading_groups[faces[i]], &status);
			CHECK_MSTATUS(status);
			const char* surf_name = set_fn.name().asChar();
			xr_surface*& surface = m_shared_surfaces[surf_name];
			if (surface == 0) {
				smap = new xr_surfmap(create_surface(surf_name, set_fn));
				surface = smap->surface;
			} else {
				smap = new xr_surfmap(surface);
			}
			surfmaps[faces[i]] = smap;
		}
		smap->faces.push_back(i);
	}
	return status;
}

struct temp_edge {
			temp_edge();
	uint32_t	faces[2];
};

inline temp_edge::temp_edge() { faces[0] = UINT32_MAX; faces[1] = UINT32_MAX; }

struct temp_face {
	uint32_t	edges[3];
};

// FIXME: consider using 3ds Max smoothing groups.
static MStatus extract_smoothing_groups(MFnMesh& mesh_fn, std::vector<uint32_t>& sgroups)
{
	MStatus status = MS::kSuccess;

	MIntArray connected;

	temp_edge* temp_edges = new temp_edge[unsigned(mesh_fn.numEdges() & INT_MAX)];
	for (MItMeshEdge it(mesh_fn.object()); !it.isDone(); it.next()) {
		if (it.isSmooth()) {
			it.getConnectedFaces(connected, &status);
			CHECK_MSTATUS(status);
			unsigned n = connected.length();
			if (n <= 2) {
				temp_edge& edge = temp_edges[it.index()];
				while (n) {
					--n;
					edge.faces[n] = uint32_t(connected[n] & INT_MAX);
				}
			}
		}
	}

	unsigned num_faces = unsigned(mesh_fn.numPolygons() & INT_MAX);
	temp_face* temp_faces = new temp_face[num_faces];
	for (MItMeshPolygon it(mesh_fn.object()); !it.isDone(); it.next()) {
		status = it.getEdges(connected);
		CHECK_MSTATUS(status);
		unsigned n = connected.length();
		if (n != 3) {
			msg("xray_re: can't build smoothing groups");
			MGlobal::displayError(MString("xray_re: can't build smoothing groups"));
			delete[] temp_edges;
			delete[] temp_faces;
			return MS::kInvalidParameter;
		}
		temp_face& face = temp_faces[it.index(&status)];
		CHECK_MSTATUS(status);
		while (n) {
			--n;
			face.edges[n] = uint32_t(connected[n] & INT_MAX);
		}
	}

	sgroups.assign(num_faces, EMESH_NO_SG);
	std::vector<uint32_t> adjacents;
	adjacents.reserve(512);
	uint32_t sgroup = 0;
	for (uint_fast32_t base_idx = num_faces; base_idx != 0;) {
		if (sgroups[--base_idx] != EMESH_NO_SG)
			continue;
		bool new_sgroup = false;
		for (uint_fast32_t face_idx = base_idx;;) {
			const temp_face& face = temp_faces[face_idx];
			for (uint_fast32_t i = 3; i != 0;) {
				temp_edge& edge = temp_edges[face.edges[--i]];
				if (edge.faces[0] == UINT32_MAX || edge.faces[1] == UINT32_MAX)
					continue;
				if (!new_sgroup) {
					new_sgroup = true;
					sgroups[face_idx] = sgroup;
				}
				uint32_t adj_face_idx = edge.faces[0] == face_idx ?
						edge.faces[1] : edge.faces[0];
				if (sgroups[adj_face_idx] == EMESH_NO_SG) {
					adjacents.push_back(adj_face_idx);
					sgroups[adj_face_idx] = sgroup;
				}
			}
			if (adjacents.empty())
				break;
			face_idx = adjacents.back();
			adjacents.pop_back();
		}
		if (new_sgroup)
			++sgroup;
	}

	delete[] temp_edges;
	delete[] temp_faces;

	return status;
}

void maya_export_tools::commit_surfaces(xr_surface_vec& surfaces)
{
	surfaces.reserve(m_shared_surfaces.size());
	for (xr_surface_map_it it = m_shared_surfaces.begin(),
			end = m_shared_surfaces.end(); it != end; ++it) {
		surfaces.push_back(it->second);
	}
}

xr_object* maya_export_tools::create_object(MObjectArray& mesh_objs)
{
	MStatus status;

	xr_object* object = new xr_object;
	object->flags() = EOF_STATIC;
	object->meshes().reserve(mesh_objs.length());

	for (unsigned i = mesh_objs.length(); i != 0;) {
		MFnMesh mesh_fn(mesh_objs[--i]);

		xr_mesh* mesh = new xr_mesh;
		object->meshes().push_back(mesh);
		mesh->name() = mesh_fn.name().asChar();

		if (!(status = extract_points(mesh_fn, mesh->points(), mesh->bbox())))
			goto fail;

		if (!(status = extract_faces(mesh_fn, mesh->faces())))
			goto fail;

		if (!(status = extract_uvs(mesh_fn, mesh->faces(), mesh->vmrefs(), mesh->vmaps())))
			goto fail;

		if (!(status = extract_surfaces(mesh_fn, mesh->surfmaps())))
			goto fail;

		if (!(status = extract_smoothing_groups(mesh_fn, mesh->sgroups())))
			goto fail;
	}

	commit_surfaces(object->surfaces());

	return object;

fail:
	delete object;
	return 0;
}

xr_object* maya_export_tools::create_skl_object(MObject& mesh_obj, MObject& skin_obj)
{
	MStatus status;

	xr_object* object = new xr_object;
	object->flags() = EOF_DYNAMIC;

	MFnMesh mesh_fn(mesh_obj);

	xr_mesh* mesh = new xr_mesh;
	// attach now to allow auto-deletion in case of error
	object->meshes().push_back(mesh);
	mesh->name() = mesh_fn.name().asChar();

	MFnSkinCluster skin_fn(skin_obj);
	if (!(status = extract_bones(skin_fn, object->bones())))
		goto fail;

	if (!(status = extract_points(mesh_fn, mesh->points(), mesh->bbox())))
		goto fail;

	if (!(status = extract_faces(mesh_fn, mesh->faces())))
		goto fail;

	if (!(status = extract_uvs(mesh_fn, mesh->faces(), mesh->vmrefs(), mesh->vmaps())))
		goto fail;

	if (!(status = extract_weights(mesh_fn, skin_fn, mesh->faces(), mesh->vmrefs(), mesh->vmaps())))
		goto fail;

	if (!(status = extract_surfaces(mesh_fn, mesh->surfmaps())))
		goto fail;

	if (!(status = extract_smoothing_groups(mesh_fn, mesh->sgroups())))
		goto fail;

	object->partitions().push_back(new xr_partition(object->bones()));

	commit_surfaces(object->surfaces());
	object->reset_bones_parts(object->partitions());
	return object;

fail:
	delete object;
	return 0;
}

static void collect_meshes(MObjectArray& mesh_objs, MObject& root_obj = MObject::kNullObj)
{
	MItDag dag_it;
	if (!root_obj.isNull() && !dag_it.reset(root_obj))
		return;

	MStatus status;
	for (MDagPath dag_path; !dag_it.isDone(); dag_it.next()) {
		status = dag_it.getPath(dag_path);
		if (!status)
			continue;
		MFnDagNode dag_fn(dag_path);
		if (dag_fn.isIntermediateObject())
			continue;
		if (!dag_path.hasFn(MFn::kTransform))
			continue;
		for (unsigned i = dag_fn.childCount(); i != 0;) {
			MObject obj = dag_fn.child(--i);
			if (obj.hasFn(MFn::kMesh)) {
				MFnMesh mesh_fn(obj);
				if (!mesh_fn.isIntermediateObject())
					mesh_objs.append(obj);
			}
		}
	}
}

static void collect_meshes(MObjectArray& mesh_objs, bool selection_only)
{
	if (selection_only) {
		MSelectionList selection;
		MStatus status = MGlobal::getActiveSelectionList(selection);
		if (!status || selection.isEmpty())
			return;
		MDagPath dag_path;
		for (MItSelectionList it(selection); !it.isDone(); it.next()) {
			if (!it.getDagPath(dag_path))
				continue;
			MObject root_obj = dag_path.node(&status);
			if (status)
				collect_meshes(mesh_objs, root_obj);
		}
	} else {
		collect_meshes(mesh_objs);
	}
}

MStatus maya_export_tools::export_object(const char* path, bool selection_only)
{
	MObjectArray mesh_objs;
	collect_meshes(mesh_objs, selection_only);
	if (mesh_objs.length() == 0) {
		msg("xray_re: can't find any mesh to export");
		MGlobal::displayError("xray_re: can't find any mesh to export");
		return MS::kFailure;
	}

	m_skeletal = false;

	MStatus status = MS::kFailure;
	if (xr_object* object = create_object(mesh_objs)) {
		if (object->save_object(path))
			status = MS::kSuccess;
		delete object;
	}

	return status;
}

static MStatus find_mesh_and_skin(MObject* mesh_obj, MObject* skin_obj, bool selection_only)
{
	MObjectArray mesh_objs;
	collect_meshes(mesh_objs, selection_only);
	switch (mesh_objs.length()) {
	case 0:
		msg("xray_re: can't find any mesh to export");
		MGlobal::displayError("xray_re: can't find any mesh to export");
		return MS::kFailure;

	case 1:
		break;

	default:
		msg("xray_re: can't handle multiple meshes in skeletal object");
		MGlobal::displayError("xray_re: can't handle multiple meshes in skeletal object");
		return MS::kFailure;
	}

	MObjectArray skin_objs;
	for (MItDependencyNodes dep_it(MFn::kSkinClusterFilter); !dep_it.isDone(); dep_it.next()) {
		MObject skin_obj = dep_it.thisNode();
		MFnSkinCluster skin_fn(skin_obj);
		MObjectArray affected;
		skin_fn.getOutputGeometry(affected);
		msg("xray_re: skin cluster %s", skin_fn.name().asChar());
		MGlobal::displayInfo(MString("xray_re: skin cluster ") + skin_fn.name().asChar());
		for (unsigned i = affected.length(); i != 0;) {
			if (affected[--i] == mesh_objs[0])
				skin_objs.append(skin_obj);
		}
	}
	switch (skin_objs.length()) {
	case 0:
		msg("xray_re: can't find skin cluster for mesh");
		MGlobal::displayError("xray_re: can't find skin cluster for mesh");
		return MS::kFailure;

	case 1:
		break;

	default:
		msg("xray_re: can't handle multiple skin clusters in skeletal object");
		MGlobal::displayError("xray_re: can't handle multiple skin clusters in skeletal object");
		return MS::kFailure;
	}

	if (mesh_obj)
		*mesh_obj = mesh_objs[0];
	if (skin_obj)
		*skin_obj = skin_objs[0];
	return MS::kSuccess;
}

MStatus maya_export_tools::export_skl_object(const char* path, bool selection_only)
{
	MObject mesh_obj, skin_obj;
	MStatus status = find_mesh_and_skin(&mesh_obj, &skin_obj, selection_only);
	if (!status)
		return status;

	m_skeletal = true;

	status = MS::kFailure;
	if (xr_object* object = create_skl_object(mesh_obj, skin_obj)) {
		if (object->save_object(path))
			status = MS::kSuccess;
		delete object;
	}
	return status;
}

MStatus maya_export_tools::export_skl(const char* path, bool selection_only)
{
	if (MTime::uiUnit() != MTime::kNTSCFrame)
		msg("xray_re: motion export with non-NTSC frame frequency was not tested!");
		MGlobal::displayWarning("xray_re: motion export with non-NTSC frame frequency was not tested!");

	MObject skin_obj;
	MStatus status = find_mesh_and_skin(0, &skin_obj, selection_only);
	if (!status)
		return status;

	MFnSkinCluster skin_fn(skin_obj);
	MDagPathArray joints;
	skin_fn.influenceObjects(joints, &status);
	unsigned num_joints = joints.length();
	if (num_joints == 0) {
		msg("xray_re: can't find any influence object");
		MGlobal::displayError("xray_re: can't find any influence object");
		return MS::kFailure;
	}
	xr_bone_motion_vec bmotions(num_joints);
	for (unsigned i = num_joints; i != 0;) {
		MFnIkJoint joint_fn(joints[--i], &status);
		if (!status) {
			msg("xray_re: can't handle non-joint node %s", joints[i].partialPathName().asChar());
			MGlobal::displayWarning(MString("xray_re: can't handle non-joint node ") +
				joints[i].partialPathName().asChar());
			return status;
		}
		xr_bone_motion* bmotion = new xr_bone_motion(joint_fn.name().asChar());
		bmotion->create_envelopes();
		bmotions[i] = bmotion;
	}

	MTime saved_time(MAnimControl::currentTime());

	int32_t frame_start = int32_t(MAnimControl::minTime().as(MTime::kNTSCFrame));
	int32_t frame_end = int32_t(MAnimControl::maxTime().as(MTime::kNTSCFrame));
	msg("xray_re: animation range=%d-%d", frame_start, frame_end);
	MGlobal::displayInfo(MString("xray_re: animation range=") + frame_start + "-" + frame_end);

	for (int32_t frame = frame_start; frame != frame_end; ++frame) {
		MGlobal::viewFrame(MTime(double(frame), MTime::kNTSCFrame));
		float time = frame/30.f;
		for (unsigned i = num_joints; i != 0;) {
			MFnIkJoint joint_fn(joints[--i]);
			xr_envelope* const* envelopes = bmotions[i]->envelopes();

			MVector t = joint_fn.getTranslation(MSpace::kTransform, &status);
			CHECK_MSTATUS(status);
			envelopes[0]->insert_key(time, float(MDistance(t.x, MDistance::kCentimeters).asMeters()));
			envelopes[1]->insert_key(time, float(MDistance(t.y, MDistance::kCentimeters).asMeters()));
			envelopes[2]->insert_key(time, float(MDistance(-t.z, MDistance::kCentimeters).asMeters()));

			MEulerRotation r;
			status = joint_fn.getRotation(r);
			CHECK_MSTATUS(status);
			r.reorderIt(MEulerRotation::kZXY);
			envelopes[4]->insert_key(time, float(-r.x));
			envelopes[3]->insert_key(time, float(-r.y));
			envelopes[5]->insert_key(time, float(r.z));
		}
	}
	xr_skl_motion* smotion = new xr_skl_motion;

	smotion->name() = "unnamed";
	smotion->fps() = 30.f;
	smotion->set_frame_range(frame_start, frame_end);
	smotion->bone_motions().swap(bmotions);
	status = smotion->save_skl(path) ? MS::kSuccess : MS::kFailure;
	delete smotion;

	MAnimControl::setCurrentTime(saved_time);

	return status;
}
