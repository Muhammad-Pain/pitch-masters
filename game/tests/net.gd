extends Node
## Two-process PvP test: "host" and "client". Run both in real time (no --fixed-fps):
## godot --headless res://tests/net.tscn -- host  &  godot --headless res://tests/net.tscn -- client
var app
var snaps := 0

func _ready() -> void:
	var role: String = OS.get_cmdline_user_args()[0] if OS.get_cmdline_user_args().size() > 0 else "host"
	app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	var net = app.get_node("Net")
	if role == "host":
		app.show_screen("pvp", {})
		await get_tree().create_timer(0.3).timeout
		# use the lobby UI: press "Создать матч"
		var pvp = app.get_current()
		_press_button(pvp, "Создать матч")
		var t := 0.0
		while t < 15.0:
			await get_tree().create_timer(0.25).timeout
			t += 0.25
			if _press_button(app.get_current(), "Начать матч", true): break
		await get_tree().create_timer(1.0).timeout
		var m = app.get_current()
		print("host screen: ", m.get_class())
		await get_tree().create_timer(4.0).timeout
		print("host: remote move=", m.get_in_move(1), " autoplay1=", m.get_autoplay(1), " clock=", snappedf(m.get_clock(), 0.1))
		m.force_fulltime()
		await get_tree().create_timer(4.0).timeout
		print("host final: ", app.get_current().get_class())
		get_tree().quit()
	else:
		await get_tree().create_timer(0.8).timeout
		app.show_screen("pvp", {})
		await get_tree().process_frame
		_press_button(app.get_current(), "Подключиться")
		await get_tree().process_frame
		var le = _find(app.get_current(), "LineEdit")
		le.text = "127.0.0.1"
		_press_button(app.get_current(), "Подключиться", true)
		net.snapshot_received.connect(func(_d): snaps += 1)
		var t := 0.0
		while app.get_current().get_class() != "PMMatch" and t < 20.0:
			await get_tree().create_timer(0.2).timeout
			t += 0.2
		var m = app.get_current()
		print("client in match, local_team=", m.get_local_team())
		m.hud_joy(Vector2(1, 0))
		await get_tree().create_timer(1.5).timeout
		m.toggle_auto()
		await get_tree().create_timer(1.5).timeout
		print("client snaps=", snaps, " clock=", snappedf(m.get_clock(), 0.1), " state=", m.get_state())
		while app.get_current().get_class() != "PMResultScreen" and t < 40.0:
			await get_tree().create_timer(0.2).timeout
			t += 0.2
		print("client final: ", app.get_current().get_class())
		get_tree().quit()

func _find(n: Node, cls: String) -> Node:
	if n.get_class() == cls: return n
	for c in n.get_children():
		var r = _find(c, cls)
		if r: return r
	return null

func _press_button(n: Node, text_start: String, enabled_only := false) -> bool:
	if n is Button and n.text.begins_with(text_start) and (not enabled_only or not n.disabled):
		n.pressed.emit()
		return true
	for c in n.get_children():
		if _press_button(c, text_start, enabled_only): return true
	return false
