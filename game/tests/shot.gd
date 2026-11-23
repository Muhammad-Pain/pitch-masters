extends Node
## Screenshots (run under xvfb-run with --rendering-driver opengl3).
var app
var out := "/home/claude/shots_cpp/"

func snap(name: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(out + name + ".png")
	print("saved ", name)

func _ready() -> void:
	DirAccess.make_dir_recursive_absolute(out)
	var which := OS.get_cmdline_user_args()
	app = PMApp.new()
	add_child(app)
	for i in 8: await get_tree().process_frame
	if "ui" in which:
		await snap("title")
		for s in ["menu", "squad", "packs", "league", "patches", "settings", "pvp"]:
			app.show_screen(s, {})
			for i in 45: await get_tree().process_frame
			await snap(s)
		var mm = app.get_current()
		app.show_screen("menu", {})
		for i in 4: await get_tree().process_frame
		app.get_current().open_missions()
		for i in 30: await get_tree().process_frame
		await snap("menu_missions")
		app.show_screen("menu", {"tab": 2})
		for i in 45: await get_tree().process_frame
		await snap("menu_club")
	if "patch" in which:
		var ex: String = app.debug_install_example()
		app.debug_activate_patch(ex)
		app.show_screen("menu", {})
		for i in 8: await get_tree().process_frame
		await snap("patch_menu")
		app.show_screen("league", {})
		for i in 8: await get_tree().process_frame
		await snap("patch_league")
		app.show_screen("patches", {})
		for i in 8: await get_tree().process_frame
		await snap("patch_list")
	if "match" in which:
		app.show_screen("match", {"mode": "quick", "minutes": 4, "seed": 3})
		for i in 30: await get_tree().process_frame
		await snap("match_kickoff")
		var m = app.get_current()
		m.toggle_auto()
		for k in 4:
			for i in 150: await get_tree().process_frame
			await snap("match_%d" % k)
	if "goal" in which:
		app.show_screen("match", {"mode": "quick", "minutes": 4, "seed": 5})
		for i in 200: await get_tree().process_frame
		var gm = app.get_current()
		gm.debug_goal(0)
		for i in 50: await get_tree().process_frame
		await snap("goal_banner")
		for i in 60: await get_tree().process_frame
		await snap("goal_celebrate")
		for i in 110: await get_tree().process_frame
		await snap("goal_replay_a")
		for i in 90: await get_tree().process_frame
		await snap("goal_replay_b")
		for i in 200: await get_tree().process_frame
		await snap("goal_after")
	if "result" in which:
		app.show_screen("match", {"mode": "quick", "minutes": 4, "seed": 6})
		for i in 200: await get_tree().process_frame
		var rm = app.get_current()
		rm.debug_goal(0)
		for i in 30: await get_tree().process_frame
		rm.force_fulltime()
		for i in 200: await get_tree().process_frame
		await snap("result")
	get_tree().quit()

func _buttons(n: Node) -> Array:
	var out_a := []
	if n is Button: out_a.append(n)
	for c in n.get_children(): out_a += _buttons(c)
	return out_a
