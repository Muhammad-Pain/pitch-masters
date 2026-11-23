extends Node
func _ready() -> void:
	var ps: PackedScene = load("res://assets/characters/AnimationLibrary_Godot_Standard.gltf")
	var n := ps.instantiate()
	add_child(n)
	_dump(n, 0)
	var sk: Skeleton3D = n.find_children("*", "Skeleton3D", true, false)[0]
	for i in sk.get_bone_count():
		print(i, " ", sk.get_bone_name(i), " parent=", sk.get_bone_parent(i), " rest=", sk.get_bone_global_rest(i).origin.snapped(Vector3(0.01,0.01,0.01)))
	var mi: MeshInstance3D = n.find_children("*", "MeshInstance3D", true, false)[0]
	print("aabb ", mi.get_aabb(), " surfaces ", mi.mesh.get_surface_count(), " lods? ", mi.mesh.get_class())
	var arr = mi.mesh.surface_get_arrays(0)
	print("verts ", arr[Mesh.ARRAY_VERTEX].size(), " bones ", arr[Mesh.ARRAY_BONES].size(), " weights ", arr[Mesh.ARRAY_WEIGHTS].size(), " uv ", arr[Mesh.ARRAY_TEX_UV] != null)
	print("skin binds ", mi.skin.get_bind_count() if mi.skin else -1)
	var ap: AnimationPlayer = n.find_children("*", "AnimationPlayer", true, false)[0]
	var names = ap.get_animation_list()
	print("anims: ", names)
	for nm in names:
		var a = ap.get_animation(nm)
		if nm.contains("Jog") or nm.contains("Sprint") or nm.contains("Idle") or nm.contains("Walk"):
			print(nm, " len=", a.length, " loop=", a.loop_mode, " tracks=", a.get_track_count())
	get_tree().quit()
func _dump(n: Node, d: int) -> void:
	if d < 4: print("  ".repeat(d), n.name, " [", n.get_class(), "]")
	for c in n.get_children(): _dump(c, d + 1)
