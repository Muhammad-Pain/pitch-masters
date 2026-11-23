extends Node
## Opens every screen, exercises packs/squad/league, plays a fuzzed human match.
var app

func _ready() -> void:
	app = PMApp.new()
	add_child(app)
	for i in 3: await get_tree().process_frame
	for s in ["menu", "squad", "packs", "league", "pvp", "settings", "patches", "title", "menu"]:
		app.show_screen(s, {})
		for i in 3: await get_tree().process_frame
		print("screen ok: ", s, " -> ", app.get_current().get_class())
	print("info: ", app.debug_info().coins, " coins, league ", app.debug_info().league_name)
	# human match with random inputs
	app.show_screen("match", {"mode": "quick", "minutes": 2, "seed": 5, "difficulty": 2, "user_home": false})
	await get_tree().process_frame
	var m = app.get_current()
	var t := 0.0
	var acts := ["pass", "through", "lob", "shoot", "pass"]
	while app.get_current() == m and is_instance_valid(m):
		await get_tree().physics_frame
		if not is_instance_valid(m) or app.get_current() != m: break
		t += 1.0 / 60.0
		var lt: int = m.get_local_team()
		var c: int = m.get_ctrl(lt)
		if c >= 0:
			var goal := Vector3(52.5 * (1 if lt == 0 else -1), 0, 0)
			var p: Vector3 = m.get_player_pos(c)
			var to: Vector3 = (goal - p) if m.local_has_ball() else (m.get_ball_pos() - p)
			var side := 1.0 if lt == 0 else -1.0
			m.hud_joy(Vector2(to.x, -to.z).normalized() * side)
		m.hud_hold("sprint", fmod(t, 3.0) < 1.0)
		m.hud_hold("through", not m.local_has_ball() and fmod(t, 5.0) < 1.5)
		if randf() < 0.015:
			var a: String = acts[randi() % acts.size()]
			m.hud_press(a)
			if a == "shoot":
				for i in randi_range(5, 50): await get_tree().physics_frame
				if not is_instance_valid(m) or app.get_current() != m: break
				m.hud_release("shoot")
		if randf() < 0.001: m.toggle_auto()
		if int(t * 60) % 1800 == 0:
			print("t=%.0f clock=%.0f state=%d score=%s" % [t, m.get_clock(), m.get_state(), str(m.get_score())])
	print("after match screen: ", app.get_current().get_class(), " info=", app.debug_info().coins)
	app.show_screen("menu", {})
	await get_tree().process_frame
	print("UI TEST DONE")
	get_tree().quit()
