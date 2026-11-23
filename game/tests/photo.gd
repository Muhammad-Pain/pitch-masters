extends Node
## Personal photo feature: load a JPG, put it on a card, open the crop dialog, show squad/menu.
func _ready() -> void:
	var app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	app.show_screen("squad", {})
	for i in 10: await get_tree().process_frame
	var cid: int = int(app.debug_info().get("first_card_id", 0))
	print("PHOTO set: ", app.debug_photo(cid, "/home/claude/test_face.jpg", false), " card ", cid)
	app.show_screen("squad", {})
	for i in 30: await get_tree().process_frame
	await snap("photo_squad")
	print("PHOTO crop: ", app.debug_photo(cid, "/home/claude/test_face.jpg", true))
	for i in 20: await get_tree().process_frame
	await snap("photo_crop")
	app.show_screen("menu", {})
	for i in 60: await get_tree().process_frame
	await snap("photo_menu")
	app.show_screen("packs", {"tab": 1})
	for i in 30: await get_tree().process_frame
	await snap("photo_collection")
	get_tree().quit()

func snap(name: String) -> void:
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	img.save_png("/home/claude/shots_cpp/%s.png" % name)
	print("saved ", name)
