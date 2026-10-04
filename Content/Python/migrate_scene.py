"""Import the approved Seattle manifest after full editor subsystems initialize.

UnrealEditor-Cmd.exe SeattleUnreal.uproject -nullrhi -unattended -nop4
    -ExecutePythonScript=Content/Python/migrate_scene.py

Requires PythonScriptPlugin, EditorScriptingUtilities, and the compiled native
SeattleUnreal editor module. No network, Blender, rendering, or source writes.
"""

import hashlib
import json
import math
from pathlib import Path
import re
import traceback

import unreal


ROOT = "/Game/Seattle"
MAP = ROOT + "/Maps/FifthUniversity"
OWNER_KEY = "SeattleImporterOwner"
OWNER = "SeattleStreetImporter.v1"
TEXTURE_ROLES = ("albedo", "normalOpenGL", "roughness", "metallicSmoothness", "occlusion")
FOLIAGE = {"leaf", "leaf-light", "leaf-dark", "leaf-vein"}
GLASS = {"glass", "glass-light", "glass-shadow"}
EXPECTED_INSTANCES = {"buildings": 9, "props": 88, "surfaces": 115}
EXPECTED_MESHES = {"building": 9, "prop": 13, "surface": 115}
KIND_COUNT = {"building": "buildings", "prop": "props", "surface": "surfaces"}
BOUNDS_ABSOLUTE_TOLERANCE_CM = 0.1
BOUNDS_RELATIVE_TOLERANCE = 0.00001


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def safe_name(value):
    name = re.sub(r"[^A-Za-z0-9_]", "_", value)
    require(bool(name), "Empty asset name")
    return name


def finite(value, context):
    require(isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(value), "Non-finite numeric value: " + context)
    return float(value)


def check_vector(value, context):
    for axis in "xyz":
        finite(value[axis], context + "." + axis)


def vector(value):
    return unreal.Vector(value["x"], value["y"], value["z"])


def vector_json(value):
    return {axis: float(getattr(value, axis)) for axis in "xyz"}


def source_file(value):
    path = Path(value)
    require(path.is_absolute() and path.is_file(), "Missing absolute source file: " + str(path))
    return path


def validate_bounds(bounds, context):
    for edge in ("min", "max"):
        check_vector(bounds[edge], context + "." + edge)
    for axis in "xyz":
        require(bounds["min"][axis] <= bounds["max"][axis], "Inverted bounds: " + context)
    require(any(bounds["max"][a] > bounds["min"][a] for a in "xyz"), "Empty bounds: " + context)


def validate_manifest(manifest, layout):
    require(manifest["schemaVersion"] == 1 and layout["schemaVersion"] == 1, "Unsupported schema")
    require(Path(manifest["sourceRoot"]).is_dir(), "Missing sourceRoot")
    require(manifest["expected"] == EXPECTED_INSTANCES, "Manifest changes the approved scene counts")
    check_vector(layout["spawn"], "spawn")
    finite(layout["spawnYaw"], "spawnYaw")
    materials = {}
    material_names = set()
    for row in manifest["materials"]:
        name = row["name"]
        key = safe_name(name).lower()
        require(key not in material_names, "Duplicate/colliding material name: " + name)
        material_names.add(key)
        materials[name] = row
        for role in TEXTURE_ROLES:
            if row[role]:
                source_file(row[role])
        for channel in "rgba":
            finite(row["baseColor"][channel], name + ".baseColor." + channel)
        for axis in "xy":
            finite(row["tiling"][axis], name + ".tiling." + axis)
        require(finite(row["normalScale"], name + ".normalScale") >= 0, "Negative normal scale: " + name)
        require(0 <= finite(row["smoothnessScale"], name + ".smoothnessScale") <= 1,
                "Smoothness scale outside [0,1]: " + name)
    meshes = {}
    package_names = set()
    mesh_counts = dict.fromkeys(EXPECTED_MESHES, 0)
    for row in manifest["meshes"]:
        mesh_id = row["id"]
        require(mesh_id not in meshes, "Duplicate mesh id: " + mesh_id)
        require(row["kind"] in mesh_counts, "Unknown mesh kind: " + mesh_id)
        mesh_counts[row["kind"]] += 1
        meshes[mesh_id] = row
        source_file(row["fbx"])
        validate_bounds(row["bounds"], mesh_id)
        require(row["materials"], "Missing material slots: " + mesh_id)
        require(len(set(row["materials"])) == len(row["materials"]), "Duplicate declared slot: " + mesh_id)
        for name in row["materials"]:
            require(name in materials, "Unknown material " + name + " on " + mesh_id)
        names = ["SM_" + safe_name(mesh_id)]
        if row["collisionFbx"]:
            source_file(row["collisionFbx"])
            require("collisionBounds" in row, "Authoritative collisionBounds required: " + mesh_id)
            validate_bounds(row["collisionBounds"], mesh_id + ".collision")
            names.append("SM_" + safe_name(mesh_id) + "_Collision")
        for name in names:
            require(name.lower() not in package_names, "Colliding mesh package: " + name)
            package_names.add(name.lower())
    require(mesh_counts == EXPECTED_MESHES, "Mesh inventory mismatch: " + str(mesh_counts))
    labels = set()
    ids = set()
    instance_counts = dict.fromkeys(EXPECTED_INSTANCES, 0)
    for row in manifest["instances"]:
        require(row["id"] not in ids, "Duplicate instance id: " + row["id"])
        ids.add(row["id"])
        require(row["mesh"] in meshes, "Unknown instance mesh: " + row["mesh"])
        mesh = meshes[row["mesh"]]
        kind = mesh["kind"]
        label = row["label"]
        require(label.startswith(kind + ":") and label not in labels, "Invalid/duplicate label: " + label)
        labels.add(label)
        instance_counts[KIND_COUNT[kind]] += 1
        check_vector(row["position"], label)
        finite(row["yaw"], label + ".yaw")
        require(finite(row["scale"], label + ".scale") > 0, "Non-positive instance scale: " + label)
        require(isinstance(row["collision"], bool), "Collision must be boolean: " + label)
        if kind == "surface" and "marking" in mesh["id"].lower():
            require(not row["collision"], "Road markings must not collide: " + label)
        if row["collision"] and (kind == "building" or (kind == "prop" and "tree" in mesh["id"].lower())):
            require(bool(mesh["collisionFbx"]), "Authoritative collision required: " + label)
    require(instance_counts == EXPECTED_INSTANCES, "Instance count mismatch: " + str(instance_counts))
    return materials, meshes


class StreetImporter:
    def __init__(self, manifest, layout, report):
        self.manifest = manifest
        self.layout = layout
        self.report = report
        self.material_rows, self.mesh_rows = validate_manifest(manifest, layout)
        self.assets = unreal.EditorAssetLibrary
        self.tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(all(value is not None for value in (self.mesh_editor, self.actors, self.levels, self.editor)),
                "Editor subsystems unavailable; use -ExecutePythonScript rather than -run=pythonscript")
        self.materials = {}
        self.textures = {}
        self.texture_names = {}
        self.meshes = {}
        self.collisions = {}
        self.collision_modes = {}
        self.game_mode = None

    def owned(self, obj):
        return self.assets.get_metadata_tag(obj, OWNER_KEY) == OWNER

    def mark_owned(self, obj):
        self.assets.set_metadata_tag(obj, OWNER_KEY, OWNER)
        self.assets.set_metadata_tag(obj, "SeattleSourceManifestSHA256", self.report["sourceManifestSHA256"])

    def existing_owned(self, path, asset_class):
        if not self.assets.does_asset_exist(path):
            return None
        asset = self.assets.load_asset(path)
        require(asset is not None and isinstance(asset, asset_class), "Unexpected asset type: " + path)
        require(self.owned(asset), "Refusing to overwrite non-importer asset: " + path)
        return asset

    def save(self, asset):
        self.mark_owned(asset)
        require(self.assets.save_loaded_asset(asset, only_if_is_dirty=False), "Cannot save " + asset.get_path_name())
        self.report["savedAssets"].append(asset.get_path_name())

    def import_asset(self, source, destination, name, asset_class, factory, options=None):
        path = destination + "/" + name
        self.existing_owned(path, asset_class)
        task = unreal.AssetImportTask()
        for prop, value in {
            "filename": str(source_file(source)), "destination_path": destination,
            "destination_name": name, "automated": True, "replace_existing": True,
            "replace_existing_settings": True, "save": False, "async_": False,
            "factory": factory,
        }.items():
            task.set_editor_property(prop, value)
        if options is not None:
            task.set_editor_property("options", options)
        self.tools.import_asset_tasks([task])
        results = list(task.get_objects())
        require(len(results) == 1 and isinstance(results[0], asset_class),
                "Import must yield exactly one " + asset_class.__name__ + ": " + str(source))
        asset = results[0]
        require(asset.get_path_name() == path + "." + name,
                "Importer changed deterministic asset path: " + asset.get_path_name())
        self.mark_owned(asset)
        return asset

    def texture(self, source, role):
        key = (str(source_file(source)).lower(), role == "albedo", role == "normalOpenGL")
        if key in self.textures:
            return self.textures[key]
        name = "T_" + safe_name(Path(source).stem)
        require(name.lower() not in self.texture_names or self.texture_names[name.lower()] == key,
                "Texture name collision or incompatible color spaces: " + name)
        self.texture_names[name.lower()] = key
        factory = unreal.TextureFactory()
        factory.set_editor_property("create_material", False)
        texture = self.import_asset(source, ROOT + "/Textures", name, unreal.Texture2D, factory)
        compression = (unreal.TextureCompressionSettings.TC_DEFAULT if role == "albedo" else
                       unreal.TextureCompressionSettings.TC_NORMALMAP if role == "normalOpenGL" else
                       unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("compression_settings", compression)
        texture.set_editor_property("srgb", role == "albedo")
        texture.set_editor_property("flip_green_channel", role == "normalOpenGL")
        texture.set_editor_property("virtual_texture_streaming", False)
        self.save(texture)
        require(texture.get_editor_property("srgb") == (role == "albedo"), "Texture color-space mismatch: " + source)
        require(texture.get_editor_property("compression_settings") == compression, "Texture compression mismatch: " + source)
        require(texture.get_editor_property("flip_green_channel") == (role == "normalOpenGL"), "Normal convention mismatch: " + source)
        self.textures[key] = texture
        self.report["textures"].append({"asset": texture.get_path_name(), "source": source,
                                        "role": role, "srgb": role == "albedo",
                                        "compression": str(compression), "flipGreen": role == "normalOpenGL"})
        return texture

    def material(self, row):
        name = "M_" + safe_name(row["name"])
        path = ROOT + "/Materials/" + name
        material = self.existing_owned(path, unreal.Material)
        if material is None:
            material = self.tools.create_asset(name, ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
        require(material is not None, "Cannot create material: " + name)
        self.mark_owned(material)
        lib = unreal.MaterialEditingLibrary
        lib.delete_all_material_expressions(material)
        foliage = row["name"] in FOLIAGE
        dielectric = row["name"] in GLASS
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
        material.set_editor_property("two_sided", foliage)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE
                                     if foliage else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        node_count = 0

        def node(cls, **properties):
            nonlocal node_count
            expression = lib.create_material_expression(material, cls, -1000 + (node_count % 5) * 200, (node_count // 5) * 200)
            require(expression is not None, "Cannot create material expression in " + name)
            node_count += 1
            for prop, value in properties.items():
                expression.set_editor_property(prop, value)
            return expression

        def connect(source, destination, pin="", output=""):
            require(lib.connect_material_expressions(source, output, destination, pin),
                    "Failed material graph connection: " + name + "." + pin)

        def output(source, prop, pin=""):
            require(lib.connect_material_property(source, pin, prop), "Failed material output: " + name + "." + str(prop))

        def scalar(value):
            return node(unreal.MaterialExpressionConstant, r=value)

        def multiply(left, right):
            expression = node(unreal.MaterialExpressionMultiply)
            connect(left, expression, "A")
            connect(right, expression, "B")
            return expression

        def one_minus(source, pin=""):
            expression = node(unreal.MaterialExpressionOneMinus)
            connect(source, expression, output=pin)
            return expression

        uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0,
                  u_tiling=row["tiling"]["x"], v_tiling=row["tiling"]["y"])
        samples = {}
        for role in TEXTURE_ROLES:
            if row[role]:
                sampler = (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role == "albedo" else
                           unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role == "normalOpenGL" else
                           unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                sample = node(unreal.MaterialExpressionTextureSample, texture=self.texture(row[role], role), sampler_type=sampler)
                connect(uv, sample, "UVs")
                samples[role] = sample
        color = row["baseColor"]
        tint = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(color["r"], color["g"], color["b"], 1.0))
        base = multiply(samples["albedo"], tint) if "albedo" in samples else tint
        output(base, unreal.MaterialProperty.MP_BASE_COLOR)
        if foliage:
            output(multiply(base, scalar(0.5)), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
            output(scalar(1.0), unreal.MaterialProperty.MP_OPACITY)
        if "normalOpenGL" in samples:
            # Scale tangent XY, retain the decoded Z, then renormalize.
            xy = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
            z = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
            connect(samples["normalOpenGL"], xy)
            connect(samples["normalOpenGL"], z)
            normal = node(unreal.MaterialExpressionAppendVector)
            connect(multiply(xy, scalar(row["normalScale"])), normal, "A")
            connect(z, normal, "B")
            normalized = node(unreal.MaterialExpressionNormalize)
            connect(normal, normalized)
            output(normalized, unreal.MaterialProperty.MP_NORMAL)
        if "roughness" in samples:
            smoothness = one_minus(samples["roughness"], "R")
            roughness_source = "roughness.R"
        elif "metallicSmoothness" in samples:
            smoothness = node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
            connect(samples["metallicSmoothness"], smoothness, output="A")
            roughness_source = "1-metallicSmoothness.A"
        else:
            smoothness = scalar(1.0)
            roughness_source = "scalar smoothness"
        roughness = one_minus(multiply(smoothness, scalar(row["smoothnessScale"])))
        output(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
        if "metallicSmoothness" in samples and not dielectric:
            output(samples["metallicSmoothness"], unreal.MaterialProperty.MP_METALLIC, "R")
        else:
            output(scalar(0.0), unreal.MaterialProperty.MP_METALLIC)
        output(scalar(0.5), unreal.MaterialProperty.MP_SPECULAR)
        if "occlusion" in samples:
            output(samples["occlusion"], unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, "R")
        errors = list(lib.recompile_material(material))
        require(not errors, "Material compile failure " + name + ": " + "; ".join(errors))
        self.save(material)
        self.materials[row["name"]] = material
        self.report["materials"].append({"name": row["name"], "asset": material.get_path_name(),
                                         "twoSided": foliage, "shadingModel": "TwoSidedFoliage" if foliage else "DefaultLit",
                                         "opaque": True, "dielectricGlass": dielectric,
                                         "roughnessSource": roughness_source,
                                         "roughnessFormula": "1-(1-sourceRoughness)*smoothnessScale",
                                         "tiling": row["tiling"], "tint": color,
                                         "normalScale": row["normalScale"], "smoothnessScale": row["smoothnessScale"],
                                         "compileErrors": errors})

    def mesh_import_options(self):
        options = unreal.FbxImportUI()
        for prop, value in {
            "import_mesh": True, "import_as_skeletal": False, "import_animations": False,
            "import_materials": False, "import_textures": False,
            "automated_import_should_detect_type": False, "override_full_name": True,
            "reset_to_fbx_on_material_conflict": True,
            "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
        }.items():
            options.set_editor_property(prop, value)
        data = options.get_editor_property("static_mesh_import_data")
        for prop, value in {
            "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
            "normal_generation_method": unreal.FBXNormalGenerationMethod.MIKK_T_SPACE,
            "convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
            "import_uniform_scale": 1.0, "import_translation": unreal.Vector(0, 0, 0),
            "import_rotation": unreal.Rotator(pitch=0, yaw=0, roll=0), "transform_vertex_to_absolute": True,
            "bake_pivot_in_vertex": False, "combine_meshes": True,
            "auto_generate_collision": False, "generate_lightmap_u_vs": False,
            "remove_degenerates": False, "build_nanite": False,
            "reorder_material_to_fbx_order": True, "import_mesh_lods": False,
            "static_mesh_lod_group": "None", "compute_weighted_normals": False,
            "distance_field_resolution_scale": 1.0,
            "vertex_color_import_option": unreal.VertexColorImportOption.REPLACE,
        }.items():
            data.set_editor_property(prop, value)
        return options

    def measured_bounds(self, mesh, expected, context):
        box = mesh.get_bounding_box()
        actual = {"min": vector_json(box.min), "max": vector_json(box.max)}
        validate_bounds(actual, context + ".measured")
        differences = []
        tolerances = {}
        for axis in "xyz":
            tolerance = max(BOUNDS_ABSOLUTE_TOLERANCE_CM,
                            abs(expected["max"][axis] - expected["min"][axis]) * BOUNDS_RELATIVE_TOLERANCE)
            tolerances[axis] = tolerance
            for edge in ("min", "max"):
                if abs(actual[edge][axis] - expected[edge][axis]) > tolerance:
                    differences.append(edge + "." + axis + ": " + str(actual[edge][axis]) + " != " + str(expected[edge][axis]))
        result = {"asset": mesh.get_path_name(), "expected": expected, "measured": actual,
                  "toleranceCm": tolerances, "pass": not differences, "differences": differences}
        self.report["bounds"].append(result)
        require(not differences, "Bounds/sign/scale mismatch " + context + ": " + "; ".join(differences))
        return actual

    def set_mesh_collision(self, mesh, enabled):
        body = mesh.get_editor_property("body_setup")
        require(body is not None, "Missing mesh BodySetup: " + mesh.get_path_name())
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        # Walkable source sheets stay one-sided in rendering, but must support
        # sweeps from either direction; this does not fill concave spaces.
        body.set_editor_property("double_sided_geometry", True)
        for section in range(mesh.get_num_sections(0)):
            self.mesh_editor.enable_section_collision(mesh, enabled, 0, section)
            require(self.mesh_editor.is_section_collision_enabled(mesh, 0, section) == enabled,
                    "Section collision mismatch: " + mesh.get_path_name())
        require(self.mesh_editor.get_collision_complexity(mesh) == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
                "Collision complexity did not apply: " + mesh.get_path_name())
        require(self.mesh_editor.get_simple_collision_count(mesh) == 0, "Unexpected generated simple collision: " + mesh.get_path_name())

    def import_mesh(self, row):
        mesh_id = row["id"]
        name = "SM_" + safe_name(mesh_id)
        mesh = self.import_asset(row["fbx"], ROOT + "/Meshes", name, unreal.StaticMesh,
                                 unreal.FbxFactory(), self.mesh_import_options())
        require(mesh.get_num_triangles(0) > 0, "Empty mesh: " + mesh_id)
        self.measured_bounds(mesh, row["bounds"], mesh_id)
        settings = self.mesh_editor.get_lod_build_settings(mesh, 0)
        require(not settings.get_editor_property("recompute_normals")
                and not settings.get_editor_property("recompute_tangents"), "Imported normals/tangents not preserved: " + mesh_id)
        settings.set_editor_property("use_full_precision_u_vs", True)
        settings.set_editor_property("use_high_precision_tangent_basis", True)
        self.mesh_editor.set_lod_build_settings(mesh, 0, settings)
        require(self.mesh_editor.get_num_uv_channels(mesh, 0) >= 1, "Missing source UV0: " + mesh_id)
        slots = list(mesh.get_editor_property("static_materials"))
        expected_slots = {safe_name(name): name for name in row["materials"]}
        actual_slots = [str(slot.get_editor_property("imported_material_slot_name")) for slot in slots]
        require(len(actual_slots) == len(expected_slots) and set(actual_slots) == set(expected_slots),
                "Material slot mismatch " + mesh_id + ": imported=" + str(actual_slots) + " expected=" + str(list(expected_slots)))
        for index, slot_name in enumerate(actual_slots):
            source_name = expected_slots[slot_name]
            material = self.materials[source_name]
            mesh.set_material(index, material)
            assigned = mesh.get_material(index)
            matched = assigned == material
            self.report["materialSlotChecks"].append({"mesh": mesh_id, "slotIndex": index,
                                                      "importedSlot": slot_name, "material": source_name,
                                                      "assignedAsset": assigned.get_path_name() if assigned else None, "pass": matched})
            require(matched, "Material assignment failed: " + mesh_id + "." + slot_name)
        collision_enabled = any(i["collision"] for i in self.manifest["instances"] if i["mesh"] == mesh_id)
        mode = "render_complex_as_simple" if collision_enabled else "none"
        collision = None
        if row["collisionFbx"]:
            collision = self.import_asset(row["collisionFbx"], ROOT + "/Meshes", name + "_Collision",
                                           unreal.StaticMesh, unreal.FbxFactory(), self.mesh_import_options())
            require(collision.get_num_triangles(0) > 0, "Empty authoritative collision mesh: " + mesh_id)
            self.measured_bounds(collision, row["collisionBounds"], mesh_id + ".collision")
            self.set_mesh_collision(collision, True)
            self.save(collision)
            self.collisions[mesh_id] = collision
            try:
                mesh.set_editor_property("complex_collision_mesh", collision)
            except Exception as error:
                # A narrowly identified reflection absence is the only reason
                # to use an equivalent hidden collision actor. All other errors fail.
                if "Failed to find property" not in str(error) or "complex_collision_mesh" not in str(error):
                    raise
                mode = "hidden_authoritative_actor" if collision_enabled else "none"
                self.report["warnings"].append("Custom collision property unavailable; hidden authoritative actor for " + mesh_id)
            else:
                require(mesh.get_editor_property("complex_collision_mesh") == collision, "Custom collision assignment mismatch: " + mesh_id)
                mode = "custom_complex_as_simple" if collision_enabled else "none"
        self.set_mesh_collision(mesh, collision_enabled and mode != "hidden_authoritative_actor")
        triangles = mesh.get_num_triangles(0)
        nanite = (bool(row["nanite"]) and row["kind"] == "building" and triangles >= 10000
                  and not any(name in FOLIAGE for name in row["materials"]))
        if nanite:
            for source_name in row["materials"]:
                material = self.materials[source_name]
                unreal.MaterialEditingLibrary.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE, True)
                self.save(material)
        nanite_settings = self.mesh_editor.get_nanite_settings(mesh)
        nanite_settings.set_editor_property("enabled", nanite)
        nanite_settings.set_editor_property("explicit_tangents", True)
        nanite_settings.set_editor_property("generate_fallback", unreal.NaniteGenerateFallback.ENABLED)
        nanite_settings.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        nanite_settings.set_editor_property("fallback_percent_triangles", 1.0)
        nanite_settings.set_editor_property("fallback_relative_error", 0.0)
        self.mesh_editor.set_nanite_settings(mesh, nanite_settings, True)
        require(self.mesh_editor.get_nanite_settings(mesh).get_editor_property("enabled") == nanite,
                "Nanite setting mismatch: " + mesh_id)
        require(self.mesh_editor.get_collision_complexity(mesh) == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
                "Final mesh build changed collision complexity: " + mesh_id)
        if mode == "custom_complex_as_simple":
            require(mesh.get_editor_property("complex_collision_mesh") == collision,
                    "Final mesh build changed authoritative collision source: " + mesh_id)
        self.save(mesh)
        self.meshes[mesh_id] = mesh
        self.collision_modes[mesh_id] = mode
        self.report["meshes"].append({"id": mesh_id, "kind": row["kind"], "asset": mesh.get_path_name(),
                                     "triangles": triangles, "nanite": nanite,
                                     "naniteDecision": "requested opaque building with >=10000 triangles" if nanite else "not eligible/requested",
                                     "naniteVerification": "saved asset setting; GPU verification is separate",
                                     "fallbackPercentTriangles": 1.0, "normalsTangents": "imported", "uvChannels": self.mesh_editor.get_num_uv_channels(mesh, 0),
                                     "collisionMode": mode, "collisionAsset": collision.get_path_name() if collision else None,
                                     "collisionTraceFlag": str(self.mesh_editor.get_collision_complexity(mesh)),
                                     "collisionSections": sum(self.mesh_editor.is_section_collision_enabled(mesh, 0, i) for i in range(mesh.get_num_sections(0))),
                                     "collisionTriangles": collision.get_num_triangles(0) if collision else triangles if collision_enabled else 0})

    def spawn(self, cls, label, position, rotation=None, tags=None):
        actor = self.actors.spawn_actor_from_class(cls, position, rotation or unreal.Rotator(pitch=0, yaw=0, roll=0))
        require(actor is not None, "Failed spawning " + label)
        actor.set_actor_label(label, mark_dirty=True)
        actor.set_editor_property("tags", tags if tags is not None else [label])
        self.mark_owned(actor)
        return actor

    def static_actor(self, row, collision_only=False):
        mesh_id = row["mesh"]
        label = row["label"]
        actor = self.spawn(unreal.StaticMeshActor, label + ":collision" if collision_only else label,
                           vector(row["position"]), unreal.Rotator(pitch=0, yaw=row["yaw"], roll=0),
                           [label, "StreetCollisionOnly"] if collision_only else [label])
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        require(component.set_static_mesh(self.collisions[mesh_id] if collision_only else self.meshes[mesh_id]),
                "Failed assigning actor mesh: " + label)
        actor.set_actor_scale3d(unreal.Vector(row["scale"], row["scale"], row["scale"]))
        enabled = row["collision"] and (collision_only or self.collision_modes[mesh_id] != "hidden_authoritative_actor")
        component.set_collision_profile_name("BlockAll" if enabled else "NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if enabled else unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("generate_overlap_events", False)
        if collision_only:
            actor.set_actor_hidden_in_game(True)
            component.set_visibility(False, True)
            component.set_hidden_in_game(True, True)
            component.set_cast_shadow(False)
            for prop in ("cast_hidden_shadow", "affect_distance_field_lighting", "affect_dynamic_indirect_lighting",
                         "affect_indirect_lighting_while_hidden", "visible_in_ray_tracing", "render_in_main_pass", "render_in_depth_pass"):
                component.set_editor_property(prop, False)
        require(component.get_collision_enabled() == (unreal.CollisionEnabled.QUERY_AND_PHYSICS if enabled else unreal.CollisionEnabled.NO_COLLISION),
                "Actor collision mismatch: " + label)
        location = vector_json(actor.get_actor_location())
        for axis in "xyz":
            require(abs(location[axis] - row["position"][axis]) < 0.001, "Actor transform mismatch: " + label)
        actual_scale = vector_json(actor.get_actor_scale3d())
        require(all(abs(actual_scale[axis] - row["scale"]) < 0.00001 for axis in "xyz"), "Actor scale mismatch: " + label)
        rotation = actor.get_actor_rotation()
        yaw_error = (rotation.yaw - row["yaw"] + 180.0) % 360.0 - 180.0
        require(abs(yaw_error) < 0.001 and abs(rotation.pitch) < 0.001 and abs(rotation.roll) < 0.001,
                "Actor rotation mismatch: " + label)
        return actor

    def environment(self):
        sun = self.spawn(unreal.DirectionalLight, "Seattle:Sun", unreal.Vector(0, 0, 12000), unreal.Rotator(pitch=-38, yaw=-32, roll=0))
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_intensity(65000.0)
        light.set_use_temperature(True)
        light.set_temperature(5800.0)
        light.set_editor_property("atmosphere_sun_light", True)
        light.set_editor_property("light_source_angle", 1.0)
        self.spawn(unreal.SkyAtmosphere, "Seattle:Atmosphere", unreal.Vector(0, 0, 0))
        sky = self.spawn(unreal.SkyLight, "Seattle:SkyLight", unreal.Vector(0, 0, 1000))
        sky_light = sky.get_component_by_class(unreal.SkyLightComponent)
        sky_light.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky_light.set_editor_property("real_time_capture", True)
        sky_light.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
        sky_light.set_intensity(4.0)
        fog = self.spawn(unreal.ExponentialHeightFog, "Seattle:Fog", unreal.Vector(0, 0, 0))
        fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        fog_component.set_fog_density(0.001)
        fog_component.set_fog_height_falloff(0.25)
        fog_component.set_fog_max_opacity(0.15)
        fog_component.set_editor_property("enable_volumetric_fog", False)
        post = self.spawn(unreal.PostProcessVolume, "Seattle:FixedExposure", unreal.Vector(0, 0, 0))
        post.set_editor_property("unbound", True)
        post.set_editor_property("priority", 100.0)
        post.set_editor_property("blend_weight", 1.0)
        settings = post.get_editor_property("settings")
        values = {
            "dynamic_global_illumination_method": unreal.DynamicGlobalIlluminationMethod.LUMEN,
            "reflection_method": unreal.ReflectionMethod.LUMEN,
            "lumen_scene_lighting_quality": 1.0, "lumen_scene_detail": 1.0,
            "lumen_scene_view_distance": 40000.0, "lumen_final_gather_quality": 1.0,
            "lumen_reflection_quality": 1.0,
            "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
            "auto_exposure_apply_physical_camera_exposure": False,
            "auto_exposure_bias": -11.0, "auto_exposure_bias_curve": None,
            "motion_blur_amount": 0.0, "bloom_intensity": 0.15,
        }
        for prop, value in values.items():
            settings.set_editor_property("override_" + prop, True)
            settings.set_editor_property(prop, value)
        post.set_editor_property("settings", settings)
        applied = post.get_editor_property("settings")
        for prop, value in values.items():
            require(applied.get_editor_property("override_" + prop), "Post-process override missing: " + prop)
            actual = applied.get_editor_property(prop)
            require(abs(actual - value) < 0.0001 if isinstance(value, float) else actual == value,
                    "Post-process setting mismatch: " + prop)
        self.report["featureSettings"] = {
            "directionalLightLux": 65000, "sunTemperatureK": 5800, "sunPitchYaw": [-38, -32],
            "sunMobility": "Movable", "skyMobility": "Movable", "skyRealtimeCapture": True, "skyIntensity": 4.0,
            "sky": "SkyAtmosphere (no substitute sky geometry)", "fogDensity": 0.001,
            "fogMaxOpacity": 0.15, "volumetricFog": False,
            "globalIllumination": "Lumen", "reflections": "Lumen", "lumenQuality": 1.0,
            "lumenViewDistanceCm": 40000, "exposure": "Manual", "physicalCameraExposure": False,
            "exposureCompensationEV": -11.0, "motionBlur": 0.0,
            "visualTuningVerified": False,
            "projectConsoleVariables": {name: unreal.SystemLibrary.get_console_variable_int_value(name) for name in (
                "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.Shadow.Virtual.Enable",
                "r.GenerateMeshDistanceFields", "r.Nanite.ProjectEnabled")},
        }

    def map(self):
        existing = self.existing_owned(MAP, unreal.World)
        if existing is None:
            require(self.levels.new_level(MAP, False), "Cannot create empty FifthUniversity map")
        else:
            require(self.levels.load_level(MAP), "Cannot load importer-owned map")
            for actor in list(self.actors.get_all_level_actors()):
                if self.owned(actor):
                    require(self.actors.destroy_actor(actor), "Cannot remove previous importer actor: " + actor.get_actor_label())
        world = self.editor.get_editor_world()
        require(world is not None, "Missing editor world")
        self.mark_owned(world)
        # Persist ownership before any later failure, so reruns may recover the
        # partially populated importer map without touching unrelated content.
        require(self.levels.save_current_level(), "Cannot persist importer map ownership")
        reserved = {row["label"] for row in self.manifest["instances"]}
        for actor in self.actors.get_all_level_actors():
            require(actor.get_actor_label() not in reserved, "Unowned actor conflicts with manifest: " + actor.get_actor_label())
        helpers = 0
        for row in self.manifest["instances"]:
            self.static_actor(row)
            if row["collision"] and self.collision_modes[row["mesh"]] == "hidden_authoritative_actor":
                self.static_actor(row, True)
                helpers += 1
        spawn = dict(self.layout["spawn"])
        spawn["z"] += 90.0
        self.spawn(unreal.PlayerStart, "Seattle:PlayerStart", vector(spawn), unreal.Rotator(pitch=0, yaw=self.layout["spawnYaw"], roll=0))
        self.environment()
        actual = dict.fromkeys(EXPECTED_INSTANCES, 0)
        scene_labels = []
        for actor in self.actors.get_all_level_actors():
            tags = {str(tag) for tag in actor.get_editor_property("tags")}
            if "StreetCollisionOnly" in tags or not self.owned(actor):
                continue
            label = actor.get_actor_label()
            for kind, count_key in KIND_COUNT.items():
                if label.startswith(kind + ":"):
                    require(label in tags, "Runtime identity tag missing: " + label)
                    actual[count_key] += 1
                    scene_labels.append(label)
        require(actual == EXPECTED_INSTANCES and set(scene_labels) == reserved, "Actual spawned scene differs from manifest: " + str(actual))
        require(self.levels.save_current_level(), "Cannot save FifthUniversity map")
        self.report["savedAssets"].append(MAP + ".FifthUniversity")
        self.report["counts"] = {**actual, "renderActors": len(scene_labels), "collisionHelpers": helpers,
                                 "meshAssets": len(self.meshes), "collisionMeshAssets": len(self.collisions),
                                 "materials": len(self.materials), "textures": len(self.textures)}
        self.report["map"] = MAP
        self.report["playerStart"] = {"position": spawn, "yaw": self.layout["spawnYaw"], "capsuleHalfHeightCm": 90.0}
        self.report["sceneImported"] = True

    def import_assets(self):
        world = self.editor.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
        # These package names are exclusively owned; no folder deletion or global save.
        for folder in ("Materials", "Textures", "Meshes", "Maps"):
            path = ROOT + "/" + folder
            require(self.assets.does_directory_exist(path) or self.assets.make_directory(path),
                    "Cannot create asset directory: " + folder)
        for row in self.manifest["materials"]:
            self.material(row)
        for row in self.manifest["meshes"]:
            self.import_mesh(row)

    def set_runtime_mode(self):
        # Keep assets and the authored map reachable when the native build is
        # blocked, but never substitute an engine/Blueprint game mode or claim
        # a completed migration. The default commandlet still exits nonzero.
        self.game_mode = unreal.load_class(None, "/Script/SeattleUnreal.StreetGameMode")
        require(self.game_mode is not None, "Native StreetGameMode missing; assets/map saved, runtime blocked until SeattleUnrealEditor is built")
        world = self.editor.get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", self.game_mode)
        require(world.get_world_settings().get_editor_property("default_game_mode") == self.game_mode, "Native game mode assignment failed")
        require(self.levels.save_current_level(), "Cannot save native game mode assignment")
        self.report["gameMode"] = self.game_mode.get_path_name()
        self.report["runtimeModeReady"] = True

    def run(self):
        self.import_assets()
        self.map()
        self.set_runtime_mode()


def main():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    command_line = unreal.SystemLibrary.get_command_line()
    override = re.search(r'(?:^|\s)-StreetEvidence=(?:"([^"]+)"|(\S+))', command_line, re.IGNORECASE)
    evidence = Path(override.group(1) or override.group(2)) if override else project / "Evidence"
    require(evidence.is_absolute(), "StreetEvidence must be an absolute path")
    evidence.mkdir(parents=True, exist_ok=True)
    destination = evidence / "import-result.json"
    report = {"schemaVersion": 1, "success": False, "engineVersion": unreal.SystemLibrary.get_engine_version(),
              "sourceManifest": str(project / "SourceData" / "import-manifest.json"),
              "sourceManifestSHA256": None, "coordinateSystem": "Unreal centimeters, X=Unity Z, Y=Unity X, Z=Unity Y",
              "materials": [], "textures": [], "meshes": [], "bounds": [], "materialSlotChecks": [],
              "savedAssets": [], "warnings": [], "errors": [], "renderingVerified": False,
              "runtimeCollisionVerified": False, "sceneImported": False, "runtimeModeReady": False}
    try:
        manifest_bytes = source_file(report["sourceManifest"]).read_bytes()
        report["sourceManifestSHA256"] = hashlib.sha256(manifest_bytes).hexdigest()
        manifest = json.loads(manifest_bytes)
        layout = json.loads((project / "Content" / "StreetData" / "layout.json").read_text(encoding="utf-8-sig"))
        StreetImporter(manifest, layout, report).run()
        report["success"] = True
    except Exception as error:
        report["errors"].append({"message": str(error), "traceback": traceback.format_exc()})
        unreal.log_error("Seattle import failed: " + str(error))
        raise
    finally:
        temporary = destination.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        temporary.replace(destination)
    unreal.log("Seattle import complete: " + str(destination))


if __name__ == "__main__":
    main()
