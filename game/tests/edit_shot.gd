extends Node
func _ready() -> void:
	var app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	for s in ["title", "editor", "settings"]:
		app.show_screen(s, {})
		for i in 45: await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/home/claude/shots_cpp/v32_%s.png" % s)
		print("saved ", s)
	get_tree().quit()
