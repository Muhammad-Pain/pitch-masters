extends Node
func _ready() -> void:
	var app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	app.show_screen("menu", {})
	for i in 20: await get_tree().process_frame
	var shots = [["face_front", Vector3(0.0, 1.66, 1.4), Vector3(0, 1.62, 0)], ["face_side", Vector3(1.3, 1.6, 0.6), Vector3(0, 1.55, 0)], ["body", Vector3(0.6, 1.1, 3.6), Vector3(0, 0.95, 0)]]
	for s in shots:
		app.debug_cam(s[1], s[2])
		app.get_current().visible = false
		for i in 6: await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/home/claude/shots_cpp/%s.png" % s[0])
		print("saved ", s[0])
	get_tree().quit()
